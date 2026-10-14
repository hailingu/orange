// Governed by docs/adr/ADR-0003-local-runtime-path.md.

#include "http_messages.hpp"

namespace orange::http {

// Placeholder body: returns a value for every input so the library compiles
// and links. The first behavioral contract (empty input is rejected) lands
// failing-first as the next task of the T-1 ladder.
std::optional<Request> parse_request(std::string_view /*raw*/) { return Request{}; }

} // namespace orange::http
