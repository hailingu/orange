// Governed by docs/adr/ADR-0003-local-runtime-path.md.
//
// Wire-format layer of the orange loopback service: types and functions for
// HTTP/1.1 messages (ADR-0003, subtask T-1). Failures are reported as
// values, never exceptions, across this module boundary
// (docs/development/cpp-development-standard.md, Error Handling And
// Contracts).

#ifndef ORANGE_HTTP_MESSAGES_HPP
#define ORANGE_HTTP_MESSAGES_HPP

#include <optional>
#include <string_view>

namespace orange::http {

// One parsed HTTP/1.1 request. Its fields (method, target, version,
// headers, body) arrive with the parsing tasks of the T-1 ladder; the type
// exists now as the value type of parse_request.
struct Request {};

// Parses one HTTP/1.1 request from raw bytes. Returns std::nullopt for
// input that cannot be a request at all; rejected inputs and parsed fields
// grow task by task per the ADR-0003 T-1 ladder.
std::optional<Request> parse_request(std::string_view raw);

} // namespace orange::http

#endif // ORANGE_HTTP_MESSAGES_HPP
