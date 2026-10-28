// ADR: docs/adr/ADR-0010-http-server-performance-measurement.md
#include "http_bench_server.hpp"
#include "http_server.hpp"
#include "routes.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <mach/mach.h>
#include <pthread.h>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace orange::bench {
namespace {
// Transfers one fixed local control frame, preserving short I/O and EINTR.
void transfer(int fd, void *bytes, std::size_t size, bool sending) {
    auto *next = static_cast<char *>(bytes);
    while (size != 0) {
        const auto n = sending ? ::send(fd, next, size, 0) : ::recv(fd, next, size, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            throw std::runtime_error("server control channel failed");
        next += n;
        size -= static_cast<std::size_t>(n);
    }
}

// Sums user+system CPU over every task thread except the calling control
// thread, so several acceptor loops report as one worker aggregate.
double server_threads_cpu() {
    thread_act_array_t threads{};
    mach_msg_type_number_t count = 0;
    if (::task_threads(::mach_task_self(), &threads, &count) != KERN_SUCCESS)
        throw std::runtime_error("server thread enumeration failed");
    const auto self = ::mach_thread_self();
    double total = 0;
    for (mach_msg_type_number_t i = 0; i < count; ++i) {
        thread_basic_info_data_t info{};
        mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
        if (threads[i] != self &&
            ::thread_info(threads[i], THREAD_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                          &info_count) == KERN_SUCCESS)
            total += info.user_time.seconds + info.system_time.seconds +
                     (info.user_time.microseconds + info.system_time.microseconds) / 1000000.0;
        ::mach_port_deallocate(::mach_task_self(), threads[i]);
    }
    ::vm_deallocate(::mach_task_self(), reinterpret_cast<vm_address_t>(threads),
                    count * sizeof(thread_t));
    return total;
}

// Captures cumulative process/thread CPU without instrumenting request handling.
// Task events are read first so the End sample excludes its own CPU queries.
// One acceptor samples its worker thread directly; several acceptors sum
// every task thread except this control thread.
ServerMetrics snapshot(std::thread &thread, int acceptors) {
    task_events_info_data_t events{};
    mach_msg_type_number_t events_count = TASK_EVENTS_INFO_COUNT;
    if (::task_info(::mach_task_self(), TASK_EVENTS_INFO, reinterpret_cast<task_info_t>(&events),
                    &events_count) != KERN_SUCCESS)
        throw std::runtime_error("server event sampling failed");
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("server CPU sampling failed");
    double worker_cpu = 0;
    if (acceptors == 1) {
        thread_basic_info_data_t info{};
        mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
        if (::thread_info(::pthread_mach_thread_np(thread.native_handle()), THREAD_BASIC_INFO,
                          reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
            throw std::runtime_error("server CPU sampling failed");
        worker_cpu = info.user_time.seconds + info.system_time.seconds +
                     (info.user_time.microseconds + info.system_time.microseconds) / 1000000.0;
    } else {
        worker_cpu = server_threads_cpu();
    }
    return {usage.ru_utime.tv_sec + usage.ru_stime.tv_sec +
                (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000000.0,
            worker_cpu, static_cast<std::uint64_t>(usage.ru_maxrss),
            static_cast<std::uint32_t>(events.syscalls_unix),
            static_cast<std::uint32_t>(events.csw)};
}

// The kernel keeps 32-bit cumulative tallies; modular subtraction stays exact
// across one wrap, far beyond any bounded 10-second window.
std::uint64_t event_delta(std::uint64_t after, std::uint64_t before) {
    return static_cast<std::uint32_t>(after - before);
}

// Joins on control failure as well as the normal stop command.
class Worker {
  public:
    // Starts only inside the already-forked child; the whole listener group
    // is bound before the port is reported to the parent.
    explicit Worker(int acceptors) : server_(acceptors) {
        if (server_.bind(0, &port) != 0)
            throw std::runtime_error("child listener bind failed");
        thread = std::thread([this] {
            // The single-argument overload keeps the benchmark child on the
            // API-only dispatch (no static store exists in this process).
            result_ = server_.serve(
                [](const http::ParsedMessage &message) {
                    return http::handle_request(message);
                },
                std::chrono::seconds(30), stop_);
        });
    }
    ~Worker() { finish(); }
    // Makes the thread result visible through join before returning it.
    int finish() {
        stop_.store(true);
        if (thread.joinable())
            thread.join();
        return result_;
    }
    std::thread thread;
    std::uint16_t port = 0;

  private:
    http::LoopbackServer server_;
    std::atomic_bool stop_{false};
    int result_ = -1;
};

// Implements the local B(egin), E(nd), Q(uit) protocol with acknowledged borders.
int run_child(int channel, int acceptors) {
    Worker worker(acceptors);
    transfer(channel, &worker.port, sizeof(worker.port), true);
    ServerMetrics before{};
    bool measuring = false;
    for (;;) {
        char command = 0;
        transfer(channel, &command, 1, false);
        if (command == 'Q')
            return worker.finish();
        if (command == 'B' && !measuring) {
            before = snapshot(worker.thread, acceptors);
            measuring = true;
            transfer(channel, &command, 1, true);
        } else if (command == 'E' && measuring) {
            auto metrics = snapshot(worker.thread, acceptors);
            metrics.process_cpu_seconds -= before.process_cpu_seconds;
            metrics.thread_cpu_seconds -= before.thread_cpu_seconds;
            metrics.unix_syscalls = event_delta(metrics.unix_syscalls, before.unix_syscalls);
            metrics.context_switches =
                event_delta(metrics.context_switches, before.context_switches);
            transfer(channel, &metrics, sizeof(metrics), true);
            measuring = false;
        } else {
            throw std::runtime_error("invalid server measurement transition");
        }
    }
}

// Bounds both ends of the channel and suppresses SIGPIPE on a crashed peer.
void configure_channel(int fd) {
    const timeval timeout{20, 0};
    const int on = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) != 0)
        throw std::runtime_error("server control setup failed");
}
} // namespace

ServerProcess::ServerProcess(int server_threads) : server_threads_(server_threads) {
    int pair[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0)
        throw std::runtime_error("server control socketpair failed");
    try {
        configure_channel(pair[0]);
        configure_channel(pair[1]);
    } catch (...) {
        ::close(pair[0]);
        ::close(pair[1]);
        throw;
    }
    child_ = ::fork();
    if (child_ == 0) {
        ::close(pair[0]);
        int result = 1;
        try {
            result = run_child(pair[1], server_threads_);
        } catch (const std::exception &error) {
            std::fprintf(stderr, "server child failed: %s\n", error.what());
        }
        ::close(pair[1]);
        ::_exit(result == 0 ? 0 : 1);
    }
    ::close(pair[1]);
    channel_ = pair[0];
    if (child_ < 0) {
        ::close(channel_);
        channel_ = -1;
        throw std::runtime_error("server fork failed");
    }
    try {
        transfer(channel_, &port, sizeof(port), false);
    } catch (...) {
        finish();
        throw;
    }
}

ServerProcess::~ServerProcess() { finish(); }

void ServerProcess::begin() {
    char command = 'B';
    transfer(channel_, &command, 1, true);
    transfer(channel_, &command, 1, false);
    if (command != 'B')
        throw std::runtime_error("server begin acknowledgement failed");
}

ServerMetrics ServerProcess::end() {
    char command = 'E';
    transfer(channel_, &command, 1, true);
    ServerMetrics metrics{};
    transfer(channel_, &metrics, sizeof(metrics), false);
    return metrics;
}

int ServerProcess::finish() noexcept {
    if (channel_ >= 0) {
        try {
            char command = 'Q';
            transfer(channel_, &command, 1, true);
        } catch (...) {
            result_ = 1;
        }
        ::close(channel_);
        channel_ = -1;
    }
    if (child_ > 0) {
        int status = 0;
        pid_t waited;
        do {
            waited = ::waitpid(child_, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited != child_ || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            result_ = 1;
        child_ = -1;
    }
    return result_;
}
} // namespace orange::bench
