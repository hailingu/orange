// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// Interposes only to observe real kevent/send calls: sockets, readiness and
// production dispatch still execute. KS-1 requires a newly queued response
// to attempt delivery before another readiness wait on that connection.
#include "http_server.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <netinet/in.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
thread_local unsigned waits = 0;
thread_local unsigned queued_at = 0;
thread_local bool pending = false;
std::atomic_bool observed{false};
std::atomic_bool immediate{false};

// Resolves the system implementation, preserving every actual I/O side effect.
template <class T> T system_call(const char *name) {
    const auto symbol = ::dlsym(RTLD_NEXT, name);
    if (!symbol)
        std::abort();
    return reinterpret_cast<T>(symbol);
}
} // namespace

// Observes readiness boundaries without altering their arguments or results.
extern "C" int kevent(int queue, const struct kevent *changes, int change_count,
                      struct kevent *events, int capacity, const timespec *timeout) {
    static const auto real = system_call<decltype(&::kevent)>("kevent");
    ++waits;
    return real(queue, changes, change_count, events, capacity, timeout);
}

// Observes the first actual send following the handler on the server thread.
extern "C" ssize_t send(int fd, const void *buffer, size_t size, int flags) {
    static const auto real = system_call<decltype(&::send)>("send");
    if (pending) {
        immediate.store(waits == queued_at);
        observed.store(true);
        pending = false;
    }
    return real(fd, buffer, size, flags);
}

// Uses a real client so a scheduling regression is tied to delivered wire bytes.
int main() {
    orange::http::LoopbackServer server;
    std::uint16_t port = 0;
    if (server.bind(0, &port) != 0)
        return 1;
    std::atomic_bool stop{false};
    int result = -1;
    std::thread worker([&] {
        result = server.serve(
            [](const orange::http::ParsedMessage &) {
                queued_at = waits;
                pending = true;
                return orange::http::Response{
                    200, orange::http::BodyFraming::content_length, "ok", {}, false};
            },
            std::chrono::seconds(5), stop);
    });
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    const timeval timeout{2, 0};
    bool pass = fd >= 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
                ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
    const std::string request = "GET / HTTP/1.1\r\n\r\n";
    if (pass)
        pass =
            ::send(fd, request.data(), request.size(), 0) == static_cast<ssize_t>(request.size());
    const std::string expected = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    std::string received;
    while (pass && received.size() < expected.size()) {
        char bytes[128];
        const auto count = ::recv(fd, bytes, sizeof(bytes), 0);
        if (count <= 0)
            pass = false;
        else
            received.append(bytes, static_cast<std::size_t>(count));
    }
    ::close(fd);
    stop.store(true);
    worker.join();
    pass = pass && received == expected && result == 0 && observed.load() && immediate.load();
    std::printf("KS-1: wire=%s first_send_before_next_wait=%s\n",
                received == expected ? "Pass" : "Fail", immediate.load() ? "Pass" : "Fail");
    return pass ? 0 : 1;
}
