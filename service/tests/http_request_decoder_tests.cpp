// ADR: service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md
// ADR: service/docs/adr/ADR-0003-fragmented-head-offsets.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
// Behavior tests for complete request bodies, distinct from head-only parsing.
#include "http_request_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <string>
#include <string_view>
#include <variant>
namespace {
bool fail_next_allocation = false;
}
// Injects one ordinary allocation failure at the public decoder boundary;
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
int failures = 0;
int checks = 0;
// Checks public results and gives CTest a failing process exit status.
void check(bool condition, std::string_view description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %.*s\n", static_cast<int>(description.size()),
                     description.data());
    }
}
// RFC 9112 6.3 fixes the body boundary independently of following input.
void check_content_length() {
    const std::string head = "POST /api/echo HTTP/1.1\r\nContent-Length: 5\r\n\r\n";
    const auto result = orange::http::parse_message(head + "helloNEXT");
    const auto *parsed = std::get_if<orange::http::ParsedMessage>(&result);
    check(parsed && parsed->body == "hello" && parsed->consumed == head.size() + 5 &&
              parsed->request.target() == "/api/echo",
          "Content-Length includes exactly five body bytes");
    const auto partial = orange::http::parse_message(head + "hell");
    check(std::get_if<orange::http::ParseError>(&partial) &&
              std::get<orange::http::ParseError>(partial) == orange::http::ParseError::incomplete,
          "an unfinished fixed-length body remains incomplete");
    const std::string binary("a\0b\r\n", 5);
    const auto bytes = orange::http::parse_message(head + binary);
    const auto *owned = std::get_if<orange::http::ParsedMessage>(&bytes);
    check(owned && owned->body == binary && owned->consumed == head.size() + 5,
          "request content preserves NUL and CRLF as data");
}
// Builds a wire fixture independently of the decoder's internal state.
std::string message(std::string_view fields, std::string_view body = {}) {
    return "POST /api/echo HTTP/1.1\r\n" + std::string(fields) + "\r\n" + std::string(body);
}
// Framing errors are public values, with no partially usable request.
void expect_error(const std::string &raw, orange::http::ParseError expected,
                  std::string_view description) {
    const auto result = orange::http::parse_message(raw);
    const auto *error = std::get_if<orange::http::ParseError>(&result);
    check(error && *error == expected, description);
}
// RFC 9112 6.3 requires one unambiguous length, regardless of field case.
void check_length_policy() {
    using orange::http::ParseError;
    const auto equal = orange::http::parse_message(
        message("content-length: 0005, 5\r\nCONTENT-LENGTH: 05\r\n", "helloNEXT"));
    const auto *parsed = std::get_if<orange::http::ParsedMessage>(&equal);
    check(parsed && parsed->body == "hello", "equivalent list and duplicate lengths are accepted");
    for (const auto value : {"", "-1", "+5", "5x", "5 5", "5, 6", "5,,6"}) {
        expect_error(message("Content-Length: " + std::string(value) + "\r\n", "hello"),
                     ParseError::malformed, "invalid or conflicting length is rejected");
    }
    expect_error(message("Content-Length: 5\r\nContent-Length: 6\r\n", "hello!"),
                 ParseError::malformed, "conflicting duplicate fields are rejected");
    const auto zero = orange::http::parse_message(message("Content-Length: 0\r\n", "NEXT"));
    const auto *empty = std::get_if<orange::http::ParsedMessage>(&zero);
    check(empty && empty->body.empty() &&
              empty->consumed == message("Content-Length: 0\r\n").size(),
          "zero length leaves pipeline input untouched");
    const auto absent = orange::http::parse_message(message("", "NEXT"));
    const auto *none = std::get_if<orange::http::ParsedMessage>(&absent);
    check(none && none->body.empty() && none->consumed == message("").size(),
          "unframed requests have no body");
    expect_error(message("Content-Length: 1048577\r\n"), ParseError::body_too_long,
                 "oversized declared body fails without waiting for content");
    expect_error(message("Content-Length: 999999999999999999999999999999999999\r\n"),
                 ParseError::body_too_long, "huge valid decimal length cannot overflow");
    expect_error(message("Content-Length: 999999999999999999999999999x\r\n"), ParseError::malformed,
                 "invalid large decimal remains malformed");
    const auto cap = orange::http::parse_message(
        message("Content-Length: 1048576\r\n", std::string(orange::http::request_body_cap, 'x')));
    const auto *bounded = std::get_if<orange::http::ParsedMessage>(&cap);
    check(bounded && bounded->body.size() == orange::http::request_body_cap,
          "exact decoded body cap is accepted");
}
// RFC 9112 6.1/6.3 prevents ambiguous framing and unsupported decoding.
void check_transfer_policy() {
    using orange::http::ParseError;
    for (const auto fields :
         {"Content-Length: 0\r\nTransfer-Encoding: chunked\r\n", "Transfer-Encoding: gzip\r\n",
          "Transfer-Encoding: chunked, gzip\r\n", "Transfer-Encoding: chunked, chunked\r\n",
          "Transfer-Encoding: chunked; x=y\r\n", "Transfer-Encoding: ,\r\n",
          "Transfer-Encoding: gzip; x\r\n"}) {
        expect_error(message(fields, "0\r\n\r\n"), ParseError::malformed,
                     "invalid transfer framing is rejected");
    }
    expect_error(message("Transfer-Encoding: gzip, chunked\r\n", "0\r\n\r\n"),
                 ParseError::unsupported_transfer_coding,
                 "unknown prior coding reports unsupported");
    expect_error("POST / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
                 ParseError::malformed, "HTTP/1.0 transfer encoding is rejected");
}
// Chunk content is binary; extensions and trailers contribute only wire bytes.
void check_chunked() {
    using orange::http::ParseError;
    const auto raw = message("transfer-encoding: ChUnKeD\r\n",
                             "4; foo=\"a\\\"b\"\r\nWiki\r\n5;bar=token\r\npedia\r\n"
                             "000; end\r\nContent-Length: 999\r\nX-Trace: yes\r\n\r\n");
    const auto outcome = orange::http::parse_message(raw + "NEXT");
    const auto *parsed = std::get_if<orange::http::ParsedMessage>(&outcome);
    check(parsed && parsed->body == "Wikipedia" && parsed->consumed == raw.size() &&
              parsed->request.headers().size() == 1,
          "chunk sizes, quoted extensions and discarded trailers define the message boundary");
    const std::string binary("a\0b\r\n", 5);
    const auto bytes = orange::http::parse_message(
        message("Transfer-Encoding: chunked\r\n", "5\r\n" + binary + "\r\n0\r\n\r\n"));
    const auto *owned = std::get_if<orange::http::ParsedMessage>(&bytes);
    check(owned && owned->body == binary, "chunk data preserves every octet");
    for (std::size_t i = message("transfer-encoding: ChUnKeD\r\n").size(); i < raw.size(); ++i) {
        expect_error(raw.substr(0, i), ParseError::incomplete,
                     "each valid chunked prefix is incomplete");
    }
    for (const auto body : {"z\r\n", "-1\r\n", "1\na\r\n0\r\n\r\n", "1\r\naX\n0\r\n\r\n",
                            "0\r\n bad: x\r\n\r\n", "0\r\nX : y\r\n\r\n", "0\r\nX: bad\x7f\r\n\r\n",
                            "0; foo=\r\n\r\n", "0; foo=\"bad\r\n\r\n"}) {
        expect_error(message("Transfer-Encoding: chunked\r\n", body), ParseError::malformed,
                     "malformed chunk metadata or data delimiter is rejected");
    }
    expect_error(message("Transfer-Encoding: chunked\r\n", "100001\r\n"), ParseError::body_too_long,
                 "oversized chunk fails before its data arrives");
    expect_error(message("Transfer-Encoding: chunked\r\n", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n"),
                 ParseError::body_too_long, "huge hexadecimal size cannot wrap");
    const std::string content(orange::http::request_body_cap, 'x');
    const auto capped = orange::http::parse_message(
        message("Transfer-Encoding: chunked\r\n", "100000\r\n" + content + "\r\n0\r\n\r\n"));
    const auto *at_cap = std::get_if<orange::http::ParsedMessage>(&capped);
    check(at_cap && at_cap->body == content, "chunked body accepts exactly the decoded cap");
    expect_error(message("Transfer-Encoding: chunked\r\n", "100000\r\n" + content + "\r\n1\r\n"),
                 ParseError::body_too_long, "decoded quota is shared across chunks");
}
// Arbitrary transport splits must agree with one-shot parsing and retain ownership.
void check_fragments(const std::string &raw) {
    for (std::size_t split = 0; split < raw.size(); ++split) {
        orange::http::RequestDecoder decoder;
        auto first = decoder.feed(std::string(raw.substr(0, split)));
        check(first.consumed == split && std::holds_alternative<std::monostate>(first.outcome),
              "a valid prefix consumes just its new bytes");
        auto last = decoder.feed(raw.substr(split) + "NEXT");
        const auto *parsed = std::get_if<orange::http::ParsedMessage>(&last.outcome);
        check(parsed && parsed->body == "hello" && parsed->consumed == raw.size() &&
                  last.consumed == raw.size() - split,
              "split request completes without consuming the following message");
        check(decoder.feed("NEXT").consumed == 0, "completed decoder waits for explicit reset");
        decoder.reset();
        const auto next = decoder.feed("GET /next HTTP/1.1\r\n\r\n");
        const auto *new_message = std::get_if<orange::http::ParsedMessage>(&next.outcome);
        check(new_message && new_message->request.target() == "/next" && new_message->body.empty(),
              "reset starts an independent pipelined request");
    }
    orange::http::RequestDecoder decoder;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const std::string transient(1, raw[i]);
        const auto step = decoder.feed(transient);
        check(step.consumed == 1, "one-byte fragments always advance once");
        if (i + 1 == raw.size()) {
            const auto *parsed = std::get_if<orange::http::ParsedMessage>(&step.outcome);
            check(parsed && parsed->body == "hello", "one-byte decoding preserves body progress");
        } else {
            check(std::holds_alternative<std::monostate>(step.outcome),
                  "partial fragment does not finish early");
        }
    }
}
// Different connection instances must keep independent progress and sticky errors.
void check_decoder_lifecycle() {
    using orange::http::ParseError;
    orange::http::RequestDecoder first;
    orange::http::RequestDecoder second;
    const auto prefix = first.feed(message("Content-Length: 5\r\n", "he"));
    check(std::holds_alternative<std::monostate>(prefix.outcome), "first connection awaits body");
    const auto done = second.feed("GET /other HTTP/1.1\r\n\r\n");
    check(std::holds_alternative<orange::http::ParsedMessage>(done.outcome),
          "second connection completes independently");
    const auto last = first.feed("llo");
    const auto *parsed = std::get_if<orange::http::ParsedMessage>(&last.outcome);
    check(parsed && parsed->body == "hello", "first connection retains its own pending body");
    first.reset();
    const auto error = first.feed(message("Content-Length: 1048577\r\n"));
    check(std::get_if<ParseError>(&error.outcome) &&
              std::get<ParseError>(error.outcome) == ParseError::body_too_long,
          "oversized body yields a terminal decoder error");
    const auto sticky = first.feed("GET / HTTP/1.1\r\n\r\n");
    check(sticky.consumed == 0 && std::get_if<ParseError>(&sticky.outcome) &&
              std::get<ParseError>(sticky.outcome) == ParseError::body_too_long,
          "error never treats later bytes as another request");
}
// BWS belongs before a following delimiter, not by itself at the line end
// (RFC 9112 7.1.1 chunk-ext ABNF).
void check_extension_whitespace() {
    using orange::http::ParseError;
    for (const auto line : {"0;foo \r\n\r\n", "0;foo=x \r\n\r\n", "0;foo=\"x\" \r\n\r\n"}) {
        expect_error(message("Transfer-Encoding: chunked\r\n", line), ParseError::malformed,
                     "unattached trailing BWS is rejected");
    }
    check_fragments(message("Transfer-Encoding: chunked\r\n",
                            "5 \t; \tfoo \t = \t\"abc\" \t;bar=token\r\nhello\r\n0\r\n\r\n"));
}
// Allocation failure is a coded sticky error, not an exception across the API.
void check_allocation_failure() {
    using orange::http::ParseError;
    orange::http::RequestDecoder decoder;
    bool threw = false;
    bool reported = false;
    fail_next_allocation = true;
    try {
        const auto result = decoder.feed("GET / HTTP/1.1\r\nX: y\r\n\r\n");
        reported = std::get_if<ParseError>(&result.outcome) &&
                   std::get<ParseError>(result.outcome) == ParseError::resource_exhausted;
    } catch (const std::bad_alloc &) {
        threw = true;
    }
    fail_next_allocation = false;
    check(!threw && reported, "allocation failure is returned as a diagnostic");
    const auto sticky = decoder.feed("GET / HTTP/1.1\r\n\r\n");
    check(sticky.consumed == 0 && std::get_if<ParseError>(&sticky.outcome) &&
              std::get<ParseError>(sticky.outcome) == ParseError::resource_exhausted,
          "allocation failure is terminal until reset");
    decoder.reset();
    check(std::holds_alternative<orange::http::ParsedMessage>(
              decoder.feed("GET / HTTP/1.1\r\n\r\n").outcome),
          "reset recovers a decoder after allocation failure");
}
// Characterizes extension/trailer octet sets independently of byte classifiers.
void check_metadata_octets() {
    using orange::http::ParseError;
    for (unsigned int byte = 0; byte < 256; ++byte) {
        const char c = static_cast<char>(byte);
        const bool field_octet = byte == 9 || (byte >= 32 && byte != 127);
        // A backslash in this fixture escapes the following valid 'b'.
        const bool quoted = field_octet && c != '"';
        const auto raw = message("Transfer-Encoding: chunked\r\n",
                                 "0;foo=\"a" + std::string(1, c) + "b\"\r\n\r\n");
        const auto result = orange::http::parse_message(raw);
        check(quoted ? std::holds_alternative<orange::http::ParsedMessage>(result)
                     : std::get_if<ParseError>(&result) &&
                           std::get<ParseError>(result) == ParseError::malformed,
              "every quoted extension octet follows its grammar");
        const auto escaped = orange::http::parse_message(message(
            "Transfer-Encoding: chunked\r\n", "0;foo=\"a\\" + std::string(1, c) + "b\"\r\n\r\n"));
        check(field_octet ? std::holds_alternative<orange::http::ParsedMessage>(escaped)
                          : std::get_if<ParseError>(&escaped) &&
                                std::get<ParseError>(escaped) == ParseError::malformed,
              "every escaped extension octet follows quoted-pair grammar");
        const auto trailer = orange::http::parse_message(message(
            "Transfer-Encoding: chunked\r\n", "0\r\nX: a" + std::string(1, c) + "b\r\n\r\n"));
        check(field_octet ? std::holds_alternative<orange::http::ParsedMessage>(trailer)
                          : std::get_if<ParseError>(&trailer) &&
                                std::get<ParseError>(trailer) == ParseError::malformed,
              "every trailer value octet follows field-content grammar");
    }
}
// Heads share the old parser's byte cap and error precedence under fragmentation.
void check_head_boundaries() {
    using orange::http::ParseError;
    const std::string short_head = "GET / HTTP/1.1\r\nX: ";
    const auto exact = short_head +
                       std::string(orange::http::header_block_cap - short_head.size() - 4, 'x') +
                       "\r\n\r\n";
    for (const auto &raw : std::array<std::string, 7>{
             exact, exact.substr(0, exact.size() - 1),
             "GET /" + std::string(orange::http::header_block_cap, 'x'),
             std::string(orange::http::header_block_cap, 'M'), "GET / HTTP/1.1\r\n bad: y\r\n",
             "GET / HTTP/1.1\n", "\r\n\r\nGET / HTTP/1.1\r\n\r\n"}) {
        const auto expected = orange::http::parse_request(raw);
        orange::http::RequestDecoder decoder;
        std::variant<std::monostate, orange::http::ParsedMessage, ParseError> outcome;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const auto step = decoder.feed(std::string_view(raw).substr(i, 1));
            outcome = step.outcome;
            if (!std::holds_alternative<std::monostate>(outcome)) {
                break;
            }
        }
        if (const auto *error = std::get_if<ParseError>(&expected)) {
            check(*error == ParseError::incomplete ? std::holds_alternative<std::monostate>(outcome)
                                                   : std::get_if<ParseError>(&outcome) &&
                                                         std::get<ParseError>(outcome) == *error,
                  "fragmented head preserves established rejection classification");
        } else {
            const auto *parsed = std::get_if<orange::http::ParsedMessage>(&outcome);
            check(parsed &&
                      parsed->consumed == std::get<orange::http::ParsedRequest>(expected).consumed,
                  "fragmented head completes at the same cap and leading-line boundary");
        }
    }
}
// The directly authorized repair bounds aggregate chunk framing and trailers
// to 8 KiB, independently of decoded content (peer-assessment repair report).
void check_metadata_limits() {
    using orange::http::ParseError;
    constexpr std::size_t cap = 8192;
    const std::string fields = "Transfer-Encoding: chunked\r\n";
    for (const auto &body :
         std::array<std::string, 2>{"0;x=" + std::string(cap - 8, 'x') + "\r\n\r\n",
                                    "0\r\nX:" + std::string(cap - 9, 'x') + "\r\n\r\n"}) {
        const auto raw = message(fields, body);
        const auto result = orange::http::parse_message(raw + "NEXT");
        const auto *parsed = std::get_if<orange::http::ParsedMessage>(&result);
        check(parsed && parsed->body.empty() && parsed->consumed == raw.size(),
              "exact metadata cap completes without consuming pipelined input");
        auto over = body;
        over.insert(over.size() - 4, 1, 'x');
        expect_error(message(fields, over), ParseError::chunk_metadata_too_long,
                     "one metadata byte above the cap is rejected with HTTP 413");
        orange::http::RequestDecoder decoder;
        for (const char c : raw) {
            const auto step = decoder.feed(std::string_view(&c, 1));
            check(step.consumed == 1, "one-byte fragments preserve the metadata budget");
        }
        decoder.reset();
        const auto again = decoder.feed(raw);
        check(std::holds_alternative<orange::http::ParsedMessage>(again.outcome),
              "reset restores a fresh per-request metadata budget");
    }
}
// Many short trailers and chunks share one budget; splitting input cannot evade it.
void check_aggregate_metadata() {
    using orange::http::ParseError;
    std::string chunks;
    for (std::size_t i = 0; i < 1700; ++i) {
        chunks += "1\r\nx\r\n";
    }
    std::string trailers = "0\r\n";
    for (std::size_t i = 0; i < 2000; ++i) {
        trailers += "X: y\r\n";
    }
    for (const auto &body : std::array<std::string, 2>{chunks + "0\r\n\r\n", trailers + "\r\n"}) {
        const auto raw = message("Transfer-Encoding: chunked\r\n", body);
        for (const std::size_t fragment : {std::size_t{1}, std::size_t{17}, raw.size()}) {
            orange::http::RequestDecoder decoder;
            bool rejected = false;
            for (std::size_t offset = 0; offset < raw.size(); offset += fragment) {
                const auto step = decoder.feed(std::string_view(raw).substr(offset, fragment));
                if (const auto *error = std::get_if<ParseError>(&step.outcome)) {
                    rejected = *error == ParseError::chunk_metadata_too_long;
                    break;
                }
            }
            check(rejected,
                  "aggregate metadata is bounded across chunk, trailer and transport splits");
            const auto sticky = decoder.feed("GET / HTTP/1.1\r\n\r\n");
            check(sticky.consumed == 0 && std::get_if<ParseError>(&sticky.outcome) &&
                      std::get<ParseError>(sticky.outcome) == ParseError::chunk_metadata_too_long,
                  "metadata rejection remains sticky until reset");
            decoder.reset();
            const auto fresh = decoder.feed(message("Transfer-Encoding: chunked\r\n", "0\r\n\r\n"));
            check(std::holds_alternative<orange::http::ParsedMessage>(fresh.outcome),
                  "reset after rejection permits a fresh independent input stream");
        }
    }
}
// Pins the independent content and framing budgets, regardless of feed splitting.
void check_joint_chunk_budgets() {
    using orange::http::ParseError;
    const std::string content(orange::http::request_body_cap, 'x');
    for (const std::size_t chunk_size : {std::size_t{512}, std::size_t{1024}}) {
        std::string body;
        const std::string size_line = chunk_size == 512 ? "200\r\n" : "400\r\n";
        for (std::size_t offset = 0; offset < content.size(); offset += chunk_size) {
            body += size_line;
            body.append(content, offset, chunk_size);
            body += "\r\n";
        }
        body += "0\r\n\r\n";
        const auto wire = message("Transfer-Encoding: chunked\r\n", body);
        for (const std::size_t fragment : {std::size_t{17}, std::size_t{8192}, wire.size()}) {
            orange::http::RequestDecoder decoder;
            bool terminal = false;
            for (std::size_t offset = 0; offset < wire.size(); offset += fragment) {
                const auto step = decoder.feed(std::string_view(wire).substr(offset, fragment));
                if (const auto *parsed = std::get_if<orange::http::ParsedMessage>(&step.outcome)) {
                    check(chunk_size == 1024 && parsed->body == content &&
                              parsed->consumed == wire.size(),
                          "1 MiB in 1024-byte chunks fits both independent budgets");
                    terminal = true;
                } else if (const auto *error = std::get_if<ParseError>(&step.outcome)) {
                    check(chunk_size == 512 && *error == ParseError::chunk_metadata_too_long,
                          "1 MiB in 512-byte chunks exceeds the metadata budget");
                    terminal = true;
                }
                if (terminal)
                    break;
            }
            check(terminal, "joint-budget outcome is independent of transport feed sizes");
        }
    }
}
// Uses known decoded content as an independent oracle for seeded wire fixtures.
void check_generated_message(const std::string &wire, const std::string &content,
                             std::mt19937 &rng) {
    orange::http::RequestDecoder decoder;
    const auto input = wire + "NEXT";
    std::size_t offset = 0;
    while (offset < input.size()) {
        const auto count = std::min<std::size_t>(1 + rng() % 31, input.size() - offset);
        const auto step = decoder.feed(std::string_view(input).substr(offset, count));
        offset += step.consumed;
        if (const auto *parsed = std::get_if<orange::http::ParsedMessage>(&step.outcome)) {
            check(parsed->body == content && parsed->consumed == wire.size() &&
                      offset == wire.size(),
                  "seeded decoding matches the independent content and wire-boundary oracle");
            return;
        }
        if (!std::holds_alternative<std::monostate>(step.outcome) || step.consumed != count) {
            check(false, "a seeded valid fragment advances until completion");
            return;
        }
    }
    check(false, "a complete seeded fixture produces a message");
}
// Exercises arbitrary binary content and many chunk/transport boundaries.
void check_seeded_roundtrips() {
    std::mt19937 rng(0x424F4459);
    for (std::size_t n = 0; n < 200; ++n) {
        std::string content(n * 37 % 513, '\0');
        for (auto &c : content) {
            c = static_cast<char>(rng() % 256);
        }
        check_generated_message(
            message("Content-Length: " + std::to_string(content.size()) + "\r\n", content), content,
            rng);
        std::string encoded;
        for (std::size_t offset = 0; offset < content.size();) {
            const auto count = std::min<std::size_t>(1 + rng() % 23, content.size() - offset);
            char hex[32];
            std::snprintf(hex, sizeof(hex), "%zx; x=\"a\\\"b\"\r\n", count);
            encoded += hex;
            encoded.append(content, offset, count);
            encoded += "\r\n";
            offset += count;
        }
        encoded += "0\r\nX: discarded\r\n\r\n";
        check_generated_message(message("Transfer-Encoding: chunked\r\n", encoded), content, rng);
    }
}
} // namespace
int main() {
    check_content_length();
    check_length_policy();
    check_transfer_policy();
    check_chunked();
    check_fragments(message("Content-Length: 5\r\n", "hello"));
    check_fragments(
        message("Transfer-Encoding: chunked\r\n", "2\r\nhe\r\n3; x=y\r\nllo\r\n0\r\nX: y\r\n\r\n"));
    check_decoder_lifecycle();
    check_extension_whitespace();
    check_allocation_failure();
    check_metadata_octets();
    check_head_boundaries();
    check_metadata_limits();
    check_aggregate_metadata();
    check_joint_chunk_budgets();
    check_seeded_roundtrips();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
