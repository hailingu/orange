// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// KS-2: a fixed 1 MiB body must reach the handler without a separate
// 8–64 KiB input-staging allocation. Counting is local to the server thread;
// body storage and the small owned head remain part of normal decoding.
#include "http_server.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <new>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
thread_local bool counting = false;
thread_local unsigned staging_allocations = 0;
} // namespace

// Counts the declared staging-size band without changing allocation semantics.
void *operator new(std::size_t size) {
    if (counting && size >= 8193 && size <= 65536)
        ++staging_allocations;
    if (void *memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc{};
}
// Matches ordinary allocations used by standard-library request buffers.
void operator delete(void *memory) noexcept { std::free(memory); }

// Drives a fragmented body over real sockets and validates owned body bytes.
int main() {
    orange::http::LoopbackServer server;
    std::uint16_t port = 0;
    if (server.bind(0, &port) != 0)
        return 1;
    std::atomic_bool stop{false};
    std::atomic_bool body_valid{false};
    unsigned allocations = 0;
    int result = -1;
    std::thread worker([&] {
        counting = true;
        result = server.serve(
            [&](const orange::http::ParsedMessage &message) {
                body_valid.store(message.body.size() == 1048576 &&
                                 std::all_of(message.body.begin(), message.body.end(),
                                             [](char c) { return c == 'x'; }));
                return orange::http::Response{
                    200, orange::http::BodyFraming::content_length, "ok", {}, false};
            },
            std::chrono::seconds(5), stop);
        counting = false;
        allocations = staging_allocations;
    });
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    const timeval timeout{2, 0};
    const int on = 1;
    bool pass = fd >= 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) == 0 &&
                ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
    const std::string request =
        "POST / HTTP/1.1\r\nContent-Length: 1048576\r\n\r\n" + std::string(1048576, 'x');
    std::size_t sent = 0;
    while (pass && sent < request.size()) {
        const auto count = ::send(fd, request.data() + sent,
                                  std::min<std::size_t>(16384, request.size() - sent), 0);
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
    std::printf("KS-2: staging_band_allocations=%u body=%s\n", allocations,
                body_valid.load() ? "Pass" : "Fail");
    return pass && received == expected && body_valid.load() && result == 0 && allocations == 0 ? 0
                                                                                                : 1;
}
