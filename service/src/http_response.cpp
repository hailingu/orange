// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Serializer side of the T-1 transport contract: validates the caller's
// response description, then frames it once into owned wire bytes.
#include "http_response.hpp"
#include "http_syntax.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace orange::http {
namespace {

// The server speaks one version; request-version policy belongs to dispatch.
constexpr std::string_view status_version = "HTTP/1.1";

struct ReasonPhrase {
    unsigned int status;
    std::string_view phrase;
};
// Reason phrases for the RFC 9110 15 registry entries this slice can answer;
// 431 comes from RFC 6585 5. Unknown codes serialize with an empty phrase.
constexpr ReasonPhrase reason_phrases[] = {
    {100, "Continue"},
    {200, "OK"},
    {204, "No Content"},
    {206, "Partial Content"},
    {304, "Not Modified"},
    {400, "Bad Request"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {413, "Content Too Large"},
    {414, "URI Too Long"},
    {416, "Range Not Satisfiable"},
    {431, "Request Header Fields Too Large"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {505, "HTTP Version Not Supported"},
};

// The registered phrase for a validated status, or empty; RFC 9112 4 keeps
// the SP after the code, so an empty reason phrase stays explicit.
std::string_view reason_phrase(unsigned int status) {
    for (const auto entry : reason_phrases) {
        if (entry.status == status)
            return entry.phrase;
    }
    return {};
}

// 1xx, 204, and 304 responses end at the header section (RFC 9110 15.2,
// 15.3.5, 15.4.5): no body or framing field may follow.
bool body_forbidden(unsigned int status) { return status < 200 || status == 204 || status == 304; }

// Field names whose wire form the encoder owns, so a response can never
// carry two framings or two connection policies (RFC 9112 6.1, 7.1).
bool encoder_owned(std::string_view name) {
    return detail::iequals(name, "content-length") || detail::iequals(name, "transfer-encoding") ||
           detail::iequals(name, "connection");
}

// Field-content octets only, without leading/trailing OWS the wire would
// silently widen into the field value.
bool valid_value(std::string_view value) {
    const auto ows = [](char c) { return c == ' ' || c == '\t'; };
    if (!value.empty() && (ows(value.front()) || ows(value.back())))
        return false;
    return std::all_of(value.begin(), value.end(), detail::is_field_octet);
}

// Every caller defect the serializer refuses to place on the wire, in the
// documented order: status, the Content-Range status pairing,
// body-forbidden statuses, each field's name, ownership and value, then the
// unframed-body mismatch.
std::optional<ResponseError> validate(const Response &response) {
    if (response.status < 100 || response.status > 599)
        return ResponseError::invalid_status;
    if (response.content_range.has_value() && response.status != 206 && response.status != 416)
        return ResponseError::invalid_content_range;
    if (body_forbidden(response.status) &&
        (response.framing != BodyFraming::none || !response.body.empty()))
        return ResponseError::body_forbidden_status;
    for (const ResponseField &field : response.fields) {
        if (!detail::is_token(field.name))
            return ResponseError::invalid_field_name;
        if (encoder_owned(field.name))
            return ResponseError::conflicting_field;
        if (!valid_value(field.value))
            return ResponseError::invalid_field_value;
    }
    if (response.framing == BodyFraming::none && !response.body.empty())
        return ResponseError::unframed_body;
    return std::nullopt;
}

// Digits of value in the given base, without formatting it.
std::size_t digit_count(std::size_t value, std::size_t base) {
    std::size_t digits = 1;
    while (value >= base) {
        value /= base;
        ++digits;
    }
    return digits;
}

// Wire length of one complete response, so assembly allocates once; the
// fixed sizes are the encoder-owned field lines including name, colon+SP,
// value and CRLF.
std::size_t wire_size(const Response &response) {
    std::size_t total =
        status_version.size() + 1 + 3 + 1 + reason_phrase(response.status).size() + 2;
    for (const ResponseField &field : response.fields)
        total += field.name.size() + 2 + field.value.size() + 2;
    if (response.content_range.has_value()) {
        // "Content-Range: bytes " + the "*/" (or '-' and '/') separators +
        // up to three 20-digit numbers + CRLF, exactly for both forms.
        const ContentRange &range = *response.content_range;
        total += 15 + 6 + 2 + digit_count(range.total, 10) + 2;
        if (range.satisfied)
            total += digit_count(range.first, 10) + digit_count(range.last, 10);
    }
    if (response.framing == BodyFraming::content_length)
        total += 16 + digit_count(response.body.size(), 10) + 2;
    if (response.framing == BodyFraming::chunked)
        total += 26 + 2;
    if (response.close)
        total += 17 + 2;
    total += 2;
    if (response.framing == BodyFraming::chunked) {
        if (!response.body.empty())
            total += digit_count(response.body.size(), 16) + 2 + response.body.size() + 2;
        total += 5;
    } else {
        total += response.body.size();
    }
    return total;
}

// Appends value in the given base; 24 bytes hold any size_t in base 10
// (20 digits) or base 16 (16 digits). Base 16 emits lowercase hex digits.
void append_number(std::string &out, std::size_t value, int base) {
    char buffer[24];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, base);
    out.append(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

// Assembles the validated response: status line, caller fields in order,
// the encoder-formatted Content-Range, the encoder-owned framing field, the
// optional Connection: close, the terminating empty line, then the framed
// body.
void serialize(const Response &response, std::string &out) {
    out.append(status_version).push_back(' ');
    append_number(out, response.status, 10);
    out.push_back(' ');
    out.append(reason_phrase(response.status)).append("\r\n");
    for (const ResponseField &field : response.fields)
        out.append(field.name).append(": ").append(field.value).append("\r\n");
    if (response.content_range.has_value()) {
        const ContentRange &range = *response.content_range;
        out.append("Content-Range: bytes ");
        if (range.satisfied) {
            append_number(out, range.first, 10);
            out.push_back('-');
            append_number(out, range.last, 10);
            out.push_back('/');
        } else {
            out.append("*/");
        }
        append_number(out, range.total, 10);
        out.append("\r\n");
    }
    if (response.framing == BodyFraming::content_length) {
        out.append("Content-Length: ");
        append_number(out, response.body.size(), 10);
        out.append("\r\n");
    } else if (response.framing == BodyFraming::chunked) {
        out.append("Transfer-Encoding: chunked\r\n");
    }
    if (response.close)
        out.append("Connection: close\r\n");
    out.append("\r\n");
    if (response.framing == BodyFraming::chunked) {
        if (!response.body.empty()) {
            append_number(out, response.body.size(), 16);
            out.append("\r\n").append(response.body).append("\r\n");
        }
        out.append("0\r\n\r\n");
    } else {
        out.append(response.body);
    }
}

// Precomputed closing wires: the exact bytes write_response produces for
// error_response of the matching parse errors, without allocating per
// rejection. check_error_wires pins them against the serializer.
constexpr std::string_view bad_request_wire =
    "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
constexpr std::string_view content_too_large_wire =
    "HTTP/1.1 413 Content Too Large\r\nConnection: close\r\n\r\n";
constexpr std::string_view uri_too_long_wire =
    "HTTP/1.1 414 URI Too Long\r\nConnection: close\r\n\r\n";
constexpr std::string_view headers_too_large_wire =
    "HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\n\r\n";
constexpr std::string_view internal_error_wire =
    "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n";
constexpr std::string_view not_implemented_wire =
    "HTTP/1.1 501 Not Implemented\r\nConnection: close\r\n\r\n";

} // namespace

std::string_view error_wire(ParseError error) {
    switch (error) {
    case ParseError::malformed:
    case ParseError::incomplete:
        return bad_request_wire;
    case ParseError::method_too_long:
    case ParseError::unsupported_transfer_coding:
        return not_implemented_wire;
    case ParseError::target_too_long:
        return uri_too_long_wire;
    case ParseError::headers_too_long:
        return headers_too_large_wire;
    case ParseError::body_too_long:
    case ParseError::chunk_metadata_too_long:
        return content_too_large_wire;
    case ParseError::resource_exhausted:
        return internal_error_wire;
    }
    return bad_request_wire;
}

std::variant<std::string, ResponseError> write_response(const Response &response) {
    std::string out;
    if (const auto failed = write_response_into(response, out)) {
        return *failed;
    }
    return out;
}

std::optional<ResponseError> write_response_into(const Response &response, std::string &out) {
    if (const auto invalid = validate(response)) {
        return *invalid;
    }
    try {
        // Reserve once against the precomputed wire size: the serialize pass
        // then appends without reallocating, and a reserve throw leaves the
        // caller's buffer untouched.
        out.reserve(out.size() + wire_size(response));
        serialize(response, out);
    } catch (const std::bad_alloc &) {
        return ResponseError::resource_exhausted;
    }
    return std::nullopt;
}

// The rejection each terminal parse error owes the connection. The status
// for every value is fixed by the ParseError contract comments; see
// http_messages.hpp for the RFC anchors.
Response error_response(ParseError error) {
    Response response;
    response.close = true;
    switch (error) {
    case ParseError::method_too_long:
    case ParseError::unsupported_transfer_coding:
        response.status = 501;
        break;
    case ParseError::target_too_long:
        response.status = 414;
        break;
    case ParseError::headers_too_long:
        response.status = 431;
        break;
    case ParseError::body_too_long:
    case ParseError::chunk_metadata_too_long:
        response.status = 413;
        break;
    case ParseError::resource_exhausted:
        response.status = 500;
        break;
    case ParseError::malformed:
    case ParseError::incomplete:
        // incomplete is a caller defect here: the event loop feeds the
        // decoder until a terminal outcome, so this branch answers as
        // malformed rather than inventing a response for a pending read.
        response.status = 400;
        break;
    }
    return response;
}

} // namespace orange::http
