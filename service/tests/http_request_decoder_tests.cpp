// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Behavior tests for complete request bodies, distinct from head-only parsing.
#include "http_request_decoder.hpp"
#include <cstdio>
#include <string>
#include <string_view>
#include <variant>
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
} // namespace
int main() {
    check_content_length();
    return failures ? 1 : 0;
}
