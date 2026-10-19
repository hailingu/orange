// ADR: docs/adr/ADR-0003-local-runtime-path.md
// The single-threaded poll() event loop: readiness-driven read/write
// state machines per connection over one loopback listener, with the
// declared idle timeout, close-after-rejection lifecycle, and a bounded
// read-pause backpressure. Sockets are owned by RAII descriptors and
// never escape this file.
#include "http_server.hpp"
#include "http_syntax.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace orange::http {
namespace {

// Stop-flag latency and idle-sweep granularity: poll never waits longer.
constexpr int poll_slice_ms = 250;
// Reading pauses while a connection holds this much undelivered output
// plus unparsed input, so a stalling peer cannot grow memory unboundedly.
constexpr std::size_t read_pause_cap = 1024 * 1024;
// Listen backlog; loopback single-user traffic (ADR-0003 connection model).
constexpr int listen_backlog = 16;
// Rejected or failing responses fall back to this precomputed wire form
// when even the serializer cannot allocate.
constexpr std::string_view internal_error_wire =
    "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n";

// True for the two errno values that mean "try again later".
bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK; }

// Move-only owning descriptor; destruction closes, invalid value is -1.
class OwnedFd {
  public:
    OwnedFd() noexcept = default;
    explicit OwnedFd(int fd) noexcept : fd_{fd} {}
    ~OwnedFd() { reset(); }
    OwnedFd(OwnedFd &&other) noexcept : fd_{std::exchange(other.fd_, -1)} {}
    OwnedFd &operator=(OwnedFd &&other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    OwnedFd(const OwnedFd &) = delete;
    OwnedFd &operator=(const OwnedFd &) = delete;
    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    int release() noexcept { return std::exchange(fd_, -1); }
    void reset() noexcept {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = -1;
    }

  private:
    int fd_ = -1;
};

// Sets O_NONBLOCK; returns the errno of the failing fcntl.
int set_non_blocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return errno;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ? errno : 0;
}

// Darwin has no MSG_NOSIGNAL; a per-socket SO_NOSIGPIPE keeps a peer that
// closed early from killing the process with SIGPIPE.
int set_no_sigpipe(int fd) {
    const int on = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) < 0 ? errno : 0;
}

// One served connection: its descriptor, decoding progress, buffered
// input, queued output, and the lifecycle flags the loop transitions.
// The decoder is held by pointer because its type is deliberately neither
// copyable nor movable (service ADR-0004 contract), while connections
// relocate when the vector grows or compacts.
struct Connection {
    OwnedFd fd;
    std::unique_ptr<RequestDecoder> decoder;
    std::string incoming;
    std::string outgoing;
    std::size_t outgoing_offset = 0;
    bool closing_after_flush = false;
    bool peer_eof = false;
    std::chrono::steady_clock::time_point last_activity{};
};

// True when a request's Connection field carries the close option
// (RFC 9110 7.6.1: comma-separated, case-insensitive option list).
bool requests_close(const Request &request) {
    const auto trim_ows = [](std::string_view value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
            value.remove_suffix(1);
        return value;
    };
    for (HeaderField field : request.headers()) {
        if (!detail::iequals(field.name, "connection"))
            continue;
        std::size_t begin = 0;
        while (begin <= field.value.size()) {
            const std::size_t comma = field.value.find(',', begin);
            const std::string_view option = trim_ows(field.value.substr(
                begin, comma == std::string_view::npos ? std::string_view::npos : comma - begin));
            if (detail::iequals(option, "close"))
                return true;
            if (comma == std::string_view::npos)
                break;
            begin = comma + 1;
        }
    }
    return false;
}

// Appends the serialized response to the outgoing backlog, or the
// precomputed 500 closing wire when serialization cannot proceed.
void queue_response(Connection &connection, const Response &response) {
    auto wire = write_response(response);
    if (const auto *bytes = std::get_if<std::string>(&wire)) {
        connection.outgoing += *bytes;
        if (response.close)
            connection.closing_after_flush = true;
        return;
    }
    connection.outgoing += internal_error_wire;
    connection.closing_after_flush = true;
}

