// ADR: docs/adr/ADR-0003-local-runtime-path.md
// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// The kqueue() acceptor loops: readiness-driven read/write state machines
// per connection over loopback listeners, with the declared idle timeout,
// close-after-rejection lifecycle, and a bounded read-pause backpressure.
// One acceptor thread is the historical single loop; more threads form a
// shared-nothing SO_REUSEPORT listener group with one kqueue and connection
// store per listener and no state crossing loops. Sockets are owned by RAII
// descriptors and never escape this file; kernel interest is maintained
// incrementally through add/enable/disable changes instead of being rebuilt
// every round like a poll set.
#include "http_server.hpp"
#include "http_connection_store.hpp"
#include "http_syntax.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <string_view>
#include <sys/event.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace orange::http {
namespace {
using detail::Connection;
using detail::ConnectionStore;
using detail::Interest;
using detail::OwnedFd;

// Stop-flag latency and idle-sweep granularity: kevent never waits longer.
constexpr int wake_slice_ms = 250;
// Upper bound on events harvested per wakeup. Level-triggered filters
// keep anything beyond this pending for the next kevent, so nothing is
// lost — the remainder just arrives one round later.
constexpr int event_capacity = 64;
// Reading pauses while a connection holds this much undelivered output
// while receive windows are synchronously consumed by the bounded decoder.
constexpr std::size_t read_pause_cap = 1024 * 1024;
// One receive drain's slice: each recv() fills at most this many bytes, so
// large coalesced bodies need fewer syscalls while stack use stays bounded.
constexpr std::size_t receive_window = 64 * 1024;
// Listen backlog; loopback single-user traffic (ADR-0003 connection model).
constexpr int listen_backlog = 16;

// True for the two errno values that mean "try again later".
bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK; }

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

// Serves one complete message: the handler's answer, honoring the request's
// decoded Connection: close option, with a 500 closing response when the
// handler fails.
void append_response(Connection &connection, const ParsedMessage &message,
                     const RequestHandler &handler) {
    Response response;
    try {
        response = handler(message);
    } catch (...) {
        queue_rejection(connection, ParseError::resource_exhausted);
        return;
    }
    response.close = response.close || message.close;
    queue_response(connection, response);
}

// KS-2: the decoder owns incomplete fragments. Process every pipeline suffix
// in this receive window synchronously, before the borrowed stack bytes expire.
void drive_messages(Connection &connection, std::string_view input, const RequestHandler &handler) {
    while (!input.empty() && !connection.closing_after_flush) {
        const DecodeResult result = connection.decoder.feed(input);
        input.remove_prefix(result.consumed);
        if (auto *message = std::get_if<ParsedMessage>(&result.outcome)) {
            append_response(connection, *message, handler);
            connection.decoder.reset();
            continue;
        }
        if (auto *error = std::get_if<ParseError>(&result.outcome)) {
            queue_rejection(connection, *error);
            return;
        }
        return;
    }
}

// Feeds received bytes directly; no connection input staging or prefix erase.
// Samples the activity clock once per drain rather than on every window slice.
void read_connection(Connection &connection, const RequestHandler &handler) {
    char buffer[receive_window];
    bool progressed = false;
    for (;;) {
        const auto received = ::recv(connection.fd.get(), buffer, sizeof(buffer), 0);
        if (received > 0) {
            progressed = true;
            drive_messages(connection, {buffer, static_cast<std::size_t>(received)}, handler);
            if (connection.closing_after_flush ||
                static_cast<std::size_t>(received) < sizeof(buffer))
                break;
            continue;
        }
        if (received == 0) {
            connection.peer_eof = true;
            break;
        }
        if (errno == EINTR)
            continue;
        if (!would_block())
            connection.fd.reset();
        break;
    }
    if (progressed && connection.fd.valid())
        connection.last_activity = std::chrono::steady_clock::now();
}

// Delivers pending bytes; a fully flushed closing response closes.
void write_connection(Connection &connection) {
    bool progressed = false;
    while (connection.outgoing_offset < connection.outgoing.size()) {
        const char *data = connection.outgoing.data() + connection.outgoing_offset;
        const std::size_t left = connection.outgoing.size() - connection.outgoing_offset;
        const auto sent = ::send(connection.fd.get(), data, left, 0);
        if (sent > 0) {
            connection.outgoing_offset += static_cast<std::size_t>(sent);
            progressed = true;
            continue;
        }
        if (sent < 0 && errno == EINTR)
            continue;
        if (sent < 0 && !would_block())
            connection.fd.reset();
        if (progressed && connection.fd.valid())
            connection.last_activity = std::chrono::steady_clock::now();
        return;
    }
    if (progressed)
        connection.last_activity = std::chrono::steady_clock::now();
    connection.outgoing.clear();
    connection.outgoing_offset = 0;
    if (connection.closing_after_flush)
        connection.fd.reset();
}

