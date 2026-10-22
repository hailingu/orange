// ADR: docs/adr/ADR-0003-local-runtime-path.md
// The single-threaded kqueue() event loop: readiness-driven read/write
// state machines per connection over one loopback listener, with the
// declared idle timeout, close-after-rejection lifecycle, and a bounded
// read-pause backpressure. Sockets are owned by RAII descriptors and
// never escape this file; kernel interest lives in one kqueue descriptor
// and is maintained incrementally through add/enable/disable changes
// instead of being rebuilt every round like a poll set.
#include "http_server.hpp"
#include "http_syntax.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <string_view>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace orange::http {
namespace {

// Stop-flag latency and idle-sweep granularity: kevent never waits longer.
constexpr int wake_slice_ms = 250;
// Upper bound on events harvested per wakeup. Level-triggered filters
// keep anything beyond this pending for the next kevent, so nothing is
// lost — the remainder just arrives one round later.
constexpr int event_capacity = 64;
// Reading pauses while a connection holds this much undelivered output
// plus unparsed input, so a stalling peer cannot grow memory unboundedly.
constexpr std::size_t read_pause_cap = 1024 * 1024;
// Listen backlog; loopback single-user traffic (ADR-0003 connection model).
constexpr int listen_backlog = 16;

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

// Nagle's algorithm would hold a small segment until the peer
// acknowledges the previous one; with the peer's delayed ACK that
// stalls a keep-alive exchange for a full ACK timer, so responses
// leave immediately instead.
int set_no_delay(int fd) {
    const int on = 1;
    return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0 ? errno : 0;
}

// Per-filter kernel-interest state, tracked so the changelist emits only
// transitions: absent (never registered), enabled (kernel reports
// readiness), or disabled (registered but silent — backpressure and
// termination phases mute a filter instead of deleting its knote).
enum class Interest { absent, enabled, disabled };

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
    Interest read_interest = Interest::absent;
    Interest write_interest = Interest::absent;
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

// Serializes the response straight into the outgoing backlog, falling back
// to the precomputed 500 closing wire when validation or allocation fails.
void queue_response(Connection &connection, const Response &response) {
    if (write_response_into(response, connection.outgoing)) {
        connection.outgoing += error_wire(ParseError::resource_exhausted);
        connection.closing_after_flush = true;
        return;
    }
    if (response.close)
        connection.closing_after_flush = true;
}

// Appends the precomputed closing wire for one terminal parse error. The
// static wire never allocates, so the rejection path stays infallible.
void queue_rejection(Connection &connection, ParseError error) {
    connection.outgoing += error_wire(error);
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
        queue_rejection(connection, ParseError::resource_exhausted);
        return;
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
            queue_rejection(connection, *error);
            return;
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

// Maps a returned event's descriptor to its connection. The kernel
// identifies knotes by descriptor while connections relocate inside their
// vector, so lookup is by descriptor value; loopback's small connection
// count keeps the scan negligible. A miss means the connection died
// earlier in this batch (its descriptor is already invalid).
Connection *find_connection(std::vector<Connection> &connections, int fd) {
    for (Connection &connection : connections)
        if (connection.fd.valid() && connection.fd.get() == fd)
            return &connection;
    return nullptr;
}

// Wake budget: the soonest idle deadline, capped so stop stays responsive.
int next_wake_ms(std::chrono::steady_clock::time_point now,
                 const std::vector<Connection> &connections,
                 std::chrono::milliseconds idle_timeout) {
    auto deadline = now + std::chrono::milliseconds(wake_slice_ms);
    for (const Connection &connection : connections)
        deadline = std::min(deadline, connection.last_activity + idle_timeout);
    const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    return static_cast<int>(std::clamp(wait.count(), 0LL, static_cast<long long>(wake_slice_ms)));
}

// Emits the change that moves one filter toward want_enabled, updating
// the tracked state. EV_SET writes through its pointer argument field by
// field, so it must never receive a fresh temporary such as the address
// of an emplace_back() call — push first, then fill the back element.
void set_interest(std::vector<struct kevent> &changes, int fd, short filter, Interest &state,
                  bool want_enabled) {
    if (state == Interest::absent) {
        if (!want_enabled)
            return; // never registered: nothing to arm or silence
        changes.push_back({});
        EV_SET(&changes.back(), fd, filter, EV_ADD, 0, 0, nullptr);
        state = Interest::enabled;
        return;
    }
    if (want_enabled == (state == Interest::enabled))
        return;
    changes.push_back({});
    EV_SET(&changes.back(), fd, filter, want_enabled ? EV_ENABLE : EV_DISABLE, 0, 0, nullptr);
    state = want_enabled ? Interest::enabled : Interest::disabled;
}

// Rebuilds this round's changelist from connection state. The listener is
// registered once before the loop, so only per-connection transitions
// appear here. Reading is disabled while closing, after peer EOF, or under
// backpressure; the write filter exists only while output is pending —
// under level-triggered semantics a permanently ready write filter would
// spin the loop, exactly the trap the poll predecessor's interest mask
// avoided by never registering POLLOUT unconditionally.
void build_interest_changes(std::vector<struct kevent> &changes,
                            std::vector<Connection> &connections) {
    changes.clear();
    for (Connection &connection : connections) {
        if (!connection.fd.valid())
            continue;
        const bool want_read =
            !connection.closing_after_flush && !connection.peer_eof &&
            connection.incoming.size() + connection.outgoing.size() - connection.outgoing_offset <
                read_pause_cap;
        set_interest(changes, connection.fd.get(), EVFILT_READ, connection.read_interest,
                     want_read);
        const bool want_write = connection.outgoing_offset < connection.outgoing.size();
        set_interest(changes, connection.fd.get(), EVFILT_WRITE, connection.write_interest,
                     want_write);
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
        if (set_non_blocking(fd) != 0 || set_no_sigpipe(fd) != 0 || set_no_delay(fd) != 0) {
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
    OwnedFd kq{::kqueue()};
    if (!kq.valid())
        return errno;
    // The listener's read filter reports pending handshakes; it is
    // registered once before the loop and stays enabled for its lifetime.
    struct kevent listener_change{};
    EV_SET(&listener_change, listen_fd_, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (::kevent(kq.get(), &listener_change, 1, nullptr, 0, nullptr) < 0)
        return errno;
    std::vector<Connection> connections;
    std::vector<struct kevent> changes;
    std::array<struct kevent, event_capacity> events{};
    while (!stop.load(std::memory_order_relaxed)) {
        const auto now = std::chrono::steady_clock::now();
        build_interest_changes(changes, connections);
        const int slice = next_wake_ms(now, connections, idle_timeout);
        const timespec wake{slice / 1000, (slice % 1000) * 1000000};
        const int ready = ::kevent(kq.get(), changes.data(), static_cast<int>(changes.size()),
                                   events.data(), static_cast<int>(events.size()), &wake);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return errno;
        }
        const auto progressed = std::chrono::steady_clock::now();
        // Readiness pass. A rejected changelist entry returns as an event
        // with EV_ERROR and the errno in data: a failing listener ends the
        // loop, a failing connection loses only itself. Connections
        // accepted below have no registered filters yet, so their first
        // event arrives from the next round — the polled-set invariant the
        // poll predecessor enforced by freezing the vector size.
        for (int i = 0; i < ready; ++i) {
            const struct kevent &event = events[static_cast<std::size_t>(i)];
            const int fd = static_cast<int>(event.ident);
            if (event.flags & EV_ERROR) {
                if (fd == listen_fd_)
                    return EINVAL;
                if (Connection *connection = find_connection(connections, fd))
                    connection->fd.reset();
                continue;
            }
            if (fd == listen_fd_) {
                accept_incoming(listen_fd_, connections, progressed);
                continue;
            }
            if (event.filter != EVFILT_READ)
                continue;
            Connection *connection = find_connection(connections, fd);
            if (connection == nullptr)
                continue;
            try {
                read_connection(*connection, handler);
                if (connection->fd.valid() && connection->peer_eof &&
                    connection->outgoing_offset == connection->outgoing.size())
                    connection->fd.reset();
            } catch (const std::bad_alloc &) {
                connection->fd.reset();
            }
        }
        // Write pass after the read pass, mirroring the predecessor's
        // per-connection ordering: a read above may queue the very bytes
        // this pass flushes, and a connection closed above is skipped by
        // the lookup because its descriptor is already invalid.
        for (int i = 0; i < ready; ++i) {
            const struct kevent &event = events[static_cast<std::size_t>(i)];
            if (event.filter != EVFILT_WRITE)
                continue;
            Connection *connection = find_connection(connections, static_cast<int>(event.ident));
            if (connection == nullptr)
                continue;
            try {
                write_connection(*connection);
                if (connection->fd.valid() && connection->peer_eof &&
                    connection->outgoing_offset == connection->outgoing.size())
                    connection->fd.reset();
            } catch (const std::bad_alloc &) {
                connection->fd.reset();
            }
        }
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
