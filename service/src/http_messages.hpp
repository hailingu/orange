// Governed by docs/adr/ADR-0003-local-runtime-path.md.
//
// Wire-format layer of the orange loopback service: types and functions for
// HTTP/1.1 messages (ADR-0003, subtask T-1). Failures are reported as
// values, never exceptions, across this module boundary
// (docs/development/cpp-development-standard.md, Error Handling And
// Contracts).

#ifndef ORANGE_HTTP_MESSAGES_HPP
#define ORANGE_HTTP_MESSAGES_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

namespace orange::http {

// The whole head — request line and header block — shares this budget:
// ADR-0003's 8 KiB header cap, answered with HTTP 431 when exceeded. Every
// line search is bounded by it, so a client that never completes a line
// cannot make the caller rescan unbounded input.
inline constexpr std::size_t header_block_cap = 8192;

// Why a parse stopped without a request. The event loop must treat the
// outcomes differently: read more bytes, answer 400, or answer 431.
enum class ParseError {
    // No line end within the received bytes and the head budget is not yet
    // exhausted: the client may still be sending.
    incomplete,
    // The bytes cannot be a request line: token structure, a bare CR or LF,
    // or a field failing its RFC 9112 grammar (sections 2.2 and 3).
    malformed,
    // No line end within the head budget, so further bytes cannot fit the
    // declared cap (HTTP 431).
    too_large,
};

// One parsed HTTP/1.1 request. The request-line fields are owned copies of
// the raw bytes, not views into the caller's buffer, because callers may
// pass transient input. Header and body fields arrive with the later
// parsing tasks of the T-1 ladder.
struct Request {
    // The request method token, containing only RFC 9110 tchar characters.
    std::string method;
    // The request target as received, containing no space or ASCII control
    // byte; validating the full origin-form grammar is a later task.
    std::string target;
    // The HTTP version token, matching `HTTP/DIGIT.DIGIT` (RFC 9112 2.5).
    // Serving-policy for unsupported versions belongs to a later layer.
    std::string version;
};

// A complete parse plus how many raw bytes it consumed — the request line,
// its CRLF, and any leading empty lines — so the caller can advance its
// buffer to the next message.
struct ParsedRequest {
    Request request;
    std::size_t consumed = 0;
};

// Parses one HTTP/1.1 request from raw bytes, skipping leading empty lines
// first (RFC 9112 2.2: a server SHOULD ignore them). The request line must
// be `method SP request-target SP HTTP-version CRLF` with three non-empty
// tokens, each passing its field validation (see Request). Returns
// ParseError::incomplete when no line end has arrived within the budget,
// ParseError::too_large when the budget is exhausted without one, and
// ParseError::malformed for every grammar or field violation, including a
// bare CR inside the line and an LF not preceded by CR. Bytes after the
// request line are not consumed yet — header and body parsing are later
// tasks of the ADR-0003 T-1 ladder.
std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw);

} // namespace orange::http

#endif // ORANGE_HTTP_MESSAGES_HPP
