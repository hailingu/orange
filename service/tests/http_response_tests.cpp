// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Pins the write_response framing contract: exact wire bytes for
// Content-Length and chunked bodies, Connection: close rejection responses,
// the reason-phrase table, typed rejections of invalid input, and the
// no-throw allocation-failure channel.
#include "http_response.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace {
bool fail_next_allocation = false;
}
// Injects one ordinary allocation failure at the public encoder boundary;
// this is fault injection, not a count of all allocation forms.
void *operator new(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc{};
    }
    if (void *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}
// Releases allocations made by this executable's fault-injection replacement.
void operator delete(void *memory) noexcept { std::free(memory); }

namespace {
using orange::http::BodyFraming;
using orange::http::Response;
using orange::http::ResponseError;
using orange::http::ResponseField;

int failures = 0;
std::size_t checks = 0;
// Gives CTest a nonzero exit for any observable wire or rejection mismatch.
void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}
// Response probes name every member, so no initializer is silently missing.
Response plain(unsigned int status, BodyFraming framing, std::string_view body,
               bool close = false) {
    return Response{status, framing, body, {}, close};
}
Response with_fields(unsigned int status, BodyFraming framing, std::string_view body,
                     std::span<const ResponseField> fields, bool close = false) {
    return Response{status, framing, body, fields, close};
}
// Owned wire bytes when the input serializes; nullopt when it is rejected.
std::optional<std::string> wire_bytes(const Response &response) {
    auto result = orange::http::write_response(response);
    if (const auto *bytes = std::get_if<std::string>(&result))
        return std::move(*bytes);
    return std::nullopt;
}
// The typed rejection when the input is invalid; nullopt when it serializes.
std::optional<ResponseError> wire_error(const Response &response) {
    auto result = orange::http::write_response(response);
    if (const auto *error = std::get_if<ResponseError>(&result))
        return *error;
    return std::nullopt;
}

// Exact one-shot wire bytes for the framing shapes the declared routes use.
void check_wire_shapes() {
    const std::array<ResponseField, 1> json_type{{{"Content-Type", "application/json"}}};
    check(wire_bytes(with_fields(200, BodyFraming::content_length, "{\"status\":\"ok\"}",
                                 json_type)) == "HTTP/1.1 200 OK\r\n"
                                                "Content-Type: application/json\r\n"
                                                "Content-Length: 15\r\n"
                                                "\r\n"
                                                "{\"status\":\"ok\"}",
          "health shape serializes fields, framing and body exactly");
    check(wire_bytes(plain(200, BodyFraming::none, "")) == "HTTP/1.1 200 OK\r\n\r\n",
          "a bare response is the status line and empty line only");
    check(wire_bytes(plain(200, BodyFraming::content_length, "")) ==
              "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",
          "an empty content_length body declares Content-Length: 0");
    check(wire_bytes(plain(200, BodyFraming::chunked, "hello")) ==
              "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
          "a chunked body is one lowercase-hex chunk plus the last chunk");
    check(wire_bytes(plain(200, BodyFraming::chunked, "")) ==
              "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
          "an empty chunked body is only the last chunk");
    check(wire_bytes(plain(200, BodyFraming::chunked, "he\r\nllo")) ==
              "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n7\r\nhe\r\nllo\r\n0\r\n\r\n",
          "chunk data may carry CR and LF bytes verbatim");
    const std::string filled(2748, 'x');
    check(wire_bytes(plain(200, BodyFraming::chunked, filled)) ==
              "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nabc\r\n" + filled +
                  "\r\n0\r\n\r\n",
          "a 2748-byte body sizes its chunk with lowercase hex abc");
    const std::string mebi(1024 * 1024, 'x');
    const auto mebi_wire = wire_bytes(plain(200, BodyFraming::chunked, mebi));
    const auto head_end = mebi_wire ? mebi_wire->find("\r\n\r\n") : std::string::npos;
    check(mebi_wire && head_end != std::string::npos &&
              mebi_wire->substr(head_end + 4, 8) == "100000\r\n" &&
              mebi_wire->substr(head_end + 12, mebi.size()) == mebi &&
              mebi_wire->size() == head_end + 12 + mebi.size() + 2 + 5 &&
              mebi_wire->substr(mebi_wire->size() - 5) == "0\r\n\r\n",
          "a 1 MiB chunked body frames size 100000, raw bytes and last chunk");
    check(wire_bytes(plain(431, BodyFraming::none, "", true)) ==
              "HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\n\r\n",
          "a 431 rejection closes the connection");
    check(wire_bytes(plain(413, BodyFraming::content_length, "rejected", true)) ==
              "HTTP/1.1 413 Content Too Large\r\nContent-Length: 8\r\n"
              "Connection: close\r\n\r\nrejected",
          "close follows the framing field before the body");
    const std::array<ResponseField, 3> repeated{
        {{"X-A", "1"}, {"X-B", std::string_view("caf\xC3\xA9", 5)}, {"X-A", "2"}}};
    check(wire_bytes(with_fields(200, BodyFraming::none, "", repeated)) ==
              "HTTP/1.1 200 OK\r\nX-A: 1\r\nX-B: caf\xC3\xA9\r\nX-A: 2\r\n\r\n",
          "fields keep caller order and duplicates; obs-text passes");
}

