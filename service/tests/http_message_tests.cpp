// Governed by docs/adr/ADR-0003-local-runtime-path.md.
//
// Unit suite for the orange HTTP wire-format layer. The harness is
// hand-rolled on purpose: the default dependency set is the C++ standard
// library and POSIX, and a third-party test framework would require an
// accepted ADR (docs/development/cpp-development-standard.md, Dependencies).
// CTest judges this executable by its process exit status; CheckReporter
// turns each failed check into a stderr line, and main() turns the failure
// count into that exit status.

#include "http_messages.hpp"

#include <cstdio>
#include <format>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <variant>

namespace {

// Records check failures for one run and renders each with its source
// location; the process exit status derived here is what CTest sees.
class CheckReporter {
  public:
    // Evaluates one named expectation. `location` defaults to the call site
    // (a C++20 idiom), so callers write plain descriptions and still get
    // exact file:line data in every failure report.
    void check(bool condition, std::string_view description,
               std::source_location location = std::source_location::current()) {
        if (condition) {
            return;
        }
        ++failures_;
        std::fputs(std::format("{}:{}: CHECK failed: {}\n", location.file_name(), location.line(),
                               description)
                       .c_str(),
                   stderr);
    }

    // Maps the failure count onto the process exit status: 0 passes, 1 fails.
    [[nodiscard]] int exit_status() const { return failures_ == 0 ? 0 : 1; }

  private:
    int failures_ = 0;
};

using ParseOutcome = std::variant<orange::http::ParsedRequest, orange::http::ParseError>;

// True when the outcome holds exactly this error value.
bool is_error(const ParseOutcome &outcome, orange::http::ParseError expected) {
    const auto *error = std::get_if<orange::http::ParseError>(&outcome);
    return error != nullptr && *error == expected;
}

// A copy of the parsed request when the outcome holds one. Returning by
// value keeps the result valid when `outcome` is a temporary; a pointer into
// a temporary variant would dangle once the full-expression ends.
std::optional<orange::http::ParsedRequest> parsed_of(const ParseOutcome &outcome) {
    if (const auto *const parsed = std::get_if<orange::http::ParsedRequest>(&outcome)) {
        return *parsed;
    }
    return std::nullopt;
}

} // namespace

