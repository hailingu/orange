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
#include <string>
#include <string_view>

namespace orange::http {

// One parsed HTTP/1.1 request. The request-line fields are owned copies of
// the raw bytes, not views into the caller's buffer, because callers may
// pass transient input. Header and body fields arrive with the later
// parsing tasks of the T-1 ladder.
struct Request {
    // The request method token, for example "GET" or "POST".
    std::string method;
    // The request target as received, for example "/" or "/api/echo".
    std::string target;
    // The HTTP version token from the request line, for example "HTTP/1.1".
    std::string version;
};

// Parses one HTTP/1.1 request from raw bytes. The request line must be
// `method SP request-target SP HTTP-version CRLF` with three non-empty
// tokens; empty input and every input without that CRLF-terminated shape
// return std::nullopt. Header and body parsing arrive with the later tasks
// of the ADR-0003 T-1 ladder; bytes after the request line are not consumed
// yet.
std::optional<Request> parse_request(std::string_view raw);

} // namespace orange::http

#endif // ORANGE_HTTP_MESSAGES_HPP