// The status-line reason table and the mandatory SP for unknown codes.
void check_reason_phrases() {
    const struct {
        unsigned int status;
        std::string_view phrase;
    } known[] = {{100, "Continue"},
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
                 {505, "HTTP Version Not Supported"}};
    for (const auto &entry : known) {
        const auto bytes = wire_bytes(plain(entry.status, BodyFraming::none, ""));
        check(bytes && *bytes == "HTTP/1.1 " + std::to_string(entry.status) + " " +
                                     std::string(entry.phrase) + "\r\n\r\n",
              "known status serializes its registered reason phrase");
    }
    check(wire_bytes(plain(599, BodyFraming::none, "")) == "HTTP/1.1 599 \r\n\r\n",
          "an unknown code keeps the mandatory SP with an empty reason phrase");
}

// Every typed rejection of invalid caller input, one failure class per check.
void check_rejections() {
    check(wire_error(plain(99, BodyFraming::none, "")) == ResponseError::invalid_status &&
              wire_error(plain(600, BodyFraming::none, "")) == ResponseError::invalid_status,
          "status codes outside 100..599 are rejected");
    check(wire_bytes(plain(100, BodyFraming::none, "")).has_value() &&
              wire_bytes(plain(599, BodyFraming::none, "")).has_value(),
          "the range boundaries 100 and 599 serialize");
    check(wire_error(plain(100, BodyFraming::content_length, "")) ==
                  ResponseError::body_forbidden_status &&
              wire_error(plain(199, BodyFraming::chunked, "")) ==
                  ResponseError::body_forbidden_status &&
              wire_error(plain(204, BodyFraming::none, "x")) ==
                  ResponseError::body_forbidden_status &&
              wire_error(plain(304, BodyFraming::content_length, "")) ==
                  ResponseError::body_forbidden_status,
          "1xx, 204 and 304 cannot carry a body or framing field");
    check(wire_error(plain(200, BodyFraming::none, "x")) == ResponseError::unframed_body,
          "a body without framing is rejected rather than dropped");
    const std::array<ResponseField, 1> empty_name{{{"", "v"}}};
    const std::array<ResponseField, 1> spaced_name{{{"X Test", "v"}}};
    const std::array<ResponseField, 1> slashed_name{{{"X/Test", "v"}}};
    check(wire_error(with_fields(200, BodyFraming::none, "", empty_name)) ==
                  ResponseError::invalid_field_name &&
              wire_error(with_fields(200, BodyFraming::none, "", spaced_name)) ==
                  ResponseError::invalid_field_name &&
              wire_error(with_fields(200, BodyFraming::none, "", slashed_name)) ==
                  ResponseError::invalid_field_name,
          "field names must be RFC 9110 5.6.2 tokens");
    const std::array<ResponseField, 1> leading_space{{{"X-A", " v"}}};
    const std::array<ResponseField, 1> trailing_tab{{{"X-A", "v\t"}}};
    const std::array<ResponseField, 1> bare_cr{{{"X-A", "a\rb"}}};
    const std::array<ResponseField, 1> bare_lf{{{"X-A", "a\nb"}}};
    const std::array<ResponseField, 1> nul_value{{{"X-A", std::string_view("a\0b", 3)}}};
    const std::array<ResponseField, 1> del_value{{{"X-A", "a\x7F"
                                                          "b"}}};
    check(wire_error(with_fields(200, BodyFraming::none, "", leading_space)) ==
                  ResponseError::invalid_field_value &&
              wire_error(with_fields(200, BodyFraming::none, "", trailing_tab)) ==
                  ResponseError::invalid_field_value &&
              wire_error(with_fields(200, BodyFraming::none, "", bare_cr)) ==
                  ResponseError::invalid_field_value &&
              wire_error(with_fields(200, BodyFraming::none, "", bare_lf)) ==
                  ResponseError::invalid_field_value &&
              wire_error(with_fields(200, BodyFraming::none, "", nul_value)) ==
                  ResponseError::invalid_field_value &&
              wire_error(with_fields(200, BodyFraming::none, "", del_value)) ==
                  ResponseError::invalid_field_value,
          "field values reject OWS edges, CR, LF, NUL and DEL octets");
    const std::array<ResponseField, 1> interior_space{{{"X-A", "a b"}}};
    check(wire_bytes(with_fields(200, BodyFraming::none, "", interior_space)).has_value(),
          "interior SP inside a value is valid field content");
    const std::array<ResponseField, 1> upper_length{{{"CONTENT-LENGTH", "5"}}};
    const std::array<ResponseField, 1> transfer{{{"Transfer-Encoding", "chunked"}}};
    const std::array<ResponseField, 1> mixed_connection{{{"cOnNeCtIoN", "close"}}};
    check(wire_error(with_fields(200, BodyFraming::none, "", upper_length)) ==
                  ResponseError::conflicting_field &&
              wire_error(with_fields(200, BodyFraming::none, "", transfer)) ==
                  ResponseError::conflicting_field &&
              wire_error(with_fields(200, BodyFraming::none, "", mixed_connection)) ==
                  ResponseError::conflicting_field,
          "encoder-owned field names are rejected in any case");
}

