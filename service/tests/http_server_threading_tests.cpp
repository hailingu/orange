// ADR: docs/adr/ADR-0003-local-runtime-path.md
// RT-1: several shared-nothing acceptor loops (SO_REUSEPORT listener group
// bound as a whole before the port is reported) must serve concurrent
// connections byte-exactly and stop cleanly. Each connection stays owned by
// whichever acceptor won it; no loop shares state.
#include "http_server.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
// The handler's constant response fields, mirroring the health route.
constexpr orange::http::ResponseField health_fields[] = {{"Content-Type", "application/json"}};

// One client exchange: request, exact expected response, close.
bool exchange(std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return false;
    const timeval timeout{2, 0};
    const int on = 1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) != 0 ||
        ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        ::close(fd);
        return false;
    }
    const std::string request = "GET /api/health HTTP/1.1\r\nHost: localhost\r\n\r\n";
    if (::send(fd, request.data(), request.size(), 0) != static_cast<ssize_t>(request.size())) {
        ::close(fd);
        return false;
    }
    const std::string expected =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 15\r\n"
        "\r\n{\"status\":\"ok\"}";
    std::string received;
    while (received.size() < expected.size()) {
        char bytes[128];
        const auto count = ::recv(fd, bytes, sizeof(bytes), 0);
        if (count <= 0) {
            ::close(fd);
            return false;
        }
        received.append(bytes, static_cast<std::size_t>(count));
    }
    ::close(fd);
    return received == expected;
}
} // namespace

int main() {
    orange::http::LoopbackServer server{3};
    std::uint16_t port = 0;
    if (server.bind(0, &port) != 0)
        return 1;
    std::atomic_bool stop{false};
    std::atomic_int served{0};
    int result = -1;
    std::thread worker([&] {
        result = server.serve(
            [&](const orange::http::ParsedMessage &) {
                ++served;
                return orange::http::Response{200,
                                              orange::http::BodyFraming::content_length,
                                              "{\"status\":\"ok\"}",
                                              {health_fields, 1},
                                              false};
            },
            std::chrono::seconds(5), stop);
    });
    std::vector<std::thread> clients;
    std::vector<bool> outcomes(6);
    for (int i = 0; i < 6; ++i)
        clients.emplace_back([&, i] { outcomes[static_cast<std::size_t>(i)] = exchange(port); });
    for (auto &client : clients)
        client.join();
    stop.store(true);
    worker.join();
    const bool pass = result == 0 && served.load() == 6 &&
                      std::all_of(outcomes.begin(), outcomes.end(), [](bool ok) { return ok; });
    std::printf("RT-1: clean_stop=%s concurrent_wires=%d/6\n", result == 0 ? "Pass" : "Fail",
                static_cast<int>(std::count(outcomes.begin(), outcomes.end(), true)));
    return pass ? 0 : 1;
}
