// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Wiring-only placeholder: the socket-loop contract in
// service/tests/http_server_tests.cpp lands red before the event loop
// replaces this stub in the green change.
#include "http_server.hpp"
#include <cerrno>

namespace orange::http {

LoopbackServer::~LoopbackServer() = default;

// Returns "not implemented" so the committed red suite is exercised
// against a linkable, behavior-free boundary.
int LoopbackServer::bind(std::uint16_t port, std::uint16_t *bound_port) {
    (void)port;
    (void)bound_port;
    listen_fd_ = -1;
    return ENOSYS;
}

int LoopbackServer::serve(RequestHandler handler, std::chrono::milliseconds idle_timeout,
                          const std::atomic_bool &stop) {
    (void)handler;
    (void)idle_timeout;
    (void)stop;
    return ENOSYS;
}

int serve_loopback(std::uint16_t port, RequestHandler handler,
                   std::chrono::milliseconds idle_timeout, const std::atomic_bool &stop,
                   std::uint16_t *bound_port) {
    (void)handler;
    (void)idle_timeout;
    (void)stop;
    (void)bound_port;
    (void)port;
    return ENOSYS;
}

} // namespace orange::http
