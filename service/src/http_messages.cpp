// Governed by docs/adr/ADR-0003-local-runtime-path.md.

#include "http_messages.hpp"

namespace orange::http {

// First behavioral contract of the T-1 ladder: input that cannot be a
// request at all (empty raw bytes) is rejected with std::nullopt. Every
// non-empty input still parses to the placeholder value; the request-line
// and later parsing tasks replace that remainder task by task.
std::optional<Request> parse_request(std::string_view raw) {
    if (raw.empty()) {
        return std::nullopt;
    }
    return Request{};
}

} // namespace orange::http
