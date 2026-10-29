// ADR: docs/adr/ADR-0010-http-server-performance-measurement.md
// Runs the measured transport in a child process with boundary-only CPU sampling.
#ifndef ORANGE_HTTP_BENCH_SERVER_HPP
#define ORANGE_HTTP_BENCH_SERVER_HPP
#include <cstdint>
#include <sys/types.h>

namespace orange::bench {
// Server-only counters bracket the issuing/drain window, excluding warmup/stop.
// The two event counts are exact whole-child-process kernel tallies: they
// include the control thread's constant boundary I/O, not per-request probes.
struct ServerMetrics {
    double process_cpu_seconds = 0;
    double thread_cpu_seconds = 0;
    std::uint64_t peak_rss_bytes = 0;
    std::uint64_t unix_syscalls = 0;
    std::uint64_t context_switches = 0;
};

// Owns a child and its bounded control channel; all socket traffic remains real.
class ServerProcess {
  public:
    // Forks before any worker thread and reports the child's ephemeral
    // listener. server_threads selects how many acceptor loops the child's
    // server serves with (1 keeps the historical single loop).
    explicit ServerProcess(int server_threads = 1);
    // Closes the channel, stops the child loop and reaps the child on every path.
    ~ServerProcess();
    ServerProcess(const ServerProcess &) = delete;
    ServerProcess &operator=(const ServerProcess &) = delete;
    // Starts server CPU accounting after the caller completes warmup.
    void begin();
    // Samples the still-live server thread after all issued responses drain.
    ServerMetrics end();
    // Requests a clean stop, reaps exactly once and returns zero only on success.
    int finish() noexcept;
    // Exposes the actual child identity for the measurement-separation contract.
    pid_t process_id() const noexcept { return child_; }
    std::uint16_t port = 0;

  private:
    int server_threads_ = 1;
    int channel_ = -1;
    pid_t child_ = -1;
    int result_ = 0;
};
} // namespace orange::bench
#endif
