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

// Captures cumulative process/thread CPU without instrumenting request handling.
// Task events are read first so the End sample excludes its own CPU queries.
ServerMetrics snapshot(std::thread &thread) {
    task_events_info_data_t events{};
    mach_msg_type_number_t events_count = TASK_EVENTS_INFO_COUNT;
    if (::task_info(::mach_task_self(), TASK_EVENTS_INFO, reinterpret_cast<task_info_t>(&events),
                    &events_count) != KERN_SUCCESS)
        throw std::runtime_error("server event sampling failed");
    rusage usage{};
    thread_basic_info_data_t info{};
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    if (::getrusage(RUSAGE_SELF, &usage) != 0 ||
        ::thread_info(::pthread_mach_thread_np(thread.native_handle()), THREAD_BASIC_INFO,
                      reinterpret_cast<thread_info_t>(&info), &count) != KERN_SUCCESS)
        throw std::runtime_error("server CPU sampling failed");
    return {usage.ru_utime.tv_sec + usage.ru_stime.tv_sec +
                (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000000.0,
            info.user_time.seconds + info.system_time.seconds +
                (info.user_time.microseconds + info.system_time.microseconds) / 1000000.0,
            static_cast<std::uint64_t>(usage.ru_maxrss),
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
    // Starts only inside the already-forked child.
    Worker() {
        if (server_.bind(0, &port) != 0)
            throw std::runtime_error("child listener bind failed");
        thread = std::thread([this] {
            result_ = server_.serve(http::handle_request, std::chrono::seconds(30), stop_);
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
int run_child(int channel) {
    Worker worker;
    transfer(channel, &worker.port, sizeof(worker.port), true);
    ServerMetrics before{};
    bool measuring = false;
    for (;;) {
        char command = 0;
        transfer(channel, &command, 1, false);
        if (command == 'Q')
            return worker.finish();
        if (command == 'B' && !measuring) {
            before = snapshot(worker.thread);
            measuring = true;
            transfer(channel, &command, 1, true);
        } else if (command == 'E' && measuring) {
            auto metrics = snapshot(worker.thread);
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

ServerProcess::ServerProcess() {
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
            result = run_child(pair[1]);
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
