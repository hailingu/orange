// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Drives the loopback event loop with real sockets: three keep-alive
// cycles and a pipelined pair on one connection, honoring a request
// Connection: close, the idle timeout, the 431/413/400 rejection
// lifecycle (exact response, EOF, no pipelined follow-up, reconnect
// success), an unfinished request not blocking another connection,
// request bodies reaching the handler in both framings, TCP_NODELAY on
// accepted connections, and the coded port-conflict error.
#include "http_server.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
using orange::http::BodyFraming;
using orange::http::LoopbackServer;
using orange::http::ParsedMessage;
using orange::http::RequestHandler;
using orange::http::Response;
using orange::http::ResponseField;

int failures = 0;
std::size_t checks = 0;
// Gives CTest a nonzero exit for any observable socket-level mismatch.
void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}

// Every blocking receive below fails fast instead of hanging the suite.
constexpr int receive_deadline_seconds = 2;

// A loopback client socket with a bounded receive deadline, or -1.
int connect_client(std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const timeval deadline{receive_deadline_seconds, 0};
    if (::connect(fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0 ||
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &deadline, sizeof(deadline)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

// Sends every byte or reports failure.
bool send_all(int fd, const std::string &bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const auto n = ::send(fd, bytes.data() + sent, bytes.size() - sent, 0);
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

// Reads exactly count bytes; false on early EOF, error, or deadline.
bool recv_exact(int fd, std::size_t count, std::string *out) {
    out->clear();
    while (out->size() < count) {
        char buffer[4096];
        const std::size_t want = std::min(count - out->size(), sizeof(buffer));
        const auto n = ::recv(fd, buffer, want, 0);
        if (n <= 0)
            return false;
        out->append(buffer, static_cast<std::size_t>(n));
    }
    return true;
}

// True when the peer closes within the deadline and sends no extra byte.
bool recv_eof(int fd) {
    char extra = 0;
    return ::recv(fd, &extra, 1, 0) == 0;
}

// The deterministic response every served request receives, so expected
// wire bytes stay independent of the serializer under test.
const std::string fixed_wire = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: 15\r\n"
                               "\r\n"
                               "{\"status\":\"ok\"}";

// Handler-owned field storage: a Response's field views must outlive the
// write_response call (the borrowed-view contract), so they cannot point
// into the handler's frame.
const std::array<ResponseField, 1> handler_fields{{{"Content-Type", "application/json"}}};

// Hosts the event loop on one thread with a stop flag; the loop itself
// stays single-threaded, and the destructor always stops and joins.
class TestServer {
  public:
    TestServer(std::chrono::milliseconds idle_timeout, RequestHandler handler)
        : handler_(std::move(handler)) {
        check(server_.bind(0, &port_) == 0, "the loop binds an ephemeral loopback port");
        thread_ = std::thread(
            [this, idle_timeout] { serve_exit_ = server_.serve(handler_, idle_timeout, stop_); });
    }
    ~TestServer() { stop_and_join(); }
    TestServer(const TestServer &) = delete;
    TestServer &operator=(const TestServer &) = delete;
    // Signals the stop flag and waits for the loop to finish.
    void stop_and_join() {
        stop_.store(true);
        if (thread_.joinable())
            thread_.join();
    }
    [[nodiscard]] std::uint16_t port() const { return port_; }
    // Valid only after stop_and_join: the loop's exit code.
    [[nodiscard]] int serve_exit() const { return serve_exit_; }

  private:
    LoopbackServer server_;
    RequestHandler handler_;
    std::atomic_bool stop_{false};
    std::thread thread_;
    std::uint16_t port_ = 0;
    int serve_exit_ = -1;
};

// Three request-response cycles on one stable connection — the exercised
// client model — plus a pipelined pair delivered in one write.
void check_keepalive(const TestServer &server) {
    const int fd = connect_client(server.port());
    check(fd >= 0, "keep-alive client connects");
    if (fd < 0)
        return;
    const std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    std::string received;
    for (int cycle = 0; cycle < 3; ++cycle) {
        check(send_all(fd, request), "keep-alive cycle sends its request");
        check(recv_exact(fd, fixed_wire.size(), &received) && received == fixed_wire,
              "keep-alive cycle receives the exact response in order");
    }
    check(send_all(fd, request + request), "pipelined pair sends in one delivery");
    check(recv_exact(fd, fixed_wire.size() * 2, &received) && received == fixed_wire + fixed_wire,
          "a pipelined pair receives both responses on one connection");
    ::close(fd);
}

// A request Connection: close gets its response and then EOF.
void check_request_close(const TestServer &server) {
    const int fd = connect_client(server.port());
    check(fd >= 0, "close-honoring client connects");
    if (fd < 0)
        return;
    const std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    const std::string expected = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 15\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "{\"status\":\"ok\"}";
    std::string received;
    check(send_all(fd, request) && recv_exact(fd, expected.size(), &received) &&
              received == expected && recv_eof(fd),
          "Connection: close is honored with a closing response then EOF");
    ::close(fd);
}

// An otherwise-valid connection closes at the configured idle timeout.
void check_idle_timeout(const TestServer &server) {
    const int fd = connect_client(server.port());
    check(fd >= 0, "idle-timeout client connects");
    if (fd < 0)
        return;
    const std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    std::string received;
    check(send_all(fd, request) && recv_exact(fd, fixed_wire.size(), &received) &&
              received == fixed_wire,
          "the idle client completes one cycle before idling");
    check(recv_eof(fd), "an idle connection is closed at the configured timeout");
    ::close(fd);
}

// One full rejection lifecycle: the exact closing response, EOF with no
// pipelined follow-up answer, then a valid request on a new connection.
void check_rejection(const TestServer &server, const std::string &offending,
                     const std::string &expected_response, const char *description) {
    const std::string follow_up = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    const int fd = connect_client(server.port());
    check(fd >= 0, "rejection client connects");
    std::string received;
    check(fd >= 0 && send_all(fd, offending + follow_up) &&
              recv_exact(fd, expected_response.size(), &received) &&
              received == expected_response && recv_eof(fd),
          description);
    ::close(fd);
    const int fresh = connect_client(server.port());
    std::string recovered;
    check(fresh >= 0 && send_all(fresh, follow_up) &&
              recv_exact(fresh, fixed_wire.size(), &recovered) && recovered == fixed_wire,
          "a valid request on a new connection succeeds after the rejection");
    ::close(fresh);
}

// The 431/413/400 rejections ADR-0003 CC-5 declares.
void check_rejections(const TestServer &server) {
    const std::string head = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n";
    check_rejection(server, head + "X-Big: " + std::string(9000, 'a') + "\r\n\r\n",
                    "HTTP/1.1 431 Request Header Fields Too Large\r\n"
                    "Connection: close\r\n\r\n",
                    "an oversized head receives 431, close and EOF with no follow-up");
    check_rejection(server, "POST / HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 1048577\r\n\r\n",
                    "HTTP/1.1 413 Content Too Large\r\nConnection: close\r\n\r\n",
                    "an over-capacity Content-Length receives 413, close and EOF");
    // 2000 one-byte chunks spend 10,000 metadata bytes on size lines and
    // data CRLFs alone, past the independent 8,192-byte budget.
    std::string tiny_chunks;
    for (int i = 0; i < 2000; ++i)
        tiny_chunks += "1\r\nx\r\n";
    check_rejection(server,
                    "POST / HTTP/1.1\r\nHost: 127.0.0.1\r\nTransfer-Encoding: chunked\r\n\r\n" +
                        tiny_chunks,
                    "HTTP/1.1 413 Content Too Large\r\nConnection: close\r\n\r\n",
                    "aggregate chunk metadata past 8192 bytes receives 413, close and EOF");
    check_rejection(server, "GET-ONLY\r\n\r\n",
                    "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n",
                    "a malformed request line receives 400, close and EOF");
}

// An unfinished request on one connection does not block another.
void check_two_connections(const TestServer &server) {
    const int paused = connect_client(server.port());
    const int active = connect_client(server.port());
    check(paused >= 0 && active >= 0, "both no-head-of-line clients connect");
    if (paused < 0 || active < 0) {
        ::close(paused);
        ::close(active);
        return;
    }
    const std::string partial = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n";
    const std::string request = partial + "\r\n";
    std::string received;
    check(send_all(paused, partial) && send_all(active, request) &&
              recv_exact(active, fixed_wire.size(), &received) && received == fixed_wire,
          "a complete request finishes while another connection is unfinished");
    check(send_all(paused, "\r\n") && recv_exact(paused, fixed_wire.size(), &received) &&
              received == fixed_wire,
          "the paused connection completes after the other one");
    ::close(paused);
    ::close(active);
}

// Request bodies reach the handler decoded, in both framings.
void check_bodies_reach_handler(const TestServer &server, const std::vector<std::string> *bodies) {
    const int fd = connect_client(server.port());
    check(fd >= 0, "body client connects");
    if (fd < 0)
        return;
    const std::string head = "POST / HTTP/1.1\r\nHost: 127.0.0.1\r\n";
    std::string received;
    check(send_all(fd, head + "Content-Length: 5\r\n\r\nhello") &&
              recv_exact(fd, fixed_wire.size(), &received) && received == fixed_wire &&
              !bodies->empty() && bodies->back() == "hello",
          "a Content-Length request body reaches the handler decoded");
    check(send_all(fd, head + "Transfer-Encoding: chunked\r\n\r\n5\r\nworld\r\n0\r\n\r\n") &&
              recv_exact(fd, fixed_wire.size(), &received) && received == fixed_wire &&
              !bodies->empty() && bodies->back() == "world",
          "a chunked request body reaches the handler decoded");
    ::close(fd);
}

// Locates the loop's accepted descriptor for the client whose local
// port is client_port: the loop runs in this process, so the server
// side of that connection is the one connected socket whose peer is
// the client. Kernel socket options are only observable through the
// descriptor itself, so the test must find it in-process.
int find_server_side(std::uint16_t client_port) {
    const int table_size = ::getdtablesize();
    for (int fd = 0; fd < table_size; ++fd) {
        sockaddr_storage peer{};
        socklen_t size = sizeof(peer);
        if (::getpeername(fd, reinterpret_cast<sockaddr *>(&peer), &size) < 0)
            continue;
        if (peer.ss_family != AF_INET)
            continue;
        const auto *address = reinterpret_cast<const sockaddr_in *>(&peer);
        if (address->sin_addr.s_addr == htonl(INADDR_LOOPBACK) &&
            ntohs(address->sin_port) == client_port)
            return fd;
    }
    return -1;
}

// Every accepted connection runs with Nagle's algorithm disabled, so a
// queued response leaves without waiting for the peer's acknowledgment
// of an earlier small segment.
void check_no_delay(const TestServer &server) {
    const int fd = connect_client(server.port());
    check(fd >= 0, "no-delay client connects");
    if (fd < 0)
        return;
    const std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    std::string received;
    check(send_all(fd, request) && recv_exact(fd, fixed_wire.size(), &received) &&
              received == fixed_wire,
          "the no-delay client completes one cycle, so its connection is accepted");
    sockaddr_in local{};
    socklen_t size = sizeof(local);
    const int accepted = ::getsockname(fd, reinterpret_cast<sockaddr *>(&local), &size) == 0
                             ? find_server_side(ntohs(local.sin_port))
                             : -1;
    check(accepted >= 0, "the loop's accepted descriptor for the client is found in-process");
    int no_delay = 0;
    socklen_t option_size = sizeof(no_delay);
    check(accepted >= 0 &&
              ::getsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, &no_delay, &option_size) == 0 &&
              no_delay != 0,
          "an accepted connection has TCP_NODELAY enabled");
    ::close(fd);
}

// A busy port produces a coded bind error rather than a silent
// alternative port (ADR-0003 TN-2).
void check_port_conflict(std::uint16_t port) {
    LoopbackServer second;
    check(second.bind(port) != 0, "binding a busy port returns a coded error");
}
} // namespace

// Executes the real-socket checks against one hosted event loop.
int main() {
    const auto bodies = std::make_shared<std::vector<std::string>>();
    const RequestHandler handler = [bodies](const ParsedMessage &message) {
        bodies->push_back(message.body);
        return Response{200, BodyFraming::content_length, "{\"status\":\"ok\"}", handler_fields,
                        false};
    };
    {
        TestServer server(std::chrono::milliseconds(1000), handler);
        check_keepalive(server);
        check_request_close(server);
        check_idle_timeout(server);
        check_rejections(server);
        check_two_connections(server);
        check_bodies_reach_handler(server, bodies.get());
        check_no_delay(server);
        check_port_conflict(server.port());
        server.stop_and_join();
        check(server.serve_exit() == 0, "the loop exits cleanly through its stop flag");
    }
    std::printf("%zu checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
