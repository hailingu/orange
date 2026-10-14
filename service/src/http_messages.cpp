// Governed by docs/adr/ADR-0003-local-runtime-path.md.

#include "http_messages.hpp"

#include <string_view>

namespace orange::http {

// Placeholder body so the suite compiles against the new contract: every
// input is reported incomplete. The classifying implementation lands with
// the change that follows this failing-first suite.
std::variant<ParsedRequest, ParseError> parse_request(std::string_view /*raw*/) {
    return ParseError::incomplete;
}

} // namespace orange::http