// The kernel interest set one live connection should hold: reading is
// disabled while closing, after peer EOF, or under backpressure; the write
// filter exists only while output is pending — under level-triggered
// semantics a permanently ready write filter would spin the loop, exactly
// the trap the poll predecessor's interest mask avoided by never
// registering POLLOUT unconditionally.
struct WantedInterest {
    bool read;
    bool write;
};
WantedInterest wanted_interest(const Connection &connection) {
    return {!connection.closing_after_flush && !connection.peer_eof &&
                connection.outgoing.size() - connection.outgoing_offset < read_pause_cap,
            connection.outgoing_offset < connection.outgoing.size()};
}

// True when the tracked kernel state differs from the wanted state — an
// absent filter still needs its add, a registered filter needs its enabled
// state flipped — so set_interest would emit a changelist entry.
bool interest_transition_pending(Interest state, bool want_enabled) {
    return state == Interest::absent ? want_enabled : want_enabled != (state == Interest::enabled);
}

// Runs one readiness action with connection-local allocation failure handling.
// KS-1: try a nonblocking write immediately after a read queues its response;
// the kernel write filter is needed only for bytes that remain afterward.
void service_connection(ConnectionStore &connections, Connection &connection,
                        const RequestHandler &handler, bool readable) {
    const auto previous_activity = connection.last_activity;
    try {
        if (readable)
            read_connection(connection, handler);
        if (connection.fd.valid() && connection.outgoing_offset < connection.outgoing.size())
            write_connection(connection);
        if (connection.fd.valid() && connection.peer_eof &&
            connection.outgoing_offset == connection.outgoing.size())
            connection.fd.reset();
    } catch (const std::bad_alloc &) {
        connection.fd.reset();
    }
    if (!connection.fd.valid()) {
        connections.retire(connection);
        return;
    }
    if (connection.last_activity != previous_activity)
        connections.touch(connection, connection.last_activity);
    // Queue an interest update only when servicing actually changed the set
    // the kernel should hold; a steady-state exchange would otherwise make a
    // dirty round-trip whose changelist entries are all no-ops.
    const auto want = wanted_interest(connection);
    if (interest_transition_pending(connection.read_interest, want.read) ||
        interest_transition_pending(connection.write_interest, want.write))
        connections.mark_dirty(connection);
}

// KS-5/KS-6: dispatch in constant time using the kernel's unchanged udata.
// A prior event may have closed this connection, but its object remains alive
// until the whole batch ends. A reused descriptor therefore cannot revive it.
Connection *event_connection(const struct kevent &event) {
    auto *connection = static_cast<Connection *>(event.udata);
    return connection != nullptr && connection->fd.get() == static_cast<int>(event.ident)
               ? connection
               : nullptr;
}

// Wake budget: the soonest idle deadline, capped so stop stays responsive.
int next_wake_ms(std::chrono::steady_clock::time_point now, const ConnectionStore &connections,
                 std::chrono::milliseconds idle_timeout) {
    auto deadline = now + std::chrono::milliseconds(wake_slice_ms);
    deadline = std::min(deadline, connections.next_deadline(idle_timeout));
    const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    return static_cast<int>(std::clamp(wait.count(), 0LL, static_cast<long long>(wake_slice_ms)));
}

// Emits the change that moves one filter toward want_enabled, updating
// the tracked state. EV_SET writes through its pointer argument field by
// field, so it must never receive a fresh temporary such as the address
// of an emplace_back() call — push first, then fill the back element.
void set_interest(std::vector<struct kevent> &changes, Connection &connection, short filter,
                  Interest &state, bool want_enabled) {
    if (!interest_transition_pending(state, want_enabled))
        return; // no registration to add and no enabled state to flip
    changes.push_back({});
    if (state == Interest::absent) {
        EV_SET(&changes.back(), connection.fd.get(), filter, EV_ADD, 0, 0, &connection);
        state = Interest::enabled;
        return;
    }
    EV_SET(&changes.back(), connection.fd.get(), filter, want_enabled ? EV_ENABLE : EV_DISABLE, 0,
           0, &connection);
    state = want_enabled ? Interest::enabled : Interest::disabled;
}