// Serves one complete message: the handler's answer, honoring a request
// Connection: close, with a 500 closing response when the handler fails.
void append_response(Connection &connection, const ParsedMessage &message,
                     const RequestHandler &handler) {
    Response response;
    try {
        response = handler(message);
    } catch (...) {
        response = error_response(ParseError::resource_exhausted);
    }
    response.close = response.close || requests_close(message.request);
    queue_response(connection, response);
}

// Drives the decoder over buffered input: each complete message becomes
// a queued response and the decoder resets for the next; a terminal
// ParseError queues the closing rejection; monostate waits for bytes.
void drive_messages(Connection &connection, const RequestHandler &handler) {
    while (!connection.closing_after_flush) {
        const DecodeResult result = connection.decoder->feed(connection.incoming);
        connection.incoming.erase(0, result.consumed);
        if (auto *message = std::get_if<ParsedMessage>(&result.outcome)) {
            append_response(connection, *message, handler);
            connection.decoder->reset();
            continue;
        }
        if (auto *error = std::get_if<ParseError>(&result.outcome)) {
            queue_response(connection, error_response(*error));
            connection.closing_after_flush = true;
        }
        return;
    }
}

// Reads available bytes and drives the decoder over everything buffered.
void read_connection(Connection &connection, const RequestHandler &handler) {
    char buffer[16384];
    for (;;) {
        const auto received = ::recv(connection.fd.get(), buffer, sizeof(buffer), 0);
        if (received > 0) {
            connection.incoming.append(buffer, static_cast<std::size_t>(received));
            connection.last_activity = std::chrono::steady_clock::now();
            drive_messages(connection, handler);
            if (connection.closing_after_flush ||
                static_cast<std::size_t>(received) < sizeof(buffer))
                return;
            continue;
        }
        if (received == 0) {
            connection.peer_eof = true;
            return;
        }
        if (errno == EINTR)
            continue;
        if (!would_block())
            connection.fd.reset();
        return;
    }
}

// Delivers pending bytes; a fully flushed closing response closes.
void write_connection(Connection &connection) {
    while (connection.outgoing_offset < connection.outgoing.size()) {
        const char *data = connection.outgoing.data() + connection.outgoing_offset;
        const std::size_t left = connection.outgoing.size() - connection.outgoing_offset;
        const auto sent = ::send(connection.fd.get(), data, left, 0);
        if (sent > 0) {
            connection.outgoing_offset += static_cast<std::size_t>(sent);
            connection.last_activity = std::chrono::steady_clock::now();
            continue;
        }
        if (sent < 0 && errno == EINTR)
            continue;
        if (sent < 0 && !would_block())
            connection.fd.reset();
        return;
    }
    connection.outgoing.clear();
    connection.outgoing_offset = 0;
    if (connection.closing_after_flush)
        connection.fd.reset();
}

// One readiness event's worth of connection work; a bad_alloc drops only
// this connection instead of the whole loop.
void service_connection(short events, Connection &connection, const RequestHandler &handler) {
    try {
        if (events & (POLLNVAL | POLLERR)) {
            connection.fd.reset();
            return;
        }
        if (events & POLLIN)
            read_connection(connection, handler);
        if (connection.fd.valid() && (events & POLLOUT))
            write_connection(connection);
        if (connection.fd.valid() && connection.peer_eof &&
            connection.outgoing_offset == connection.outgoing.size())
            connection.fd.reset();
    } catch (const std::bad_alloc &) {
        connection.fd.reset();
    }
}

// Poll budget: the soonest idle deadline, capped so stop stays responsive.
int next_wake_ms(std::chrono::steady_clock::time_point now,
                 const std::vector<Connection> &connections,
                 std::chrono::milliseconds idle_timeout) {
    auto deadline = now + std::chrono::milliseconds(poll_slice_ms);
    for (const Connection &connection : connections)
        deadline = std::min(deadline, connection.last_activity + idle_timeout);
    const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    return static_cast<int>(std::clamp(wait.count(), 0LL, static_cast<long long>(poll_slice_ms)));
}

