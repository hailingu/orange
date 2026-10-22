// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Response framing of the orange loopback service (ADR-0003, subtask T-1):
// one value describes a complete HTTP/1.1 response and write_response
// serializes it to owned wire bytes with Content-Length or chunked framing,
// including the Connection: close rejection responses the transport owes
// after a 431/413.
#ifndef ORANGE_HTTP_RESPONSE_HPP
#define ORANGE_HTTP_RESPONSE_HPP
#include "http_messages.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <variant>
namespace orange::http {

// How the body reaches the wire. `none` sends no framing field and no body;
// `content_length` emits `Content-Length` with the body inline; `chunked`
// emits `Transfer-Encoding: chunked` and the body as one size-prefixed chunk
// plus the last chunk (RFC 9112 6, 7.1).
enum class BodyFraming { none, content_length, chunked };

// One caller-assembled response field. Both views borrow caller storage and
// stay valid only for the write_response call that serializes them.
struct ResponseField {
    std::string_view name;
    std::string_view value;
};

// A complete response description. The encoder owns the status line, the
// framing fields, and the Connection field; caller fields must not supply
// those names in any case and are rejected with conflicting_field so two
// framings can never reach the wire together.
struct Response {
    // Any status code within the RFC 9110 15.1 valid range 100..599.
    unsigned int status = 200;
    BodyFraming framing = BodyFraming::none;
    // The response body; must be empty when framing is `none`.
    std::string_view body;
    // Emitted in order after the status line; duplicates stay separate.
    std::span<const ResponseField> fields;
    // Appends `Connection: close` (RFC 9110 7.6.1) after the framing field;
    // the event loop owns closing the connection after these bytes.
    bool close = false;
};

// Why a response could not be serialized. The caller owns the input; every
// value except resource_exhausted marks a caller defect the encoder refuses
// to place on the wire.
enum class ResponseError {
    // Status outside the RFC 9110 15.1 range 100..599.
    invalid_status,
    // A 1xx, 204, or 304 response carries a body or framing field
    // (RFC 9110 15.2, 15.3.5, 15.4.5).
    body_forbidden_status,
    // A field name is not an RFC 9110 5.6.2 token.
    invalid_field_name,
    // A field value holds an octet outside field-content/OWS, or
    // leading/trailing whitespace the wire would silently change.
    invalid_field_value,
    // A caller field duplicates an encoder-owned name: content-length,
    // transfer-encoding, or connection, in any case.
    conflicting_field,
    // Framing is `none` but the body is not empty.
    unframed_body,
    // Owned wire bytes could not be allocated.
    resource_exhausted,
};

// Serializes one complete response to owned wire bytes: the fixed-HTTP/1.1
// status line (RFC 9112 4) with an RFC 9110 15 reason phrase — empty for an
// unknown code, keeping the mandatory SP — the caller fields in order, the
// encoder-owned framing field, the optional Connection: close, and the
// framed body. A chunked body is binary-safe: one lowercase-hex size line,
// the raw bytes including CR/LF, and the `0\r\n\r\n` last chunk; an empty
// chunked body is the last chunk alone. Allocation failure returns
// resource_exhausted instead of throwing across this boundary.
[[nodiscard]] std::variant<std::string, ResponseError> write_response(const Response &response);

// The precomputed closing wire for a terminal parse error: byte-identical to
// write_response(error_response(error)) — status line, Connection: close, and
// the terminating empty line — but held in static storage, so a rejection
// never allocates. The event loop appends these bytes and closes after the
// flush; the response suite pins the equality for every ParseError value.
[[nodiscard]] std::string_view error_wire(ParseError error);

// Maps one terminal parse outcome to the rejection response the connection
// owes: malformed→400 (RFC 9112 2.2), method_too_long and
// unsupported_transfer_coding→501, target_too_long→414,
// headers_too_long→431 (RFC 6585 5), body_too_long and
// chunk_metadata_too_long→413, resource_exhausted→500. Every result carries
// Connection: close with no body and no framing field: after a terminal
// error the message boundary is not reusable, so the caller completes this
// response and closes. Passing incomplete is a caller defect answered
// defensively as 400; the event loop feeds the decoder until a terminal
// outcome instead.
[[nodiscard]] Response error_response(ParseError error);

} // namespace orange::http
#endif // ORANGE_HTTP_RESPONSE_HPP
