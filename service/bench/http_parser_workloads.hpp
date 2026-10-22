// ADR: docs/adr/ADR-0008-http-parser-benchmarks.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Authorization: docs/reviews/2026-10-16-owned-request-storage.md.
// Shared deterministic workloads for separate timing and allocation executables.
#ifndef ORANGE_HTTP_PARSER_WORKLOADS_HPP
#define ORANGE_HTTP_PARSER_WORKLOADS_HPP
#include "http_request_decoder.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
namespace orange::bench {
using orange::http::ParseError;
// Separates complete head, complete message and transport-fragment delivery.
enum class Mode { head, message, fragmented };
// Expected sizes and error are independent fixture facts, never parser output.
struct Case {
    std::string name;
    std::string wire;
    Mode mode = Mode::head;
    std::size_t headers = 0;
    std::size_t content = 0;
    double limit_ns = 5000;
    bool malformed = false;
    // Transport feed size for fragmented delivery; 17 exercises the
    // smallest grammar cursors, larger slices mirror the recv buffer.
    std::size_t feed = 17;
};
// Expected observations come from fixture facts, independently of parser output.
inline std::uint64_t expected_result(const Case &fixture) {
    return fixture.malformed ? 1
                             : fixture.wire.size() + fixture.headers + fixture.content +
                                   (fixture.content == 0 ? 0 : 240);
}
// Encodes a known all-'x' body; HTTP chunks are independent of feed boundaries.
inline std::string chunked(std::size_t bytes, std::size_t chunk_size) {
    std::string wire = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n";
    for (std::size_t offset = 0; offset < bytes; offset += chunk_size) {
        const auto count = std::min(chunk_size, bytes - offset);
        char hex[32];
        std::snprintf(hex, sizeof(hex), "%zx\r\n", count);
        wire += hex;
        wire.append(count, 'x');
        wire += "\r\n";
    }
    return wire + "0\r\n\r\n";
}
// Encodes a known all-'x' body with an exact Content-Length declaration.
inline std::string fixed(std::size_t bytes) {
    return "POST / HTTP/1.1\r\nContent-Length: " + std::to_string(bytes) + "\r\n\r\n" +
           std::string(bytes, 'x');
}
// Fixed-size fixtures make revisions comparable without timing input generation.
inline std::vector<Case> fixtures() {
    const std::string minimal = "GET / HTTP/1.1\r\n\r\n";
    std::string realistic = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n"
                            "Accept: application/json\r\nAccept-Encoding: gzip, deflate\r\n"
                            "Accept-Language: en-US,en;q=0.9\r\nCache-Control: no-cache\r\n"
                            "Pragma: no-cache\r\nX-Request-Id: 0123456789abcdef\r\nUser-Agent: ";
    realistic.append(434 - realistic.size() - 4, 'a');
    realistic += "\r\n\r\n";
    std::string full = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 409; ++i) {
        full += "X: 12345678901234\r\n";
    }
    full.insert(full.size() - 2, 8192 - full.size() - 2, 'x');
    full += "\r\n";
    std::string long_values = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 16; ++i)
        long_values += "X-V: " + std::string(128, 'v') + "\r\n";
    long_values += "\r\n";
    std::string short_fields = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 1000; ++i)
        short_fields += "X-A: v\r\n";
    short_fields += "\r\n";
    const std::string ows = "GET / HTTP/1.1\r\nX: " + std::string(3900, ' ') + "v" +
                            std::string(3900, '\t') + "\r\n\r\n";
    std::string many_ows = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 8; ++i)
        many_ows += "X: " + std::string(400, ' ') + "v" + std::string(400, '\t') + "\r\n";
    many_ows += "\r\n";
    const std::string metadata = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n";
    return {{"head_long_values_16", long_values, Mode::head, 16, 0, 10000},
            {"head_short_fields_1000", short_fields, Mode::head, 1000, 0, 200000},
            {"head_ows_7800", ows, Mode::head, 1, 0, 10000},
            {"head_ows_8_fields", many_ows, Mode::head, 8, 0, 10000},
            {"head_one_short_field", "GET / HTTP/1.1\r\nX-A: a\r\n\r\n", Mode::head, 1, 0, 1000},
            {"head_minimal", minimal, Mode::head, 0, 0, 500},
            {"head_realistic_434", realistic, Mode::head, 9, 0, 5000},
            {"head_full_8192", full, Mode::head, 409, 0, 100000},
            {"head_decoder_whole", realistic, Mode::message, 9, 0, 10000},
            {"head_decoder_feed17", realistic, Mode::fragmented, 9, 0, 20000},
            {"echo_chunked_1024", chunked(1024, 1024), Mode::message, 1, 1024, 10000},
            {"body_1m_chunk1024", chunked(1048576, 1024), Mode::message, 1, 1048576, 500000},
            {"body_1m_chunk16384", chunked(1048576, 16384), Mode::message, 1, 1048576, 500000},
            {"body_1m_fixed_feed16384", fixed(1048576), Mode::fragmented, 1, 1048576, 500000,
             false, 16384},
            {"extension_7000", metadata + "0;x=" + std::string(7000, 'x') + "\r\n\r\n",
             Mode::message, 1, 0, 200000},
            {"trailer_7000", metadata + "0\r\nX: " + std::string(7000, 'x') + "\r\n\r\n",
             Mode::message, 1, 0, 200000},
            {"head_cr_dense", "GET /" + std::string(8187, '\r'), Mode::head, 0, 0, 20000, true}};
}
// Uses the same result observation for old and compact owned representations.
template <class T> std::size_t field_count(const T &request) {
    if constexpr (requires { request.headers(); })
        return request.headers().size();
    else
        return request.headers.size();
}
// Observes result sizes and content endpoints; production code is in a separate TU.
inline std::uint64_t digest(const orange::http::ParsedMessage &parsed) {
    return parsed.consumed + field_count(parsed.request) + parsed.body.size() +
           (parsed.body.empty() ? 0
                                : static_cast<unsigned char>(parsed.body.front()) +
                                      static_cast<unsigned char>(parsed.body.back()));
}
// Includes construction, parsing, result observation and destruction per request.
inline std::uint64_t run(const Case &fixture) {
    if (fixture.mode == Mode::head) {
        const auto result = orange::http::parse_request(fixture.wire);
        if (const auto *parsed = std::get_if<orange::http::ParsedRequest>(&result)) {
            return parsed->consumed + field_count(parsed->request);
        }
        return std::get<ParseError>(result) == ParseError::malformed ? 1 : 0;
    }
    if (fixture.mode == Mode::message) {
        const auto result = orange::http::parse_message(fixture.wire);
        const auto *parsed = std::get_if<orange::http::ParsedMessage>(&result);
        return parsed ? digest(*parsed) : 0;
    }
    orange::http::RequestDecoder decoder;
    for (std::size_t offset = 0; offset < fixture.wire.size(); offset += fixture.feed) {
        const auto result =
            decoder.feed(std::string_view(fixture.wire).substr(offset, fixture.feed));
        if (const auto *parsed = std::get_if<orange::http::ParsedMessage>(&result.outcome)) {
            return digest(*parsed);
        }
    }
    return 0;
}
} // namespace orange::bench
#endif
