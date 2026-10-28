// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
// Internal transport ownership: stable identities, dirty work and ordered expiry.
#ifndef ORANGE_HTTP_CONNECTION_STORE_HPP
#define ORANGE_HTTP_CONNECTION_STORE_HPP
#include "http_request_decoder.hpp"
#include <chrono>
#include <list>
#include <string>
#include <unistd.h>
#include <utility>

namespace orange::http::detail {
// Owns a POSIX descriptor; invalidation immediately prevents old-batch dispatch.
class OwnedFd {
  public:
    // Empty descriptors own no resource.
    OwnedFd() noexcept = default;
    // Adopts one descriptor returned by the operating system.
    explicit OwnedFd(int fd) noexcept : fd_(fd) {}
    // Releases the adopted descriptor once.
    ~OwnedFd() { reset(); }
    // Transfers ownership without closing the transferred descriptor.
    OwnedFd(OwnedFd &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    // Releases the old descriptor and adopts the source's ownership.
    OwnedFd &operator=(OwnedFd &&other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    OwnedFd(const OwnedFd &) = delete;
    OwnedFd &operator=(const OwnedFd &) = delete;
    // Borrows the descriptor for a synchronous syscall.
    int get() const noexcept { return fd_; }
    // Reports whether dispatch may still use the resource.
    bool valid() const noexcept { return fd_ >= 0; }
    // Transfers ownership to the listener owner after successful bind.
    int release() noexcept { return std::exchange(fd_, -1); }
    // Closes immediately; repeated invalidation is harmless.
    void reset() noexcept {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = -1;
    }

  private:
    int fd_ = -1;
};

// Tracks kernel registration so only actual enabled-state transitions are sent.
enum class Interest { absent, enabled, disabled };

// Stable connection state remains alive through all events in its closing batch.
struct Connection {
    // Constructs the nonmovable decoder directly inside its final owning node.
    Connection(OwnedFd descriptor, std::chrono::steady_clock::time_point now)
        : fd(std::move(descriptor)), last_activity(now) {}
    OwnedFd fd;
    RequestDecoder decoder;
    std::string outgoing;
    std::size_t outgoing_offset = 0;
    bool closing_after_flush = false;
    bool peer_eof = false;
    Interest read_interest = Interest::absent;
    Interest write_interest = Interest::absent;
    std::chrono::steady_clock::time_point last_activity{};

  private:
    friend class ConnectionStore;
    std::list<Connection>::iterator position;
    Connection *dirty_previous = nullptr;
    Connection *dirty_next = nullptr;
    bool queued = false;
    bool retired = false;
};

// Owns connection nodes in activity order. No operation scans live connections;
// expire/collect visit only due/closed nodes. All calls belong to the loop thread.
class ConnectionStore {
  public:
    // Adopts a connection, initially dirty; allocation failure retains RAII cleanup.
    Connection &add(OwnedFd fd, std::chrono::steady_clock::time_point now);
    // Queues at most one pending interest update for a live connection.
    void mark_dirty(Connection &connection) noexcept;
    // Removes the next pending update, or returns null when no state changed.
    Connection *take_dirty() noexcept;
    // Records successful I/O in monotonically sampled activity order, without allocation.
    void touch(Connection &connection, std::chrono::steady_clock::time_point now) noexcept;
    // Closes/unlinks immediately but defers destruction until collect at batch end.
    void retire(Connection &connection) noexcept;
    // Returns the oldest live deadline, or max when there is no live connection.
    std::chrono::steady_clock::time_point
    next_deadline(std::chrono::milliseconds idle_timeout) const noexcept;
    // Retires only the consecutive oldest nodes whose deadline has arrived.
    void expire(std::chrono::steady_clock::time_point now,
                std::chrono::milliseconds idle_timeout) noexcept;
    // Destroys only retired nodes after both event passes have finished.
    void collect() noexcept;

  private:
    void unlink_dirty(Connection &connection) noexcept;
    std::list<Connection> active_;
    std::list<Connection> retired_;
    Connection *dirty_head_ = nullptr;
    Connection *dirty_tail_ = nullptr;
};
} // namespace orange::http::detail
#endif
