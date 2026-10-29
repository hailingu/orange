// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// Detects lost/doubled dirty work, incorrect timeout ordering and early reclamation.
#include "http_connection_store.hpp"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;
using namespace orange::http::detail;
int checks = 0;

// Reports the invariant that failed instead of relying on debug-only assertions.
void check(bool value, const char *name) {
    ++checks;
    if (!value)
        throw std::runtime_error(name);
}

// Exercises real descriptor ownership without requiring a listener or wall clock.
OwnedFd descriptor() {
    const int fd = ::open("/dev/null", O_RDONLY);
    if (fd < 0)
        throw std::runtime_error("descriptor setup");
    return OwnedFd(fd);
}

// Mutations that duplicate work or leave a closed pointer queued must fail here.
void dirty_and_retired() {
    ConnectionStore store;
    const auto t = std::chrono::steady_clock::time_point{};
    auto &a = store.add(descriptor(), t);
    auto &b = store.add(descriptor(), t + 1ms);
    auto &c = store.add(descriptor(), t + 2ms);
    store.mark_dirty(a);
    store.mark_dirty(b);
    check(store.take_dirty() == &a, "dirty FIFO first");
    store.mark_dirty(b);
    const int closed_fd = b.fd.get();
    b.outgoing = "batch identity survives close";
    store.retire(b);
    store.retire(b);
    check(!b.fd.valid(), "retire closes descriptor");
    check(::fcntl(closed_fd, F_GETFD) == -1 && errno == EBADF, "retired fd released");
    check(b.outgoing == "batch identity survives close", "retire defers object destruction");
    check(store.take_dirty() == &c, "closed middle entry removed from dirty work");
    check(store.take_dirty() == nullptr, "dirty work is deduplicated");
    store.mark_dirty(a);
    store.mark_dirty(a);
    check(store.take_dirty() == &a, "processed connection can be queued again");
    check(store.take_dirty() == nullptr, "repeat marks are idempotent");
    store.collect();
    check(a.fd.valid() && c.fd.valid(), "collect preserves live connections");
}

// Keeping a touched former head in front would expire the wrong connection.
void ordered_deadlines() {
    ConnectionStore store;
    const auto t = std::chrono::steady_clock::time_point{};
    check(store.next_deadline(10ms) == decltype(t)::max(), "empty deadline");
    auto &a = store.add(descriptor(), t);
    auto &b = store.add(descriptor(), t + 1ms);
    auto &c = store.add(descriptor(), t + 2ms);
    store.touch(a, t + 5ms);
    check(store.next_deadline(10ms) == t + 11ms, "activity reorders nearest expiry");
    store.expire(t + 10ms, 10ms);
    check(a.fd.valid() && b.fd.valid() && c.fd.valid(), "no early expiry");
    store.expire(t + 11ms, 10ms);
    check(!b.fd.valid() && a.fd.valid() && c.fd.valid(), "exact-boundary expiry");
    check(store.next_deadline(10ms) == t + 12ms, "next oldest deadline");
    store.touch(c, t + 11ms);
    store.expire(t + 15ms, 10ms);
    check(!a.fd.valid() && c.fd.valid(), "refresh preserves live peer");
    // Touching the already-newest node must leave the deadline order intact.
    store.touch(c, t + 12ms);
    check(store.next_deadline(10ms) == t + 22ms, "tail touch refreshes the sole deadline");
    auto &d = store.add(descriptor(), t + 13ms);
    store.touch(d, t + 14ms);
    check(store.next_deadline(10ms) == t + 22ms, "touching the newest node keeps the older head");
    store.expire(t + 22ms, 10ms);
    check(!c.fd.valid() && d.fd.valid(), "older node expires after a tail-only refresh");
    store.expire(t + 24ms, 10ms);
    check(!d.fd.valid(), "tail node expires at its refreshed deadline");
    check(store.take_dirty() == nullptr, "expired entries removed from pending work");
    check(store.next_deadline(10ms) == decltype(t)::max(), "all expired deadline");
    store.collect();
}
} // namespace

// Each case operates on the actual lifecycle owner used by the event loop.
int main() {
    try {
        dirty_and_retired();
        ordered_deadlines();
        std::printf("connection store: %d checks Pass\n", checks);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "connection store failed: %s\n", error.what());
        return 1;
    }
}
