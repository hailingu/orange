// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
// Storage authorization: docs/reviews/2026-10-16-owned-request-storage.md.
//
// Wire-format layer of the orange loopback service: types and functions for
// HTTP/1.1 messages (ADR-0003, subtask T-1). Parser entry points report coded
// errors. Ordinary Request value copies can throw bad_alloc; a proposed narrow
// value-copy exception is recorded in
// service/docs/adr/ADR-0004-owned-request-storage.md (Engineering Exceptions).

#ifndef ORANGE_HTTP_MESSAGES_HPP
#define ORANGE_HTTP_MESSAGES_HPP

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace orange::http {

namespace detail {
// Validated slices fit the 8 KiB head cap; each wire field needs four bytes.
struct HeaderFieldOffsets {
    std::uint16_t name_begin;
    std::uint16_t name_size;
    std::uint16_t value_begin;
    std::uint16_t value_size;
};
// The version has exactly eight octets after request-line validation.
struct RequestLineOffsets {
    std::uint16_t method_begin;
    std::uint16_t method_size;
    std::uint16_t target_begin;
    std::uint16_t target_size;
    std::uint16_t version_begin;
};
// Incremental validation is the other trusted constructor of request storage.
class HeadFields;
} // namespace detail

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
    // Aggregate chunk framing and discarded trailers exceed the independent
    // chunk_metadata_cap wire budget (HTTP 413; close the connection).
    chunk_metadata_too_long,
    // A valid transfer coding preceding chunked is not supported (HTTP 501).
    unsupported_transfer_coding,
    // Owned request storage or a decoder buffer could not be allocated (HTTP 500).
    resource_exhausted,
};

// One header field line as received: `name: value`. The name is a token
// (RFC 9110 5.6.2) preserved with the case the client sent — comparison is
// the caller's case-insensitive job — and the value is field-content
// trimmed of its optional surrounding whitespace (RFC 9110 5.5), where
// obs-text octets are allowed. Duplicate names stay separate entries:
// combining them is per-field policy (RFC 9110 5.2), not the wire parser's.
// Views borrow from the owning Request, not from caller input; they expire
// when that Request is destroyed, assigned, or moved from.
struct HeaderField {
    std::string_view name;
    std::string_view value;
};

// Forward declaration for the parser's validated construction boundary.
struct ParsedRequest;

// Owns request characters independently of input and decoder lifetimes. Offsets,
// rather than saved views, survive both short-string and heap-backed moves.
// Accessor views borrow from this value and expire on assignment/destruction or
// moving from it. Copies own independent storage; moved-from values are empty.
// ParsedMessage owns the decoded body separately. Mutating wire fields is not
// part of this parser value's API.
class Request {
  public:
    // An empty value supports containers, decoder reset and move destinations.
    Request() = default;
    // Copies own independent character and descriptor storage.
    Request(const Request &) = default;
    // Commits a copy only after both allocations succeed.
    Request &operator=(const Request &other);
    // Transfers ownership and leaves the source safely empty.
    Request(Request &&other) noexcept;
    // Replaces storage and leaves the source safely empty; self-move is inert.
    Request &operator=(Request &&other) noexcept;
    // Standard containers release all storage when the value dies.
    ~Request() = default;
    // Returns the validated RFC 9110 method token, or empty for an empty value.
    [[nodiscard]] std::string_view method() const noexcept {
        return {head_.data() + line_.method_begin, line_.method_size};
    }
    // Returns the raw URI target with validated percent-encoding.
    [[nodiscard]] std::string_view target() const noexcept {
        return {head_.data() + line_.target_begin, line_.target_size};
    }
    // Returns HTTP/DIGIT.DIGIT; serving policy belongs to the caller.
    [[nodiscard]] std::string_view version() const noexcept {
        return {head_.data() + line_.version_begin, head_.empty() ? 0U : 8U};
    }
    // Provides sized/indexable ordered fields without per-field strings or
    // allocations. The range and fields borrow from this Request; iterators
    // additionally require the returned range object to remain alive.
    [[nodiscard]] auto headers() const noexcept {
        return std::span(fields_) | std::views::transform([bytes = std::string_view(head_)](
                                                              detail::HeaderFieldOffsets field) {
                   return HeaderField{{bytes.data() + field.name_begin, field.name_size},
                                      {bytes.data() + field.value_begin, field.value_size}};
               });
    }

  private:
    friend class detail::HeadFields;
    friend std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw);
    // Only the parser's validated, capped offsets may construct owned storage.
    Request(std::string_view head, detail::RequestLineOffsets line,
            std::span<const detail::HeaderFieldOffsets> fields);
    // Appends one validated slice and rewrites its offset into compact storage.
    void append_slice(std::string_view source, std::uint16_t &begin, std::uint16_t size);
    // Exchanges the entire coupled representation for strong copy assignment.
    void swap(Request &other) noexcept;
    std::string head_;
    std::vector<detail::HeaderFieldOffsets> fields_;
    detail::RequestLineOffsets line_{};
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
// Owned-storage allocation failure returns ParseError::resource_exhausted.
[[nodiscard]] std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw);

} // namespace orange::http

#endif // ORANGE_HTTP_MESSAGES_HPP
