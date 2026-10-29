// ADR: docs/adr/ADR-0010-http-server-performance-measurement.md
// Measures real loopback request completion with independent wire validation.
// The client multiplexes nonblocking sockets, optionally across disjoint
// driving shards (--client-threads); the production loop runs in a child
// process forked before any client thread starts. CPU and RSS are attributed
// separately.
#include "http_bench_server.hpp"
#include "routes.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

// Owns one client descriptor, including early validation-failure paths.
struct Client {
    int fd = -1;
    std::size_t sent = 0;
    std::size_t received = 0;
    Clock::time_point started{};
    bool done = false;
    explicit Client(int socket) : fd(socket) {}
    ~Client() {
        if (fd >= 0)
            ::close(fd);
    }
    Client(Client &&other) noexcept : fd(std::exchange(other.fd, -1)) {}
    Client(const Client &) = delete;
};

using Server = orange::bench::ServerProcess;

// Scenario dimensions keep connection count, pipeline depth and body distinct.
struct Scenario {
    const char *name;
    int active;
    int idle;
    int pipeline;
    std::size_t body;
};
constexpr Scenario scenarios[] = {
    {"health_c1", 1, 0, 1, 0},          {"health_c8", 8, 0, 1, 0},
    {"health_c64", 64, 0, 1, 0},        {"health_idle64", 1, 64, 1, 0},
    {"health_pipeline16", 1, 0, 16, 0}, {"echo_1k_c1", 1, 0, 1, 1024},
    {"echo_1k_c8", 8, 0, 1, 1024},      {"echo_1m_c1", 1, 0, 1, 1048576},
    {"health_idle512", 1, 512, 1, 0},   {"health_idle2048", 1, 2048, 1, 0}};

// Constructs expected protocol bytes without calling the production serializer.
std::pair<std::string, std::string> wires(const Scenario &scenario) {
    if (scenario.body == 0)
        return {"GET /api/health HTTP/1.1\r\nHost: localhost\r\n\r\n",
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                "Content-Length: 15\r\n\r\n{\"status\":\"ok\"}"};
    const std::string body(scenario.body, 'x');
    const std::string hex = scenario.body == 1024 ? "400" : "100000";
    return {"POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
                std::to_string(scenario.body) + "\r\n\r\n" + body,
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + hex + "\r\n" + body +
                "\r\n0\r\n\r\n"};
}