// Builds this round's changelist from dirty connections. The listener is
// registered once before the loop, so only per-connection transitions
// appear here.
void build_interest_changes(std::vector<struct kevent> &changes, ConnectionStore &connections) {
    changes.clear();
    while (Connection *pending = connections.take_dirty()) {
        Connection &connection = *pending;
        if (!connection.fd.valid())
            continue;
        const auto want = wanted_interest(connection);
        set_interest(changes, connection, EVFILT_READ, connection.read_interest, want.read);
        set_interest(changes, connection, EVFILT_WRITE, connection.write_interest, want.write);
    }
}

// Accepts every pending connection from the non-blocking listener.
void accept_incoming(int listen_fd, ConnectionStore &connections) {
    for (;;) {
        OwnedFd fd{::accept(listen_fd, nullptr, nullptr)};
        if (!fd.valid()) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (set_non_blocking(fd.get()) != 0 || set_no_sigpipe(fd.get()) != 0 ||
            set_no_delay(fd.get()) != 0)
            continue;
        try {
            connections.add(std::move(fd), std::chrono::steady_clock::now());
        } catch (const std::bad_alloc &) {
            return; // RAII closes this unregistered accept; existing owners survive.
        }
    }
}

// One shared-nothing acceptor loop: this listener, this kqueue, this
// connection store. A loop that fails unrecoverably raises abort so sibling
// loops stop promptly instead of outliving a broken listener group.
int run_acceptor(int listener_fd, const RequestHandler &handler,
                 std::chrono::milliseconds idle_timeout, const std::atomic_bool &stop,
                 std::atomic_bool &abort) {
    OwnedFd kq{::kqueue()};
    if (!kq.valid()) {
        abort.store(true, std::memory_order_relaxed);
        return errno;
    }
    // The listener's read filter reports pending handshakes; it is
    // registered once before the loop and stays enabled for its lifetime.
    struct kevent listener_change{};
    EV_SET(&listener_change, listener_fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (::kevent(kq.get(), &listener_change, 1, nullptr, 0, nullptr) < 0) {
        abort.store(true, std::memory_order_relaxed);
        return errno;
    }
    ConnectionStore connections;
    std::vector<struct kevent> changes;
    std::array<struct kevent, event_capacity> events{};
    while (!stop.load(std::memory_order_relaxed) && !abort.load(std::memory_order_relaxed)) {
        const auto now = std::chrono::steady_clock::now();
        build_interest_changes(changes, connections);
        const int slice = next_wake_ms(now, connections, idle_timeout);
        const timespec wake{slice / 1000, (slice % 1000) * 1000000};
        const int ready = ::kevent(kq.get(), changes.data(), static_cast<int>(changes.size()),
                                   events.data(), static_cast<int>(events.size()), &wake);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            abort.store(true, std::memory_order_relaxed);
            return errno;
        }
        const auto progressed = std::chrono::steady_clock::now();
        // Readiness pass. A rejected changelist entry returns as an event
        // with EV_ERROR and the errno in data: a failing listener ends the
        // loop, a failing connection loses only itself. Connections
        // accepted below have no registered filters yet, so their first
        // event arrives from the next round — the polled-set invariant the
        // poll predecessor enforced by freezing the vector size.
        int first_write = ready;
        for (int i = 0; i < ready; ++i) {
            const struct kevent &event = events[static_cast<std::size_t>(i)];
            const int fd = static_cast<int>(event.ident);
            if (event.flags & EV_ERROR) {
                if (fd == listener_fd) {
                    abort.store(true, std::memory_order_relaxed);
                    return EINVAL;
                }
                if (Connection *connection = event_connection(event))
                    connections.retire(*connection);
                continue;
            }
            if (fd == listener_fd) {
                accept_incoming(listener_fd, connections);
                continue;
            }
            if (event.filter != EVFILT_READ) {
                if (event.filter == EVFILT_WRITE && first_write == ready)
                    first_write = i;
                continue;
            }
            Connection *connection = event_connection(event);
            if (connection == nullptr)
                continue;
            service_connection(connections, *connection, handler, true);
        }
        // Write pass after the read pass, mirroring the predecessor's
        // per-connection ordering: a read above may queue the very bytes
        // this pass flushes, and a connection closed above is skipped by
        // the identity guard because its descriptor is already invalid.
        // Skip the scan entirely in the common case where no write filter fired.
        for (int i = first_write; i < ready; ++i) {
            const struct kevent &event = events[static_cast<std::size_t>(i)];
            if (event.filter != EVFILT_WRITE || (event.flags & EV_ERROR))
                continue;
            Connection *connection = event_connection(event);
            if (connection == nullptr)
                continue;
            service_connection(connections, *connection, handler, false);
        }
        connections.expire(progressed, idle_timeout);
        // Returned udata may still name a retired node through both event passes.
        // Destroy those nodes only now; live nodes are never traversed to collect.
        connections.collect();
    }
    return 0;
}