// Allocation failure must surface as the typed error, never an exception.
void check_allocation_failure() {
    const Response health = plain(200, BodyFraming::content_length, "{\"status\":\"ok\"}");
    bool threw = false;
    try {
        fail_next_allocation = true;
        const auto result = orange::http::write_response(health);
        check(std::holds_alternative<ResponseError>(result) &&
                  std::get<ResponseError>(result) == ResponseError::resource_exhausted,
              "allocation failure returns resource_exhausted");
    } catch (const std::bad_alloc &) {
        threw = true;
    }
    fail_next_allocation = false;
    check(!threw, "write_response does not throw across its boundary");
    const auto recovered = wire_bytes(health);
    check(recovered &&
              *recovered == "HTTP/1.1 200 OK\r\nContent-Length: 15\r\n\r\n{\"status\":\"ok\"}",
          "the encoder serializes the same response after the fault clears");
}

// Every terminal parse error maps to its status with an empty closing
// response, and the rejection statuses serialize the exact rejection wire.
void check_error_responses() {
    const struct {
        orange::http::ParseError error;
        unsigned int status;
    } cases[] = {{orange::http::ParseError::malformed, 400},
                 {orange::http::ParseError::method_too_long, 501},
                 {orange::http::ParseError::target_too_long, 414},
                 {orange::http::ParseError::headers_too_long, 431},
                 {orange::http::ParseError::body_too_long, 413},
                 {orange::http::ParseError::chunk_metadata_too_long, 413},
                 {orange::http::ParseError::unsupported_transfer_coding, 501},
                 {orange::http::ParseError::resource_exhausted, 500},
                 {orange::http::ParseError::incomplete, 400}};
    for (const auto &entry : cases) {
        const Response response = orange::http::error_response(entry.error);
        check(response.status == entry.status && response.close &&
                  response.framing == BodyFraming::none && response.body.empty(),
              "each parse error maps to its status with an empty closing response");
    }
    check(wire_bytes(orange::http::error_response(orange::http::ParseError::malformed)) ==
              "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n",
          "malformed serializes the exact 400 closing wire");
    check(wire_bytes(orange::http::error_response(orange::http::ParseError::headers_too_long)) ==
              "HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\n\r\n",
          "headers_too_long serializes the exact 431 closing wire");
    check(wire_bytes(orange::http::error_response(orange::http::ParseError::body_too_long)) ==
              "HTTP/1.1 413 Content Too Large\r\nConnection: close\r\n\r\n",
          "body_too_long serializes the exact 413 closing wire");
    check(wire_bytes(orange::http::error_response(orange::http::ParseError::resource_exhausted)) ==
              "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n",
          "resource_exhausted serializes the exact 500 closing wire");
}
// Every precomputed rejection wire is byte-identical to serializing the
// response the error maps to, so the static path can never drift.
void check_error_wires() {
    const orange::http::ParseError errors[] = {
        orange::http::ParseError::incomplete,
        orange::http::ParseError::malformed,
        orange::http::ParseError::method_too_long,
        orange::http::ParseError::target_too_long,
        orange::http::ParseError::headers_too_long,
        orange::http::ParseError::body_too_long,
        orange::http::ParseError::chunk_metadata_too_long,
        orange::http::ParseError::unsupported_transfer_coding,
        orange::http::ParseError::resource_exhausted};
    for (const auto error : errors) {
        const auto serialized = wire_bytes(orange::http::error_response(error));
        check(serialized && *serialized == orange::http::error_wire(error),
              "precomputed rejection wires match the serializer byte for byte");
    }
}
// The append-form serializer must emit the same bytes as the owning form,
// preserve existing destination content, and leave the destination unchanged
// on validation or allocation failure.
void check_write_into() {
    const std::array<ResponseField, 1> json_type{{{"Content-Type", "application/json"}}};
    const Response shapes[] = {
        with_fields(200, BodyFraming::content_length, "{\"status\":\"ok\"}", json_type),
        plain(200, BodyFraming::none, ""),
        plain(200, BodyFraming::content_length, ""),
        plain(200, BodyFraming::chunked, "he\r\nllo"),
        plain(200, BodyFraming::chunked, ""),
        plain(431, BodyFraming::none, "", true),
        plain(599, BodyFraming::none, "")};
    for (const Response &response : shapes) {
        const auto expected = wire_bytes(response);
        std::string out = "queued:";
        const auto failed = orange::http::write_response_into(response, out);
        check(expected && !failed && out == "queued:" + *expected,
              "write_response_into appends the exact wire after existing content");
    }
    const Response invalid[] = {plain(99, BodyFraming::none, ""),
                                plain(204, BodyFraming::none, "x"),
                                plain(200, BodyFraming::none, "x")};
    for (const Response &response : invalid) {
        std::string out = "untouched";
        const auto failed = orange::http::write_response_into(response, out);
        check(failed && out == "untouched", "rejected input leaves the destination unchanged");
    }
    std::string out = "stable";
    bool threw = false;
    try {
        fail_next_allocation = true;
        const auto failed =
            orange::http::write_response_into(plain(200, BodyFraming::chunked, "hello"), out);
        check(failed && *failed == ResponseError::resource_exhausted && out == "stable",
              "allocation failure returns resource_exhausted and changes nothing");
    } catch (const std::bad_alloc &) {
        threw = true;
    }
    fail_next_allocation = false;
    check(!threw, "write_response_into does not throw across its boundary");
}
} // namespace
// Executes the framing, phrase, rejection and fault-injection checks.
int main() {
    check_wire_shapes();
    check_reason_phrases();
    check_rejections();
    check_allocation_failure();
    check_error_responses();
    check_error_wires();
    check_write_into();
    std::printf("%zu checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
