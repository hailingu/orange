// ADR: docs/adr/ADR-0010-http-server-performance-measurement.md
// Regresses unreaped child processes and control-channel failure concealment.
#include "http_bench_server.hpp"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

namespace {
// Verifies the owner reaped its own child; observing ECHILD does not reap others.
bool reaped(pid_t child) {
    int status = 0;
    return ::waitpid(child, &status, WNOHANG) == -1 && errno == ECHILD;
}

// Removing shutdown/reaping or including an in-process server must fail this case.
bool clean_stop() {
    orange::bench::ServerProcess server;
    const pid_t child = server.process_id();
    server.begin();
    const auto metrics = server.end();
    return child > 0 && child != ::getpid() && server.port != 0 &&
           metrics.process_cpu_seconds >= 0 && metrics.thread_cpu_seconds >= 0 &&
           metrics.peak_rss_bytes > 0 && server.finish() == 0 && reaped(child) &&
           server.finish() == 0;
}

// A killed owned server must produce a measurement error and still be reaped.
bool interrupted_child() {
    orange::bench::ServerProcess server;
    const pid_t child = server.process_id();
    server.begin();
    if (child <= 0 || ::kill(child, SIGKILL) != 0)
        return false;
    bool failed = false;
    try {
        (void)server.end();
    } catch (const std::runtime_error &) {
        failed = true;
    }
    return failed && server.finish() != 0 && reaped(child);
}

// A repeated begin is rejected by the real child state machine, without hanging.
bool invalid_transition() {
    orange::bench::ServerProcess server;
    const pid_t child = server.process_id();
    server.begin();
    bool failed = false;
    try {
        server.begin();
    } catch (const std::runtime_error &) {
        failed = true;
    }
    return failed && server.finish() != 0 && reaped(child);
}
} // namespace

// Uses actual fork, control sockets, Mach CPU sampling and waitpid boundaries.
int main() {
    try {
        const bool pass = clean_stop() && interrupted_child() && invalid_transition();
        std::printf("isolated server lifecycle: %s\n", pass ? "Pass" : "Fail");
        return pass ? 0 : 1;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "isolated server lifecycle failed: %s\n", error.what());
        return 1;
    }
}
