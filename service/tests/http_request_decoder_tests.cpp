// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Behavior tests for complete request bodies, distinct from head-only parsing.
#include "http_request_decoder.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
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
// Checks public results and gives CTest a failing process exit status.
void check(bool condition, std::string_view description) {
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
              parsed->request.target == "/api/echo",
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
              parsed->request.headers.size() == 1,
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
        check(new_message && new_message->request.target == "/next" && new_message->body.empty(),
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
    return failures ? 1 : 0;
}
