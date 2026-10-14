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
#include <vector>

namespace orange::http {

// The whole head — request line and header block — shares this budget:
// ADR-0003's 8 KiB header cap. Every line search is bounded by it, so a
// client that never completes a line cannot make the caller rescan
// unbounded input. A request line that cannot end inside the budget is
// classified per RFC 9112 section 3 (see ParseError); a header block that
// cannot finish inside the budget that remains after the request line
// answers 431 (ParseError::headers_too_long).
inline constexpr std::size_t header_block_cap = 8192;
// ADR-0003 bounds decoded request content, excluding chunk metadata, to 1 MiB.
inline constexpr std::size_t request_body_cap = 1024 * 1024;

// Why a parse stopped without a request. The event loop must treat the
// outcomes differently: read more bytes, or answer the noted status.
enum class ParseError {
    // No line end within the received bytes and the head budget is not yet
    // exhausted: the client may still be sending.
    incomplete,
    // The bytes cannot be a request message (HTTP 400, RFC 9112 2.2): token
    // structure, a bare CR or LF, a field failing its grammar, an obsolete
    // line folding, empty lines alone exhausting the head budget, ambiguous
    // body framing, or invalid chunk metadata/delimiters.
    malformed,
    // The request line cannot end inside the head budget and the method is
    // the longer of method and target (HTTP 501, RFC 9112 3 SHOULD).
    method_too_long,
    // The request line cannot end inside the head budget and the target is
    // at least as long as the method (HTTP 414, RFC 9112 3 MUST).
    target_too_long,
    // The header block after a parsed request line cannot finish inside the
    // head budget that remains (HTTP 431, RFC 6585 section 5): the head is
    // too large to ever complete under the cap.
    headers_too_long,
    // Decoded request content exceeds ADR-0003's 1 MiB cap (HTTP 413).
    body_too_long,
    // A valid transfer coding preceding chunked is not supported (HTTP 501).
    unsupported_transfer_coding,
    // An owned decoder buffer could not be allocated (HTTP 500).
    resource_exhausted,
};

// One header field line as received: `name: value`. The name is a token
// (RFC 9110 5.6.2) preserved with the case the client sent — comparison is
// the caller's case-insensitive job — and the value is field-content
// trimmed of its optional surrounding whitespace (RFC 9110 5.5), where
// obs-text octets are allowed. Duplicate names stay separate entries:
// combining them is per-field policy (RFC 9110 5.2), not the wire parser's.
struct HeaderField {
    std::string name;
    std::string value;
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
// parse-to-response window by construction. ParsedMessage in
// http_request_decoder.hpp owns the decoded body separately.
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
    // The header fields in received order, owned copies under the same
    // lifetime contract as the other fields.
    std::vector<HeaderField> headers;
};

// A complete parse plus how many raw bytes it consumed — leading empty
// lines, the request line with its CRLF, the header block, and its
// terminating empty line — so the caller can advance its buffer to the
// next message.
struct ParsedRequest {
    Request request;
    std::size_t consumed = 0;
};

// Parses one HTTP/1.1 request head from raw bytes: leading empty lines are
// skipped first (RFC 9112 2.2: a server SHOULD ignore them), the request
// line must be `method SP request-target SP HTTP-version CRLF` with three
// non-empty tokens each passing its field validation (see Request), and the
// header block after it is zero or more CRLF-terminated `field-name: OWS
// field-value OWS` lines (RFC 9112 5) ended by one empty line. Field names
// are tokens; values are trimmed of optional whitespace and may contain
// obs-text octets (RFC 9110 5.5); obsolete line folding is rejected
// (RFC 9112 5.2). Returns ParseError::incomplete while the head may still
// arrive inside header_block_cap, ParseError::method_too_long or
// ParseError::target_too_long when the request line alone exhausts the
// budget (RFC 9112 3), ParseError::headers_too_long when the header block
// cannot finish inside the budget remaining after the request line
// (RFC 6585 5), and ParseError::malformed for every grammar violation,
// including a bare CR inside a line and an LF not preceded by CR. Only the
// head is consumed. parse_message and RequestDecoder in
// http_request_decoder.hpp also decode Content-Length and chunked bodies.
[[nodiscard]] std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw);

} // namespace orange::http

#endif // ORANGE_HTTP_MESSAGES_HPP
