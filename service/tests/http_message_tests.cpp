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
#include <string_view>

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

} // namespace

int main() {
    CheckReporter reporter;

    // Wiring sanity: proves the executable runs and that a passing check
    // yields exit status 0 under CTest. The development sequence first
    // observed a deliberately failing check; the evidence is recorded with
    // this change's verification runs.
    reporter.check(1 + 1 == 2, "harness wiring: arithmetic sanity");

    // First behavioral contract of the T-1 ladder: empty raw input cannot
    // be an HTTP/1.1 request at all, so the parser must reject it with
    // std::nullopt (http_messages.hpp, parse_request).
    reporter.check(!orange::http::parse_request("").has_value(),
                   "parse_request rejects empty input");

    // Second behavioral contract: the request line must be
    // `method SP request-target SP HTTP-version CRLF` with three non-empty
    // tokens, parsed into the named Request fields. Bytes after the request
    // line are not consumed yet — header and body parsing are later tasks.
    const auto posted = orange::http::parse_request("POST /api/echo HTTP/1.1\r\n");
    reporter.check(posted.has_value(), "well-formed request line parses");
    reporter.check(posted && posted->method == "POST", "request-line method parses into its field");
    reporter.check(posted && posted->target == "/api/echo",
                   "request-line target parses into its field");
    reporter.check(posted && posted->version == "HTTP/1.1",
                   "request-line version parses into its field");

    reporter.check(!orange::http::parse_request("GET\r\n").has_value(),
                   "one-token request line is rejected");
    reporter.check(!orange::http::parse_request("GET /\r\n").has_value(),
                   "two-token request line is rejected");
    reporter.check(!orange::http::parse_request("GET  / HTTP/1.1\r\n").has_value(),
                   "empty token from a doubled space is rejected");
    reporter.check(!orange::http::parse_request(" GET / HTTP/1.1\r\n").has_value(),
                   "leading space leaves an empty method and is rejected");
    reporter.check(!orange::http::parse_request("GET / HTTP/1.1 extra\r\n").has_value(),
                   "fourth token in the request line is rejected");
    reporter.check(!orange::http::parse_request("GET / HTTP/1.1\n").has_value(),
                   "LF without CR does not terminate a request line");
    reporter.check(!orange::http::parse_request("GET / HTTP/1.1").has_value(),
                   "input without CRLF is rejected");

    // Line-boundary pin, landed before the terminator-search refactor: all
    // three tokens must live before the first CRLF, so a first space that
    // appears only after it cannot satisfy the grammar. A cursor-style
    // walker without a line bound silently accepted this input class.
    reporter.check(!orange::http::parse_request("GET\r\nX-Header: a b\r\n").has_value(),
                   "tokens cannot cross the first CRLF");

    // Bare-CR characterization: a lone '\r' inside a token is outside the
    // declared grammar; this pins today's behavior (the byte stays part of
    // the token) so a deliberate later rejection lands as a visible
    // behavior change, not a silent regression.
    const auto bare_cr = orange::http::parse_request("GET / HT\rTP/1.1\r\n");
    reporter.check(bare_cr.has_value() && bare_cr->version == "HT\rTP/1.1",
                   "bare CR inside a token stays part of the token");

    return reporter.exit_status();
}