// Rebuilds the poll set: the listener, then one entry per connection.
// Reading is off while closing, after peer EOF, or under backpressure.
void build_poll_fds(std::vector<pollfd> &poll_fds, int listen_fd,
                    const std::vector<Connection> &connections) {
    poll_fds.clear();
    poll_fds.push_back(pollfd{listen_fd, POLLIN, 0});
    for (const Connection &connection : connections) {
        short events = 0;
        if (!connection.closing_after_flush && !connection.peer_eof &&
            connection.incoming.size() + connection.outgoing.size() - connection.outgoing_offset <
                read_pause_cap)
            events |= POLLIN;
        if (connection.outgoing_offset < connection.outgoing.size())
            events |= POLLOUT;
        poll_fds.push_back(pollfd{connection.fd.get(), events, 0});
    }
}

// Accepts every pending connection from the non-blocking listener.
void accept_incoming(int listen_fd, std::vector<Connection> &connections,
                     std::chrono::steady_clock::time_point now) {
    for (;;) {
        const int fd = ::accept(listen_fd, nullptr, nullptr);
        if (fd < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (set_non_blocking(fd) != 0 || set_no_sigpipe(fd) != 0) {
            ::close(fd);
            continue;
        }
        Connection &connection = connections.emplace_back();
        connection.fd = OwnedFd(fd);
        connection.decoder = std::make_unique<RequestDecoder>();
        connection.last_activity = now;
    }
}

} // namespace

LoopbackServer::~LoopbackServer() {
    if (listen_fd_ >= 0)
        ::close(listen_fd_);
}

int LoopbackServer::bind(std::uint16_t port, std::uint16_t *bound_port) {
    OwnedFd fd{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!fd.valid())
        return errno;
    if (const int error = set_non_blocking(fd.get()); error != 0)
        return error;
    const int reuse = 1;
    if (::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        return errno;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(fd.get(), reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0)
        return errno;
    if (::listen(fd.get(), listen_backlog) < 0)
        return errno;
    if (bound_port != nullptr) {
        sockaddr_in bound{};
        socklen_t size = sizeof(bound);
        if (::getsockname(fd.get(), reinterpret_cast<sockaddr *>(&bound), &size) < 0)
            return errno;
        *bound_port = ntohs(bound.sin_port);
    }
    listen_fd_ = fd.release();
    return 0;
}

int LoopbackServer::serve(RequestHandler handler, std::chrono::milliseconds idle_timeout,
                          const std::atomic_bool &stop) {
    if (listen_fd_ < 0 || !handler)
        return EINVAL;
    std::vector<Connection> connections;
    std::vector<pollfd> poll_fds;
    while (!stop.load(std::memory_order_relaxed)) {
        const auto now = std::chrono::steady_clock::now();
        build_poll_fds(poll_fds, listen_fd_, connections);
        const int ready = ::poll(poll_fds.data(), static_cast<nfds_t>(poll_fds.size()),
                                 next_wake_ms(now, connections, idle_timeout));
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return errno;
        }
        const auto progressed = std::chrono::steady_clock::now();
        const std::size_t polled = connections.size();
        if (const short listener_events = poll_fds.front().revents;
            listener_events & (POLLERR | POLLNVAL))
            return EINVAL;
        else if (listener_events & POLLIN)
            accept_incoming(listen_fd_, connections, progressed);
        // Connections accepted above have no pollfd entry yet; they get
        // their first readiness event from the next iteration.
        for (std::size_t i = 0; i < polled; ++i)
            service_connection(poll_fds[i + 1].revents, connections[i], handler);
        for (Connection &connection : connections)
            if (connection.fd.valid() && progressed - connection.last_activity >= idle_timeout)
                connection.fd.reset();
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                           [](const Connection &connection) { return !connection.fd.valid(); }),
            connections.end());
    }
    return 0;
}

int serve_loopback(std::uint16_t port, RequestHandler handler,
                   std::chrono::milliseconds idle_timeout, const std::atomic_bool &stop,
                   std::uint16_t *bound_port) {
    LoopbackServer server;
    if (const int error = server.bind(port, bound_port); error != 0)
        return error;
    return server.serve(std::move(handler), idle_timeout, stop);
}

} // namespace orange::http
