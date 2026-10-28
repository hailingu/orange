// ADR: docs/adr/ADR-0003-local-runtime-path.md
// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// The loopback event loop (ADR-0003, subtask T-1): one thread multiplexes
// a 127.0.0.1 listener and every accepted connection through kqueue(),
// driving readiness-based read/write state machines per connection. This
// class creates no threads; callers that need the loop beside other work
// own that decision (the test harness hosts the loop on one thread).
#ifndef ORANGE_HTTP_SERVER_HPP
#define ORANGE_HTTP_SERVER_HPP
#include "http_request_decoder.hpp"
#include "http_response.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
namespace orange::http {

// Serves one complete parsed message and returns the response to send.
// Called on acceptor loop threads only; with more than one acceptor loop the
// calls happen concurrently, so the handler must hold no shared mutable
// state, and it must not block on I/O.
using RequestHandler = std::function<Response(const ParsedMessage &)>;

// Owns the loopback listening descriptor and the kqueue() acceptor loops.
// Descriptors never leave this class; destruction closes whatever is still
// open. With one acceptor thread (the default) the loop is exactly the
// historical single-threaded event loop; more threads form a shared-nothing
// SO_REUSEPORT listener group with one independent loop per listener.
class LoopbackServer {
  public:
    // An empty value holds no descriptor.
    LoopbackServer() noexcept = default;
    // Closes the listening descriptor if one was bound.
    ~LoopbackServer();
    LoopbackServer(const LoopbackServer &) = delete;
    LoopbackServer &operator=(const LoopbackServer &) = delete;

    // Binds 127.0.0.1 only — loopback by construction, no LAN or cloud
    // listener can be produced (CON-02) — and starts listening. A busy port
    // still fails with EADDRINUSE; serve forms any SO_REUSEPORT group only
    // when several acceptor threads are requested. port 0 requests an
    // ephemeral port; when bound_port is not null the actual port is stored
    // through it before returning. Returns the errno of the first failing
    // socket call, or 0 on success.
    [[nodiscard]] int bind(std::uint16_t port, std::uint16_t *bound_port = nullptr);

    // Runs the event loop(s) until stop loads true, then closes every owned
    // descriptor and returns 0; returns the errno of an unrecoverable socket
    // failure, or EINVAL when not bound, the handler is empty or
    // acceptor_threads is outside 1..64. With acceptor_threads 1 the calling
    // thread runs the historical single loop; with N > 1 the calling thread
    // runs one loop and N-1 additional loops each own a kernel-balanced
    // sibling listener, its own kqueue and connection store — no connection
    // or mutable state crosses loops, and a failing loop signals its
    // siblings to stop. Each accepted connection is configured with
    // TCP_NODELAY (Nagle's algorithm disabled) and feeds received bytes to
    // its own RequestDecoder; a complete message goes to handler and the
    // serialized response is queued; a terminal ParseError queues the
    // error_response rejection and closes after the flush; a request
    // Connection: close is honored the same way (RFC 9110 7.6.1). A
    // connection with no read or write progress for idle_timeout is closed.
    // Reading pauses while a connection holds read_pause_cap (1 MiB) of
    // undelivered output. Receive windows are consumed directly by the
    // bounded decoder before their stack storage expires.
    [[nodiscard]] int serve(RequestHandler handler, std::chrono::milliseconds idle_timeout,
                            const std::atomic_bool &stop, int acceptor_threads = 1);

  private:
    int listen_fd_ = -1;
};

// The one-shot form of the loops above: bind port (0 = ephemeral, reported
// through bound_port when not null), then serve until stop. Returns the
// errno-style code of the first failing phase, or 0 after a clean stop.
[[nodiscard]] int serve_loopback(std::uint16_t port, RequestHandler handler,
                                 std::chrono::milliseconds idle_timeout,
                                 const std::atomic_bool &stop, std::uint16_t *bound_port = nullptr,
                                 int acceptor_threads = 1);
} // namespace orange::http
#endif // ORANGE_HTTP_SERVER_HPP
