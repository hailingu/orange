// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// KS-5/KS-6: real kqueue registrations carry stable connection identities;
// growth, collection, descriptor reuse and same-batch write hints preserve wire.
#include "http_server.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
thread_local int listener = -1;
thread_local std::map<int, void *> identities;
thread_local bool tokens_valid = true;
thread_local unsigned registrations = 0;
thread_local unsigned write_hints = 0;

// Forwards to the operating system rather than replacing socket behavior.
template <class T> T system_call(const char *name) {
    const auto symbol = ::dlsym(RTLD_NEXT, name);
    if (!symbol)
        std::abort();
    return reinterpret_cast<T>(symbol);
}

// Tracks only the explicit kernel identity contract, independent of layout.
void observe_changes(const struct kevent *changes, int count) {
    for (int i = 0; i < count; ++i) {
        const auto &change = changes[i];
        const int fd = static_cast<int>(change.ident);
        if (listener < 0)
            listener = fd;
        if (fd == listener)
            continue;
        if (change.filter == EVFILT_READ && (change.flags & EV_ADD)) {
            identities[fd] = change.udata;
            ++registrations;
        }
        const auto found = identities.find(fd);
        tokens_valid = tokens_valid && change.udata != nullptr && found != identities.end() &&
                       found->second == change.udata;
    }
}

// Adds a currently writable hint after a real read, forcing both event passes
// to observe the same connection even if the read closes it. All I/O stays real.
int observe_events(struct kevent *events, int ready, int capacity) {
    int total = ready;
    for (int i = 0; i < ready; ++i) {
        const auto &event = events[i];
        const int fd = static_cast<int>(event.ident);
        if (fd == listener || (event.flags & EV_ERROR))
            continue;
        const auto found = identities.find(fd);
        tokens_valid = tokens_valid && event.udata != nullptr && found != identities.end() &&
                       found->second == event.udata;
        pollfd writable{fd, POLLOUT, 0};
        if (event.filter == EVFILT_READ && total < capacity && ::poll(&writable, 1, 0) == 1 &&
            (writable.revents & POLLOUT)) {
            events[total] = event;
            events[total++].filter = EVFILT_WRITE;
            ++write_hints;
        }
    }
    return total;
}

// Owns one client socket through every early test-failure path.
struct Client {
    int fd = -1;
    explicit Client(int descriptor) : fd(descriptor) {}
    ~Client() {
        if (fd >= 0)
            ::close(fd);
    }
    Client(Client &&other) noexcept : fd(std::exchange(other.fd, -1)) {}
    Client(const Client &) = delete;
};

// Connects with bounded reads/writes so a routing defect cannot hang CTest.
Client connect_to(std::uint16_t port) {
    Client client(::socket(AF_INET, SOCK_STREAM, 0));
    const timeval timeout{2, 0};
    const int on = 1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (client.fd < 0 ||
        ::setsockopt(client.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(client.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(client.fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) != 0 ||
        ::connect(client.fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        if (client.fd >= 0)
            ::close(std::exchange(client.fd, -1));
    }
    return client;
}

// Every request has a distinct expected target, exposing cross-connection reuse.
bool exchange(const Client &client, int id, bool close) {
    const std::string target = "/connection/" + std::to_string(id);
    const std::string request =
        "GET " + target + " HTTP/1.1\r\n" + (close ? "Connection: close\r\n" : "") + "\r\n";
    const std::string expected =
        "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(target.size()) + "\r\n" +
        (close ? "Connection: close\r\n" : "") + "\r\n" + target;
    std::size_t sent = 0;
    while (sent < request.size()) {
        const auto n = ::send(client.fd, request.data() + sent, request.size() - sent, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    std::string received;
    while (received.size() < expected.size()) {
        char bytes[256];
        const auto n = ::recv(client.fd, bytes, sizeof(bytes), 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        received.append(bytes, static_cast<std::size_t>(n));
    }
    if (received != expected)
        return false;
    if (close) {
        char byte;
        return ::recv(client.fd, &byte, 1, 0) == 0;
    }
    return true;
}

// Holds an early connection through vector growth, alternating collection,
// new accepts and descriptor reuse. Closing reads also get same-batch writes.
bool exercise(std::uint16_t port) {
    auto first = connect_to(port);
    if (!exchange(first, 0, false))
        return false;
    std::vector<Client> clients;
    for (int i = 1; i <= 96; ++i) {
        clients.push_back(connect_to(port));
        if (!exchange(clients.back(), i, false))
            return false;
    }
    for (int i = 0; i < 96; i += 2)
        if (!exchange(clients[static_cast<std::size_t>(i)], 1000 + i, true))
            return false;
    for (int i = 0; i < 64; ++i) {
        auto replacement = connect_to(port);
        if (!exchange(replacement, 2000 + i, true))
            return false;
    }
    for (int i = 1; i < 96; i += 2)
        if (!exchange(clients[static_cast<std::size_t>(i)], 3000 + i, true))
            return false;
    return exchange(first, 4000, true);
}
} // namespace

// Observes actual registrations and returns genuine readiness plus write hints.
extern "C" int kevent(int queue, const struct kevent *changes, int count, struct kevent *events,
                      int capacity, const timespec *timeout) {
    static const auto real = system_call<decltype(&::kevent)>("kevent");
    observe_changes(changes, count);
    const int ready = real(queue, changes, count, events, capacity, timeout);
    return ready > 0 ? observe_events(events, ready, capacity) : ready;
}

// Joins before reading server-thread observations; no production instrumentation.
int main() {
    orange::http::LoopbackServer server;
    std::uint16_t port = 0;
    if (server.bind(0, &port) != 0)
        return 1;
    std::atomic_bool stop{false};
    int result = -1;
    bool identities_valid = false;
    unsigned registered = 0;
    unsigned hints = 0;
    std::thread worker([&] {
        result = server.serve(
            [](const orange::http::ParsedMessage &message) {
                return orange::http::Response{200,
                                              orange::http::BodyFraming::content_length,
                                              message.request.target(),
                                              {},
                                              false};
            },
            std::chrono::seconds(10), stop);
        identities_valid = tokens_valid;
        registered = registrations;
        hints = write_hints;
    });
    const bool wire = exercise(port);
    stop.store(true);
    worker.join();
    std::printf("KS-5/KS-6: wire=%s stable_udata=%s registrations=%u same_batch_write_hints=%u\n",
                wire ? "Pass" : "Fail", identities_valid ? "Pass" : "Fail", registered, hints);
    return wire && identities_valid && registered == 161 && hints > 0 && result == 0 ? 0 : 1;
}
