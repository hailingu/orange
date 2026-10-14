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
// ADR-0003's 8 KiB header cap. Every line search is bounded by it, so a
// client that never completes a line cannot make the caller rescan
// unbounded input. A request line that cannot end inside the budget is
// classified per RFC 9112 section 3 (see ParseError); the header-field
// overflow outcome (431) arrives with header parsing, a later T-1 task.
inline constexpr std::size_t header_block_cap = 8192;

// Why a parse stopped without a request. The event loop must treat the
// outcomes differently: read more bytes, or answer the noted status.
enum class ParseError {
    // No line end within the received bytes and the head budget is not yet
    // exhausted: the client may still be sending.
    incomplete,
    // The bytes cannot be a request line (HTTP 400, RFC 9112 2.2): token
    // structure, a bare CR or LF, a field failing its grammar, or empty
    // lines alone exhausting the head budget.
    malformed,
    // The request line cannot end inside the head budget and the method is
    // the longer of method and target (HTTP 501, RFC 9112 3 SHOULD).
    method_too_long,
    // The request line cannot end inside the head budget and the target is
    // at least as long as the method (HTTP 414, RFC 9112 3 MUST).
    target_too_long,
};

// One parsed HTTP/1.1 request. The request-line fields are owned copies of
// the raw bytes, not views into the caller's buffer, because callers may
// pass transient input. That is a deliberate lifetime contract: borrowed
// std::string_view fields would hand lifetime control to the caller and
// dangle silently — a temporary buffer destroyed after the call, or a read
// buffer reused by the next cycle, yields undefined behavior with no
// compile-time or run-time diagnostic (the measured saving is ~10-25 ns per
// request; see ADR-0003's T-1 evidence for the benchmark). Revisit only if
// a later event loop guarantees a stable read buffer for the whole
// parse-to-response window by construction. Header and body fields arrive
// with the later parsing tasks of the T-1 ladder.
struct Request {
    // The request method token, containing only RFC 9110 tchar characters.
    std::string method;
    // The request target as received, built only from URI characters with
    // well-formed percent-encoding (RFC 9112 3.2, RFC 3986 2.1-2.3).
    // Distinguishing the four request-target forms belongs to dispatch.
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
// ParseError::method_too_long or ParseError::target_too_long when the budget
// is exhausted by a grammar-valid line prefix, and ParseError::malformed for
// every grammar or field violation, including a bare CR inside the line and
// an LF not preceded by CR. Bytes after the request line are not consumed
// yet — header and body parsing are later tasks of the ADR-0003 T-1 ladder.
[[nodiscard]] std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw);

} // namespace orange::http

#endif // ORANGE_HTTP_MESSAGES_HPP
