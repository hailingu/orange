// ADR: service/docs/adr/ADR-0003-fragmented-head-offsets.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Authorization: docs/reviews/2026-10-16-http-performance.md.
// Pins owned construction from validated offsets despite buffer relocation.
#include "http_head_fields.hpp"
#include <cstdio>
#include <string>

namespace {
using orange::http::detail::HeadFields;
using orange::http::detail::HeadLineResult;
int failures = 0;
// Gives CTest a failure for field loss, stale views or premature completion.
void check(bool condition, const char *description) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}
// Forces relocation between lines; offsets, order, case and OWS must survive.
void check_owned_fields() {
    HeadFields fields;
    std::string head;
    for (const std::string line :
         {"", "GET /path%20x HTTP/1.1", "X-A: \tvalue\t ", "x-a: second", "Empty:", ""}) {
        const auto begin = head.size();
        head = std::string(head); // No saved borrowed view may rely on this buffer.
        head += line + "\r\n";
        const auto state = fields.append_line(line, begin);
        check(state == (head.ends_with("Empty:\r\n\r\n") ? HeadLineResult::complete
                                                         : HeadLineResult::pending),
              "only the final empty line completes a validated head");
    }
    auto parsed = fields.finish(head, head.size());
    head.assign(9000, 'z');
    check(parsed.request.method() == "GET" && parsed.request.target() == "/path%20x" &&
              parsed.request.version() == "HTTP/1.1" && parsed.request.headers().size() == 3,
          "request line and all fields own their storage");
    if (parsed.request.headers().size() == 3) {
        check(parsed.request.headers()[0].name == "X-A" &&
                  parsed.request.headers()[0].value == "value" &&
                  parsed.request.headers()[1].name == "x-a" &&
                  parsed.request.headers()[1].value == "second" &&
                  parsed.request.headers()[2].value.empty(),
              "offset construction preserves field order, received case and trimmed values");
    }
}
// Shared validation must reject invalid lines before they can become descriptors.
void check_rejections() {
    HeadFields oversized;
    check(oversized.append_line("GET / HTTP/1.1", 8190) == HeadLineResult::malformed,
          "offset descriptors cannot exceed the head budget");
    for (const auto bad : {"GET /bad%Q0 HTTP/1.1", "G ET / HTTP/1.1", "GET / HTTP/1.10"}) {
        HeadFields fields;
        check(fields.append_line(bad, 0) == HeadLineResult::malformed,
              "invalid request line cannot enter the validated head");
    }
    for (const auto bad : {" bad: value", "X : value", "X: bad\x7f", "X: bad\rvalue"}) {
        HeadFields fields;
        check(fields.append_line("GET / HTTP/1.1", 0) == HeadLineResult::pending,
              "valid request line remains pending");
        check(fields.append_line(bad, 16) == HeadLineResult::malformed,
              "invalid field cannot enter the validated head");
    }
}
// Growing descriptor storage must retain all fields through buffer relocations.
void check_many_fields() {
    HeadFields fields;
    std::string head = "GET / HTTP/1.1\r\n";
    check(fields.append_line("GET / HTTP/1.1", 0) == HeadLineResult::pending,
          "request line starts a large fragmented head");
    for (int i = 0; i < 409; ++i) {
        const std::string line = "X: " + std::to_string(i);
        const auto begin = head.size();
        head += line + "\r\n";
        check(fields.append_line(line, begin) == HeadLineResult::pending,
              "each valid field remains pending");
    }
    check(fields.append_line("", head.size()) == HeadLineResult::complete,
          "empty line completes all saved fields");
    head += "\r\n";
    const auto parsed = fields.finish(head, head.size());
    check(parsed.consumed == head.size() && parsed.request.headers().size() == 409,
          "growing descriptors preserve count and final cursor");
    for (std::size_t i = 0; i < parsed.request.headers().size(); ++i) {
        check(parsed.request.headers()[i].name == "X" &&
                  parsed.request.headers()[i].value == std::to_string(i),
              "all field values survive descriptor and head relocations in order");
    }
}
} // namespace
// Executes owned construction, grammar rejection and offset-budget checks.
int main() {
    check_owned_fields();
    check_rejections();
    check_many_fields();
    return failures ? 1 : 0;
}