// Creates one bound, listening, non-blocking loopback listener. A shared
// listener joins the port's SO_REUSEPORT group, over which the kernel spreads
// incoming connections; an unshared one is the historical lone listener.
// An invalid result stores the failing errno in error.
OwnedFd make_listener(const sockaddr_in &address, bool shared, int &error) {
    OwnedFd fd{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!fd.valid()) {
        error = errno;
        return fd;
    }
    const int reuse = 1;
    if (set_non_blocking(fd.get()) != 0 ||
        ::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        (shared && ::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) ||
        ::bind(fd.get(), reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0 ||
        ::listen(fd.get(), listen_backlog) < 0) {
        error = errno;
        fd.reset();
    }
    return fd;
}

} // namespace

LoopbackServer::~LoopbackServer() {
    for (const int fd : listeners_)
        ::close(fd);
}

int LoopbackServer::bind(std::uint16_t port, std::uint16_t *bound_port) {
    if (acceptor_threads_ < 1 || acceptor_threads_ > 64 || !listeners_.empty())
        return EINVAL;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const bool shared = acceptor_threads_ > 1;
    int error = 0;
    std::vector<OwnedFd> group;
    group.push_back(make_listener(address, shared, error));
    if (!group.back().valid())
        return error;
    // Siblings must name the actual port, which an ephemeral request reveals
    // only after the first bind; the whole group listens before the port is
    // reported, so no connection can arrive while it is still forming.
    socklen_t size = sizeof(address);
    if (::getsockname(group.front().get(), reinterpret_cast<sockaddr *>(&address), &size) < 0)
        return errno;
    for (int index = 1; index < acceptor_threads_; ++index) {
        group.push_back(make_listener(address, shared, error));
        if (!group.back().valid())
            return error; // RAII closes the members that did bind
    }
    if (bound_port != nullptr)
        *bound_port = ntohs(address.sin_port);
    for (auto &listener : group)
        listeners_.push_back(listener.release());
    return 0;
}

int LoopbackServer::serve(RequestHandler handler, std::chrono::milliseconds idle_timeout,
                          const std::atomic_bool &stop) {
    if (listeners_.empty() || !handler)
        return EINVAL;
    std::atomic_bool abort{false};
    if (listeners_.size() == 1)
        return run_acceptor(listeners_.front(), handler, idle_timeout, stop, abort);
    // Every sibling listener gets its own loop; the calling thread keeps loop 0.
    std::vector<int> results(listeners_.size(), 0);
    std::vector<std::thread> loops;
    try {
        for (std::size_t index = 1; index < listeners_.size(); ++index)
            loops.emplace_back([handler, this, &results, index, &idle_timeout, &stop, &abort] {
                results[index] =
                    run_acceptor(listeners_[index], handler, idle_timeout, stop, abort);
            });
    } catch (...) {
        // A loop that never started cannot raise abort itself.
        abort.store(true, std::memory_order_relaxed);
        for (auto &loop : loops)
            loop.join();
        return EAGAIN;
    }
    results[0] = run_acceptor(listeners_.front(), handler, idle_timeout, stop, abort);
    for (auto &loop : loops)
        loop.join();
    for (const int result : results)
        if (result != 0)
            return result;
    return 0;
}

int serve_loopback(std::uint16_t port, RequestHandler handler,
                   std::chrono::milliseconds idle_timeout, const std::atomic_bool &stop,
                   std::uint16_t *bound_port, int acceptor_threads) {
    LoopbackServer server{acceptor_threads};
    if (const int error = server.bind(port, bound_port); error != 0)
        return error;
    return server.serve(std::move(handler), idle_timeout, stop);
}

} // namespace orange::http