// Connects with a bounded timeout and identical TCP options for every revision.
Client connect_to(std::uint16_t port) {
    Client client(::socket(AF_INET, SOCK_STREAM, 0));
    if (client.fd < 0)
        throw std::runtime_error("client socket failed");
    const timeval timeout{2, 0};
    const int on = 1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::setsockopt(client.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0 ||
        ::setsockopt(client.fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) < 0 ||
        ::setsockopt(client.fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0 ||
        ::connect(client.fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0 ||
        ::fcntl(client.fd, F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("client setup errno=" + std::to_string(errno));
    return client;
}

// Sends a window without blocking; all socket failures remain explicit.
void send_window(Client &client, const std::string &request) {
    while (client.sent < request.size()) {
        if (client.sent == 0)
            client.started = Clock::now();
        const auto count =
            ::send(client.fd, request.data() + client.sent, request.size() - client.sent, 0);
        if (count > 0)
            client.sent += static_cast<std::size_t>(count);
        else if (count < 0 && errno == EINTR)
            continue;
        else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        else
            throw std::runtime_error("client send failed");
    }
}

// Validates each byte and records completion of each response in a window.
void receive_window(Client &client, const std::string &response, int pipeline,
                    std::vector<double> &latencies) {
    const auto total = response.size() * static_cast<std::size_t>(pipeline);
    while (client.received < total) {
        char buffer[16384];
        const auto count =
            ::recv(client.fd, buffer, std::min(sizeof(buffer), total - client.received), 0);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        if (count <= 0)
            throw std::runtime_error("client receive failed or unexpected EOF");
        std::size_t offset = 0;
        while (offset < static_cast<std::size_t>(count)) {
            const auto at = client.received % response.size();
            const auto length =
                std::min(response.size() - at, static_cast<std::size_t>(count) - offset);
            if (std::memcmp(buffer + offset, response.data() + at, length) != 0)
                throw std::runtime_error("response wire mismatch");
            offset += length;
            client.received += length;
            if (client.received % response.size() == 0)
                latencies.push_back(
                    std::chrono::duration<double, std::micro>(Clock::now() - client.started)
                        .count());
        }
    }
}

// Drives one disjoint slice of clients through the closed loop; a shard error
// is recorded instead of propagating across the thread boundary. Every client
// belongs to exactly one shard, so no state is shared between shards.
void drive(std::span<Client> clients, const std::string &request, const std::string &response,
           int pipeline, std::vector<double> &latencies, Clock::time_point end,
           std::string &failure) {
    try {
        std::vector<pollfd> fds(clients.size());
        std::size_t done = 0;
        while (done < clients.size()) {
            if (Clock::now() > end + 3s)
                throw std::runtime_error("client deadline exceeded");
            for (std::size_t i = 0; i < clients.size(); ++i) {
                const auto &client = clients[i];
                fds[i] = {client.fd,
                          static_cast<short>(client.done                    ? 0
                                             : client.sent < request.size() ? POLLOUT
                                                                            : POLLIN),
                          0};
            }
            if (::poll(fds.data(), fds.size(), 100) < 0) {
                if (errno == EINTR)
                    continue;
                throw std::runtime_error("client poll failed");
            }
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto &client = clients[i];
                if (client.done)
                    continue;
                if (fds[i].revents & (POLLERR | POLLNVAL | POLLHUP))
                    throw std::runtime_error("client readiness error");
                if (fds[i].revents & POLLOUT)
                    send_window(client, request);
                if (fds[i].revents & POLLIN)
                    receive_window(client, response, pipeline, latencies);
                if (client.received != response.size() * static_cast<std::size_t>(pipeline))
                    continue;
                if (Clock::now() >= end) {
                    client.done = true;
                    ++done;
                } else {
                    client.sent = 0;
                    client.received = 0;
                }
            }
        }
    } catch (const std::exception &error) {
        failure = error.what();
    }
}

// Measures a closed-loop load across disjoint client shards, then drains every
// issued request before returning. All shards share one deadline; latencies
// merge unordered and are sorted by the caller.
double exercise(std::vector<Client> &clients, const std::string &request,
                const std::string &response, int pipeline, int duration_ms,
                std::vector<double> &latencies, int client_threads) {
    const auto begin = Clock::now();
    const auto end = begin + std::chrono::milliseconds(duration_ms);
    const std::size_t shards = std::min<std::size_t>(client_threads, clients.size());
    auto slice = [&](std::size_t shard) {
        const std::size_t first = clients.size() * shard / shards;
        const std::size_t last = clients.size() * (shard + 1) / shards;
        return std::span<Client>(clients.data() + first, last - first);
    };
    std::vector<std::vector<double>> shard_latencies(shards);
    std::vector<std::string> failures(shards);
    std::vector<std::thread> workers;
    for (std::size_t shard = 1; shard < shards; ++shard)
        workers.emplace_back([&, shard] {
            drive(slice(shard), request, response, pipeline, shard_latencies[shard], end,
                  failures[shard]);
        });
    drive(slice(0), request, response, pipeline, shard_latencies[0], end, failures[0]);
    for (auto &worker : workers)
        worker.join();
    for (const auto &failure : failures)
        if (!failure.empty())
            throw std::runtime_error(failure);
    for (auto &samples : shard_latencies)
        latencies.insert(latencies.end(), samples.begin(), samples.end());
    return std::chrono::duration<double>(Clock::now() - begin).count();
}

// Converts the isolated client process CPU to seconds.
double cpu_seconds(const rusage &usage) {
    return usage.ru_utime.tv_sec + usage.ru_stime.tv_sec +
           (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000000.0;
}

// Reports nearest-rank percentiles over validated individual response completions.
double percentile(const std::vector<double> &samples, std::size_t percent) {
    return samples[(samples.size() * percent + 99) / 100 - 1];
}

// Raises only this disposable process's soft budget before the server inherits it.
void prepare_descriptor_budget(const Scenario &scenario) {
    rlimit limit{};
    const rlim_t needed = static_cast<rlim_t>(scenario.active + scenario.idle + 64);
    if (::getrlimit(RLIMIT_NOFILE, &limit) != 0 || needed > limit.rlim_max)
        throw std::runtime_error("insufficient descriptor budget for fixture");
    if (needed > limit.rlim_cur) {
        limit.rlim_cur = needed;
        if (::setrlimit(RLIMIT_NOFILE, &limit) != 0)
            throw std::runtime_error("fixture descriptor limit setup failed");
    }
}

// Excludes connection setup, warmup and shutdown from timing and CPU deltas.
void measure(const Scenario &scenario, int duration_ms, int client_threads, int server_threads) {
    prepare_descriptor_budget(scenario);
    Server server{server_threads};
    std::vector<Client> idle;
    for (int i = 0; i < scenario.idle; ++i)
        idle.push_back(connect_to(server.port));
    std::vector<Client> clients;
    for (int i = 0; i < scenario.active; ++i)
        clients.push_back(connect_to(server.port));
    const auto [single_request, response] = wires(scenario);
    std::string request;
    for (int i = 0; i < scenario.pipeline; ++i)
        request += single_request;
    std::vector<double> latencies;
    latencies.reserve(262144);
    exercise(clients, request, response, scenario.pipeline, 20, latencies, client_threads);
    for (auto &client : clients) {
        client.done = false;
        client.sent = 0;
        client.received = 0;
    }
    latencies.clear();
    server.begin();
    rusage before{}, after{};
    if (::getrusage(RUSAGE_SELF, &before) != 0)
        throw std::runtime_error("getrusage failed");
    const double elapsed = exercise(clients, request, response, scenario.pipeline, duration_ms,
                                    latencies, client_threads);
    if (::getrusage(RUSAGE_SELF, &after) != 0)
        throw std::runtime_error("getrusage failed");
    const auto server_metrics = server.end();
    if (latencies.empty())
        throw std::runtime_error("no completed requests");
    std::sort(latencies.begin(), latencies.end());
    std::cout << "{\"name\":\"" << scenario.name << "\",\"active_connections\":" << scenario.active
              << ",\"idle_connections\":" << scenario.idle << ",\"pipeline\":" << scenario.pipeline
              << ",\"client_threads\":" << client_threads
              << ",\"server_threads\":" << server_threads << ",\"body_bytes\":" << scenario.body
              << ",\"requests\":" << latencies.size() << ",\"elapsed_seconds\":" << elapsed
              << ",\"requests_per_second\":" << latencies.size() / elapsed
              << ",\"p50_us\":" << percentile(latencies, 50)
              << ",\"p95_us\":" << percentile(latencies, 95)
              << ",\"p99_us\":" << percentile(latencies, 99)
              << ",\"client_process_cpu_seconds\":" << cpu_seconds(after) - cpu_seconds(before)
              << ",\"client_peak_rss_bytes\":" << after.ru_maxrss
              << ",\"server_process_cpu_seconds\":" << server_metrics.process_cpu_seconds
              << ",\"server_thread_cpu_seconds\":" << server_metrics.thread_cpu_seconds
              << ",\"server_peak_rss_bytes\":" << server_metrics.peak_rss_bytes
              << ",\"server_unix_syscalls\":" << server_metrics.unix_syscalls
              << ",\"server_context_switches\":" << server_metrics.context_switches
              << ",\"client_pid\":" << ::getpid() << ",\"server_pid\":" << server.process_id()
              << ",\"errors\":0}";
    if (server.finish() != 0)
        throw std::runtime_error("server loop returned an error");
}

// Parses a bounded duration so malformed or unbounded invocations fail fast.
bool duration(std::string_view value, int &result) {
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && result > 0 &&
           result <= 10000;
}

// Parses a bounded client thread count; one thread is the historical driver.
bool thread_count(std::string_view value, int &result) {
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && result >= 1 &&
           result <= 8;
}
} // namespace

// The CLI exits 2 for invalid options and 1 for socket/wire/deadline failures.
int main(int argc, char **argv) {
    int duration_ms = 200;
    int client_threads = 1;
    int server_threads = 1;
    std::string_view selected;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--quick")
            duration_ms = 20;
        else if (argument == "--duration-ms" && i + 1 < argc && duration(argv[i + 1], duration_ms))
            ++i;
        else if (argument == "--client-threads" && i + 1 < argc &&
                 thread_count(argv[i + 1], client_threads))
            ++i;
        else if (argument == "--server-threads" && i + 1 < argc &&
                 thread_count(argv[i + 1], server_threads))
            ++i;
        else if (argument == "--case" && i + 1 < argc)
            selected = argv[++i];
        else {
            std::fprintf(stderr, "invalid server benchmark invocation\n");
            return 2;
        }
    }
    if (!selected.empty() &&
        std::none_of(std::begin(scenarios), std::end(scenarios),
                     [&](const auto &scenario) { return selected == scenario.name; }))
        return 2;
    try {
        std::cout << "{\"compiler\":\"" << ORANGE_BENCH_COMPILER << "\",\"flags\":\""
                  << ORANGE_BENCH_FLAGS << "\",\"architecture\":\"" << ORANGE_BENCH_ARCH
                  << "\",\"cases\":[";
        bool first = true;
        for (const auto &scenario : scenarios) {
            // Preserve the eight-case default; large idle fixtures are explicit.
            if (selected.empty() && scenario.idle > 64)
                continue;
            if (!selected.empty() && selected != scenario.name)
                continue;
            if (!first)
                std::cout << ',';
            first = false;
            measure(scenario, duration_ms, client_threads, server_threads);
        }
        std::cout << "]}\n";
    } catch (const std::exception &error) {
        std::fprintf(stderr, "server benchmark failed: %s\n", error.what());
        return 1;
    }
    return 0;
}
