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

// The parsed request when the outcome holds one, else nullptr.
const orange::http::ParsedRequest *parsed_of(const ParseOutcome &outcome) {
    return std::get_if<orange::http::ParsedRequest>(&outcome);
}

} // namespace

int main() {
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

    // --- too_large: head budget exhausted without a line end -------------
    const std::string cap_filled(orange::http::header_block_cap, 'A');
    reporter.check(
        is_error(orange::http::parse_request(cap_filled), orange::http::ParseError::too_large),
        "cap-sized input without a line end is too_large");

    // --- parses: fields, validation breadth, consumed bytes --------------
    const auto posted = orange::http::parse_request("POST /api/echo HTTP/1.1\r\n");
    reporter.check(parsed_of(posted) != nullptr, "well-formed request line parses");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.method == "POST",
                   "request-line method parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.target == "/api/echo",
                   "request-line target parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.version == "HTTP/1.1",
                   "request-line version parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->consumed == 25,
                   "consumed spans the request line and its CRLF");

    const auto *const future = parsed_of(orange::http::parse_request("GET / HTTP/2.0\r\n"));
    reporter.check(future != nullptr && future->request.version == "HTTP/2.0",
                   "grammar-valid future version parses; serving policy is a later layer");

    const auto *const custom =
        parsed_of(orange::http::parse_request("!CUSTOM! /a%20b?q=1 HTTP/1.1\r\n"));
    reporter.check(custom != nullptr && custom->request.method == "!CUSTOM!",
                   "tchar-only custom method parses");

    const auto *const skipped = parsed_of(orange::http::parse_request("\r\nGET / HTTP/1.1\r\n"));
    reporter.check(skipped != nullptr && skipped->request.method == "GET",
                   "a leading empty line is ignored (RFC 9112 2.2)");
    reporter.check(skipped != nullptr && skipped->consumed == 18,
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
    reporter.check(is_error(orange::http::parse_request("GET /a\x00b HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "NUL in the target is rejected");
    reporter.check(is_error(orange::http::parse_request("GET /a\tb HTTP/1.1\r\n"),
                            orange::http::ParseError::malformed),
                   "TAB in the target is rejected");

    return reporter.exit_status();
}
