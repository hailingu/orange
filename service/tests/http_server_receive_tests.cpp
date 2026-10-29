// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// KS-11: interposes only to observe real recv calls on the server thread. A
// request whose 64 KiB body lands in one client write must be drained in at
// most three data receives, which a 16 KiB receive window cannot do.
#include "http_server.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
thread_local bool counting = false;
thread_local unsigned data_receives = 0;

// Resolves the system implementation, preserving every actual I/O side effect.
template <class T> T system_call(const char *name) {
    const auto symbol = ::dlsym(RTLD_NEXT, name);
    if (!symbol)
        std::abort();
    return reinterpret_cast<T>(symbol);
}
} // namespace

// Counts only receives that returned data, and only on the watched thread.
extern "C" ssize_t recv(int fd, void *buffer, size_t size, int flags) {
    static const auto real = system_call<decltype(&::recv)>("recv");
    const auto received = real(fd, buffer, size, flags);
    if (counting && received > 0)
        ++data_receives;
    return received;
}

// Uses a real socket so the window contract is tied to delivered wire bytes.
int main() {
    orange::http::LoopbackServer server;
    std::uint16_t port = 0;
    if (server.bind(0, &port) != 0)
        return 1;
    std::atomic_bool stop{false};
    std::atomic_bool body_valid{false};
    unsigned receives = 0;
    int result = -1;
    std::thread worker([&] {
        counting = true;
        result = server.serve(
            [&](const orange::http::ParsedMessage &message) {
                body_valid.store(message.body.size() == 65536 &&
                                 std::all_of(message.body.begin(), message.body.end(),
                                             [](char c) { return c == 'x'; }));
                return orange::http::Response{
                    200, orange::http::BodyFraming::content_length, "ok", {}, false};
            },
            std::chrono::seconds(5), stop);
        counting = false;
        receives = data_receives;
    });
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    const timeval timeout{2, 0};
    const int on = 1;
    const int send_buffer = 1 << 18;
    bool pass = fd >= 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(send_buffer)) == 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) == 0 &&
                ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
    const std::string request =
        "POST /api/echo HTTP/1.1\r\nContent-Length: 65536\r\n\r\n" + std::string(65536, 'x');
    std::size_t sent = 0;
    while (pass && sent < request.size()) {
        const auto count = ::send(fd, request.data() + sent, request.size() - sent, 0);
        if (count <= 0)
            pass = false;
        else
            sent += static_cast<std::size_t>(count);
    }
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
    const bool window = receives <= 3;
    std::printf("KS-11: wire=%s body=%s data_receives=%u window64k=%s\n",
                received == expected ? "Pass" : "Fail", body_valid.load() ? "Pass" : "Fail",
                receives, window ? "Pass" : "Fail");
    return pass && received == expected && body_valid.load() && result == 0 && window ? 0 : 1;
}
