// ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
#include "http_connection_store.hpp"
#include <iterator>

namespace orange::http::detail {
Connection &ConnectionStore::add(OwnedFd fd, std::chrono::steady_clock::time_point now) {
    active_.emplace_back(std::move(fd), now);
    Connection &connection = active_.back();
    connection.position = std::prev(active_.end());
    mark_dirty(connection);
    return connection;
}

void ConnectionStore::mark_dirty(Connection &connection) noexcept {
    if (connection.queued || connection.retired)
        return;
    connection.queued = true;
    connection.dirty_previous = dirty_tail_;
    if (dirty_tail_)
        dirty_tail_->dirty_next = &connection;
    else
        dirty_head_ = &connection;
    dirty_tail_ = &connection;
}

void ConnectionStore::unlink_dirty(Connection &connection) noexcept {
    if (!connection.queued)
        return;
    if (connection.dirty_previous)
        connection.dirty_previous->dirty_next = connection.dirty_next;
    else
        dirty_head_ = connection.dirty_next;
    if (connection.dirty_next)
        connection.dirty_next->dirty_previous = connection.dirty_previous;
    else
        dirty_tail_ = connection.dirty_previous;
    connection.dirty_previous = connection.dirty_next = nullptr;
    connection.queued = false;
}

Connection *ConnectionStore::take_dirty() noexcept {
    Connection *connection = dirty_head_;
    if (connection)
        unlink_dirty(*connection);
    return connection;
}

void ConnectionStore::touch(Connection &connection,
                            std::chrono::steady_clock::time_point now) noexcept {
    connection.last_activity = now;
    // Splicing the newest node to the tail is the identity relink; skip it.
    // splice(end, list, prev(end)) would additionally hit the standard's
    // position == ++i undefined case (c1 steady state touches only the tail).
    if (connection.position != std::prev(active_.end()))
        active_.splice(active_.end(), active_, connection.position);
}

void ConnectionStore::retire(Connection &connection) noexcept {
    if (connection.retired)
        return;
    connection.fd.reset();
    unlink_dirty(connection);
    connection.retired = true;
    retired_.splice(retired_.end(), active_, connection.position);
}

std::chrono::steady_clock::time_point
ConnectionStore::next_deadline(std::chrono::milliseconds idle_timeout) const noexcept {
    return active_.empty() ? std::chrono::steady_clock::time_point::max()
                           : active_.front().last_activity + idle_timeout;
}

void ConnectionStore::expire(std::chrono::steady_clock::time_point now,
                             std::chrono::milliseconds idle_timeout) noexcept {
    while (!active_.empty() && active_.front().last_activity + idle_timeout <= now)
        retire(active_.front());
}

void ConnectionStore::collect() noexcept { retired_.clear(); }
} // namespace orange::http::detail