int main() {
    using namespace std::string_view_literals;
    CheckReporter reporter;

    // --- incomplete: no line end yet, head budget not exhausted ----------
    reporter.check(is_error(orange::http::parse_request(""), orange::http::ParseError::incomplete),
                   "empty input is incomplete, not malformed");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1"),
                            orange::http::ParseError::incomplete),
                   "request line without its CRLF is incomplete");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r"),
                            orange::http::ParseError::incomplete),
                   "trailing lone CR is incomplete");
    reporter.check(is_error(orange::http::parse_request(std::string(64, 'A')),
                            orange::http::ParseError::incomplete),
                   "short unterminated input is incomplete");
    reporter.check(
        is_error(orange::http::parse_request("\r\n"), orange::http::ParseError::incomplete),
        "only empty lines so far is incomplete");

    // --- the request line overflows the head budget (RFC 9112 3) ---------
    // A grammar-valid prefix is too long rather than malformed: 501 for an
    // overlong method, 414 (MUST) for an overlong target, attributed to the
    // longer of the two once both have started.
    constexpr std::size_t cap = orange::http::header_block_cap;
    reporter.check(is_error(orange::http::parse_request(std::string(cap, 'A')),
                            orange::http::ParseError::method_too_long),
                   "budget exhausted inside the method is method_too_long (501)");
    reporter.check(is_error(orange::http::parse_request("GET /" + std::string(cap - 5, 'a')),
                            orange::http::ParseError::target_too_long),
                   "budget exhausted inside the target is target_too_long (414)");
    reporter.check(is_error(orange::http::parse_request("GET /" + std::string(cap - 7, 'a') + "%2"),
                            orange::http::ParseError::target_too_long),
                   "a percent-encoding cut by the budget keeps the target too long");
    reporter.check(
        is_error(orange::http::parse_request(std::string(cap - 12, 'A') + " / HTTP/1.1\r\n"),
                 orange::http::ParseError::method_too_long),
        "budget exhausted in the version blames the longer field: the method");

    const std::string line_at_cap = "GET /" + std::string(cap - 16, 'a') + " HTTP/1.1\r\n";
    const auto at_cap = parsed_of(orange::http::parse_request(line_at_cap));
    reporter.check(at_cap && at_cap->consumed == cap,
                   "a request line ending on the budget's last byte parses");
    reporter.check(is_error(orange::http::parse_request("GET /" + std::string(cap - 15, 'a') +
                                                        " HTTP/1.1\r\n"),
                            orange::http::ParseError::target_too_long),
                   "a request line ending one byte past the budget is target_too_long");

    // An overflowing prefix that already breaks the grammar stays malformed.
    reporter.check(is_error(orange::http::parse_request("GE(T /" + std::string(cap, 'a')),
                            orange::http::ParseError::malformed),
                   "overflow after an invalid method byte is malformed");
    reporter.check(is_error(orange::http::parse_request("GET /a\rb" + std::string(cap, 'a')),
                            orange::http::ParseError::malformed),
                   "overflow after a bare CR is malformed");
    reporter.check(is_error(orange::http::parse_request("GET /" + std::string(cap - 20, 'a') +
                                                        " banana-banana-banana"),
                            orange::http::ParseError::malformed),
                   "overflow inside an invalid version is malformed");
    std::string empty_lines;
    while (empty_lines.size() < cap) {
        empty_lines += "\r\n";
    }
    reporter.check(
        is_error(orange::http::parse_request(empty_lines), orange::http::ParseError::malformed),
        "empty lines exhausting the budget are malformed, not a header overflow");

    // --- parses: fields, validation breadth, consumed bytes --------------
    const auto posted = orange::http::parse_request("POST /api/echo HTTP/1.1\r\n");
    reporter.check(parsed_of(posted).has_value(), "well-formed request line parses");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.method == "POST",
                   "request-line method parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.target == "/api/echo",
                   "request-line target parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.version == "HTTP/1.1",
                   "request-line version parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->consumed == 25,
                   "consumed spans the request line and its CRLF");

    const auto future = parsed_of(orange::http::parse_request("GET / HTTP/2.0\r\n"));
    reporter.check(future && future->request.version == "HTTP/2.0",
                   "grammar-valid future version parses; serving policy is a later layer");

    const auto custom = parsed_of(orange::http::parse_request("!CUSTOM! /a%20b?q=1 HTTP/1.1\r\n"));
    reporter.check(custom && custom->request.method == "!CUSTOM!",
                   "tchar-only custom method parses");

    const auto skipped = parsed_of(orange::http::parse_request("\r\nGET / HTTP/1.1\r\n"));
    reporter.check(skipped && skipped->request.method == "GET",
                   "a leading empty line is ignored (RFC 9112 2.2)");
    reporter.check(skipped && skipped->consumed == 18,
                   "consumed includes the skipped leading empty line");

    // --- malformed: the bytes cannot be a request line --------------------
    reporter.check(
        is_error(orange::http::parse_request("GET\r\n"), orange::http::ParseError::malformed),
        "one-token request line is malformed");
    reporter.check(
        is_error(orange::http::parse_request("GET /\r\n"), orange::http::ParseError::malformed),
        "two-token request line is malformed");
    reporter.check(is_error(orange::http::parse_request("GET  / HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "empty token from a doubled space is malformed");
    reporter.check(is_error(orange::http::parse_request(" GET / HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "leading space leaves an empty method: malformed");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1 extra\r\n"),
                            orange::http::ParseError::malformed),
                   "fourth token in the request line is malformed");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\n"),
                            orange::http::ParseError::malformed),
                   "LF without CR does not terminate a request line: malformed");
    reporter.check(is_error(orange::http::parse_request("GET\r\nX-Header: a b\r\n"),
                            orange::http::ParseError::malformed),
                   "tokens cannot cross the first CRLF: malformed");
    reporter.check(is_error(orange::http::parse_request("GET / HT\rTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "bare CR inside the line is rejected (RFC 9112 2.2)");
    reporter.check(is_error(orange::http::parse_request("GE(T / HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "method must contain only tchar characters");
    reporter.check(is_error(orange::http::parse_request("GET / banana\r\n"),
                            orange::http::ParseError::malformed),
                   "version must match HTTP/DIGIT.DIGIT");
    reporter.check(is_error(orange::http::parse_request("GET / http/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "HTTP-name is case-sensitive");
    // The fixture is a string_view literal with an explicit NUL byte: a
    // `\x00b` escape would swallow the 'b' as a hex digit (yielding VT), and
    // a plain const char* would be truncated at the NUL by strlen.
    constexpr std::string_view nul_line = "GET /a\0b HTTP/1.1\r\n"sv;
    reporter.check(nul_line.size() == 19 && nul_line[6] == '\0',
                   "NUL fixture carries its NUL byte inside the target");
    reporter.check(
        is_error(orange::http::parse_request(nul_line), orange::http::ParseError::malformed),
        "NUL in the target is rejected");
    reporter.check(is_error(orange::http::parse_request("GET /a\vb HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "VT in the target is rejected");
    reporter.check(is_error(orange::http::parse_request("GET /a\tb HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "TAB in the target is rejected");

    // Target octets follow the URI character set every request-target form
    // is built from (RFC 9112 3.2, RFC 3986 2.1-2.3).
    reporter.check(is_error(orange::http::parse_request("GET /a\x7F HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "DEL in the target is rejected");
    reporter.check(is_error(orange::http::parse_request("GET /caf\xC3\xA9 HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "raw non-ASCII bytes in the target are rejected");
    reporter.check(is_error(orange::http::parse_request("GET /a#frag HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "a fragment is not part of any request-target form");
    reporter.check(is_error(orange::http::parse_request("GET /a<b> HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "characters outside the URI set are rejected");
    reporter.check(is_error(orange::http::parse_request("GET /a%G1 HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "percent-encoding requires two hex digits");
    reporter.check(is_error(orange::http::parse_request("GET /a%2 HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "a truncated percent-encoding in a complete line is malformed");

    const auto asterisk = parsed_of(orange::http::parse_request("OPTIONS * HTTP/1.1\r\n"));
    reporter.check(asterisk && asterisk->request.target == "*", "asterisk-form target parses");
    const auto absolute =
        parsed_of(orange::http::parse_request("GET http://example.com:8080/a?b=c HTTP/1.1\r\n"));
    reporter.check(absolute && absolute->request.target == "http://example.com:8080/a?b=c",
                   "absolute-form target parses");
    const auto authority =
        parsed_of(orange::http::parse_request("CONNECT example.com:443 HTTP/1.1\r\n"));
    reporter.check(authority && authority->request.target == "example.com:443",
                   "authority-form target parses");

    return reporter.exit_status();
}
