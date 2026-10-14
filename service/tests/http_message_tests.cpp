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

#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <new>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

// Measures ordinary heap allocations only during a parser call; fixture
// construction and reporter diagnostics are outside the measurement.
thread_local bool count_allocations = false;
thread_local std::size_t allocation_count = 0;

} // namespace

// Keeps real allocation behavior while counting the parser's memory cost.
void *operator new(std::size_t size) {
    if (void *storage = std::malloc(size == 0 ? 1 : size)) {
        if (count_allocations) {
            ++allocation_count;
        }
        return storage;
    }
    throw std::bad_alloc();
}

// Counts array allocations through the same real allocator.
void *operator new[](std::size_t size) { return ::operator new(size); }

// Releases the storage obtained by the counting allocator.
void operator delete(void *storage) noexcept { std::free(storage); }

// Releases arrays obtained by the counting allocator.
void operator delete[](void *storage) noexcept { ::operator delete(storage); }

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

// Limits allocation tracking to one parse, including all error exits.
class AllocationCounter {
  public:
    // Starts a fresh measurement on this test thread.
    AllocationCounter() {
        allocation_count = 0;
        count_allocations = true;
    }
    // Stops tracking before diagnostics or fixture cleanup can allocate.
    ~AllocationCounter() { count_allocations = false; }

    // A measurement scope has exactly one owner.
    AllocationCounter(const AllocationCounter &) = delete;
    AllocationCounter &operator=(const AllocationCounter &) = delete;

    // Reports the parser's observed allocation cost without allocating.
    [[nodiscard]] std::size_t count() const { return allocation_count; }
};

// Associates a real parse outcome with its measured heap allocation count.
struct CountedParse {
    ParseOutcome outcome;
    std::size_t allocations;
};

// Excludes result inspection and diagnostics from the allocation count.
CountedParse parse_counted(std::string_view raw) {
    AllocationCounter counter;
    auto outcome = orange::http::parse_request(raw);
    return {std::move(outcome), counter.count()};
}

// Catches repeated materialization of incomplete/rejected heads and vector
// growth while preserving the existing grammar and owned-result semantics.
void check_parse_allocations(CheckReporter &reporter) {
    const auto empty = parse_counted("GET / HTTP/1.1\r\n\r\n");
    reporter.check(std::holds_alternative<orange::http::ParsedRequest>(empty.outcome) &&
                       empty.allocations == 0,
                   "a short head without fields remains allocation-free");

    std::string head = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 16; ++i) {
        head += "X-A: a\r\n";
    }
    const auto incomplete = parse_counted(head);
    reporter.check(is_error(incomplete.outcome, orange::http::ParseError::incomplete) &&
                       incomplete.allocations == 0,
                   "complete fields in an unfinished head do not allocate owned results");

    const auto rejected = parse_counted(head + " Bad: folded\r\n\r\n");
    reporter.check(is_error(rejected.outcome, orange::http::ParseError::malformed) &&
                       rejected.allocations == 0,
                   "a malformed field after valid fields rejects without allocations");

    const auto oversized =
        parse_counted(head + "X-Pad: " + std::string(orange::http::header_block_cap, 'a'));
    reporter.check(is_error(oversized.outcome, orange::http::ParseError::headers_too_long) &&
                       oversized.allocations == 0,
                   "a header overflow after valid fields rejects without allocations");

    head += "\r\n";
    const auto complete = parse_counted(head + "body\r\nBad Header: ignored\r\n");
    const auto *parsed = std::get_if<orange::http::ParsedRequest>(&complete.outcome);
    reporter.check(parsed && parsed->consumed == 146 && parsed->request.headers.size() == 16 &&
                       complete.allocations <= 1,
                   "sixteen short fields use at most one allocation and consume only the head");

    const std::string long_target = "/" + std::string(64, 'a');
    const auto long_incomplete =
        parse_counted("CUSTOM-METHOD-WITH-LONG-NAME " + long_target +
                      " HTTP/1.1\r\nX-Long: " + std::string(128, 'b') + "\r\n");
    reporter.check(is_error(long_incomplete.outcome, orange::http::ParseError::incomplete) &&
                       long_incomplete.allocations == 0,
                   "unfinished heads do not allocate long request-line or field copies");
}

// Catches incorrect deferred offsets, borrowed result fields, or a scratch
// bound too small for the grammar's shortest valid header lines.
void check_deferred_field_copies(CheckReporter &reporter) {
    using namespace std::string_view_literals;
    std::string source = "GET /owned HTTP/1.1\r\nx-a:\t \r\nX-Note: " + std::string(64, 'n') +
                         " \t\r\nx-a: caf\xC3\xA9\r\n\r\n";
    const auto owned = orange::http::parse_request(source);
    source.assign(source.size(), 'x');
    const auto *parsed = std::get_if<orange::http::ParsedRequest>(&owned);
    reporter.check(parsed && parsed->request.method == "GET" &&
                       parsed->request.target == "/owned" && parsed->request.version == "HTTP/1.1",
                   "deferred request-line copies survive reuse of the input buffer");
    reporter.check(parsed && parsed->request.headers.size() == 3 &&
                       parsed->request.headers[0].name == "x-a" &&
                       parsed->request.headers[0].value.empty() &&
                       parsed->request.headers[1].name == "X-Note" &&
                       parsed->request.headers[1].value == std::string(64, 'n') &&
                       parsed->request.headers[2].name == "x-a" &&
                       parsed->request.headers[2].value == "caf\xC3\xA9"sv,
                   "deferred fields keep empty, trimmed, duplicate, and obs-text values owned");

    std::string dense = "\r\nGET / HTTP/1.1\r\n";
    for (int i = 0; i < 2043; ++i) {
        dense += "a:\r\n";
    }
    dense += "\r\n";
    const auto fitted = parse_counted(dense);
    const auto *full = std::get_if<orange::http::ParsedRequest>(&fitted.outcome);
    reporter.check(full && full->consumed == 8192 && full->request.headers.size() == 2043 &&
                       full->request.headers.back().name == "a" &&
                       full->request.headers.back().value.empty() && fitted.allocations <= 1,
                   "shortest field lines fitting the head budget use bounded scratch offsets");
}

// Catches signed-byte classification, accepted CTLs/DEL, and unchecked
// interior octets. The accepted ranges come from RFC 9110 5.5: HTAB,
// SP/VCHAR, and obs-text; internal OWS must survive the owned copy.
void check_field_value_octets(CheckReporter &reporter) {
    constexpr std::array<std::size_t, 8> positions{1, 7, 15, 16, 31, 32, 47, 62};
    for (unsigned int byte = 0; byte < 256; ++byte) {
        const bool accepted = byte == 0x09 || (byte >= 0x20 && byte <= 0x7E) || byte >= 0x80;
        for (const std::size_t position : positions) {
            std::string value(64, 'a');
            value[position] = static_cast<char>(byte);
            const std::string raw = "GET / HTTP/1.1\r\nX-Value: " + value + "\r\n\r\n";
            const auto outcome = orange::http::parse_request(raw);
            const auto *parsed = std::get_if<orange::http::ParsedRequest>(&outcome);
            if (accepted) {
                reporter.check(parsed && parsed->consumed == raw.size() &&
                                   parsed->request.headers.size() == 1 &&
                                   parsed->request.headers[0].value == value,
                               "every allowed interior field octet is preserved");
            } else {
                reporter.check(is_error(outcome, orange::http::ParseError::malformed),
                               "every forbidden interior field octet is rejected");
            }
        }
    }
}

// Catches skipped short tails, out-of-range reads, and OWS or body bytes
// entering a value when validation changes its processing width.
void check_field_value_boundaries(CheckReporter &reporter) {
    constexpr std::array<std::size_t, 16> lengths{0,  1,  7,  8,  15, 16,  17,  31,
                                                  32, 33, 63, 64, 65, 127, 128, 129};
    for (const std::size_t length : lengths) {
        for (std::size_t padding = 0; padding < 16; ++padding) {
            const std::string prefix = "GET / HTTP/1.1\r\nX-Value:" + std::string(padding, '\t');
            const std::string value(length, '\xFF');
            const std::string raw = prefix + value + " \t\r\n\r\nbody";
            const auto outcome = orange::http::parse_request(raw);
            const auto *parsed = std::get_if<orange::http::ParsedRequest>(&outcome);
            reporter.check(parsed && parsed->consumed == raw.size() - 4 &&
                               parsed->request.headers.size() == 1 &&
                               parsed->request.headers[0].value == value,
                           "short and long obs-text values trim OWS and leave the body unread");
        }
        for (std::size_t position = 0; position < length; ++position) {
            std::string value(length, 'a');
            value[position] = '\x7F';
            reporter.check(is_error(orange::http::parse_request(
                                        "GET / HTTP/1.1\r\nX-Value: " + value + "\r\n\r\n"),
                                    orange::http::ParseError::malformed),
                           "DEL is rejected at every position including the last partial span");
        }
    }
}

// Catches trimming across a non-OWS byte, retaining SP/HTAB at the edges,
// and losing interior whitespace or the owned slice at processing boundaries.
// RFC 9110 5.6.3 permits only SP and HTAB in the surrounding OWS.
void check_ows_trimming(CheckReporter &reporter) {
    constexpr std::array<std::size_t, 17> lengths{0,  1,  7,  15,  16,  17,  31,  32,  33,
                                                  63, 64, 65, 127, 128, 129, 511, 2048};
    for (const std::size_t length : lengths) {
        for (int pattern = 0; pattern < 3; ++pattern) {
            std::string padding(length, pattern == 1 ? '\t' : ' ');
            if (pattern == 2) {
                for (std::size_t i = 1; i < length; i += 2) {
                    padding[i] = '\t';
                }
            }
            const std::string raw =
                "GET / HTTP/1.1\r\nX:" + padding + "v \t\xFF" + padding + "\r\n\r\nbody";
            const auto outcome = orange::http::parse_request(raw);
            const auto *parsed = std::get_if<orange::http::ParsedRequest>(&outcome);
            reporter.check(parsed && parsed->consumed == raw.size() - 4 &&
                               parsed->request.headers.size() == 1 &&
                               parsed->request.headers[0].value == "v \t\xFF",
                           "surrounding OWS is removed while interior SP/HTAB and obs-text remain");
            const std::string empty = "GET / HTTP/1.1\r\nX:" + padding + "\r\n\r\n";
            const auto counted = parse_counted(empty);
            const auto *trimmed = std::get_if<orange::http::ParsedRequest>(&counted.outcome);
            reporter.check(trimmed && trimmed->consumed == empty.size() &&
                               trimmed->request.headers.size() == 1 &&
                               trimmed->request.headers[0].value.empty() &&
                               counted.allocations <= 1,
                           "all-OWS values become empty without adding allocations");
        }
    }
    const std::string fitted = "GET / HTTP/1.1\r\nX:" + std::string(8170, '\t') + "\r\n\r\n";
    const auto owned = orange::http::parse_request(fitted);
    const auto *parsed = std::get_if<orange::http::ParsedRequest>(&owned);
    reporter.check(parsed && parsed->consumed == 8192 && parsed->request.headers.size() == 1 &&
                       parsed->request.headers[0].value.empty(),
                   "an all-OWS field can complete at the shared 8 KiB cap");
}

// Catches treating any non-OWS octet as whitespace when a grouped trim
// reaches it from either edge. Grammar expectations come from RFC 9110
// 5.5 and 5.6.3, independently of the parser's classifiers.
void check_ows_edge_octets(CheckReporter &reporter) {
    constexpr std::array<std::size_t, 9> lengths{15, 16, 17, 31, 32, 33, 63, 64, 65};
    for (const std::size_t length : lengths) {
        for (unsigned int byte = 0; byte < 256; ++byte) {
            const std::string raw = "GET / HTTP/1.1\r\nX:" + std::string(length, ' ') +
                                    static_cast<char>(byte) + std::string(length, '\t') +
                                    "\r\n\r\n";
            const auto outcome = orange::http::parse_request(raw);
            const auto *parsed = std::get_if<orange::http::ParsedRequest>(&outcome);
            if (byte == 0x09 || byte == 0x20) {
                reporter.check(parsed && parsed->consumed == raw.size() &&
                                   parsed->request.headers.size() == 1 &&
                                   parsed->request.headers[0].value.empty(),
                               "SP and HTAB at either trimming boundary are OWS");
            } else if ((byte >= 0x21 && byte <= 0x7E) || byte >= 0x80) {
                reporter.check(parsed && parsed->consumed == raw.size() &&
                                   parsed->request.headers.size() == 1 &&
                                   parsed->request.headers[0].value ==
                                       std::string(1, static_cast<char>(byte)),
                               "every non-OWS field-content octet stops trimming at both edges");
            } else {
                reporter.check(is_error(outcome, orange::http::ParseError::malformed),
                               "forbidden octets beside long OWS remain malformed");
            }
        }
    }
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
    reporter.check(is_error(orange::http::parse_request(line_at_cap),
                            orange::http::ParseError::headers_too_long),
                   "a request line ending on the budget's last byte leaves no room for the "
                   "terminating empty line: headers_too_long");
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
    // The head is the request line AND its header block through the one
    // terminating empty line; a request line alone never completes a parse.
    const auto posted = orange::http::parse_request("POST /api/echo HTTP/1.1\r\n\r\n");
    reporter.check(parsed_of(posted).has_value(), "well-formed request line parses");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.method == "POST",
                   "request-line method parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.target == "/api/echo",
                   "request-line target parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.version == "HTTP/1.1",
                   "request-line version parses into its field");
    reporter.check(parsed_of(posted) && parsed_of(posted)->consumed == 27,
                   "consumed spans the request line and the terminating empty line");
    reporter.check(parsed_of(posted) && parsed_of(posted)->request.headers.empty(),
                   "a head with no field lines collects an empty header list");

    const auto future = parsed_of(orange::http::parse_request("GET / HTTP/2.0\r\n\r\n"));
    reporter.check(future && future->request.version == "HTTP/2.0",
                   "grammar-valid future version parses; serving policy is a later layer");

    const auto custom =
        parsed_of(orange::http::parse_request("!CUSTOM! /a%20b?q=1 HTTP/1.1\r\n\r\n"));
    reporter.check(custom && custom->request.method == "!CUSTOM!",
                   "tchar-only custom method parses");

    const auto skipped = parsed_of(orange::http::parse_request("\r\nGET / HTTP/1.1\r\n\r\n"));
    reporter.check(skipped && skipped->request.method == "GET",
                   "a leading empty line is ignored (RFC 9112 2.2)");
    reporter.check(skipped && skipped->consumed == 20,
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

    const auto asterisk = parsed_of(orange::http::parse_request("OPTIONS * HTTP/1.1\r\n\r\n"));
    reporter.check(asterisk && asterisk->request.target == "*", "asterisk-form target parses");
    const auto absolute = parsed_of(
        orange::http::parse_request("GET http://example.com:8080/a?b=c HTTP/1.1\r\n\r\n"));
    reporter.check(absolute && absolute->request.target == "http://example.com:8080/a?b=c",
                   "absolute-form target parses");
    const auto authority =
        parsed_of(orange::http::parse_request("CONNECT example.com:443 HTTP/1.1\r\n\r\n"));
    reporter.check(authority && authority->request.target == "example.com:443",
                   "authority-form target parses");

    // --- the header block: fields, order, trimming, consumed -------------
    const auto with_fields = parsed_of(orange::http::parse_request(
        "POST /api/echo HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nContent-Length: 5\r\n\r\n"));
    reporter.check(with_fields.has_value(), "a head with field lines parses");
    reporter.check(with_fields && with_fields->request.headers.size() == 2,
                   "every field line is collected");
    reporter.check(with_fields && with_fields->request.headers.size() == 2 &&
                       with_fields->request.headers[0].name == "Host" &&
                       with_fields->request.headers[0].value == "127.0.0.1:8080",
                   "field name and value parse into their owned copies");
    reporter.check(with_fields && with_fields->request.headers.size() == 2 &&
                       with_fields->request.headers[1].name == "Content-Length" &&
                       with_fields->request.headers[1].value == "5",
                   "a second field line parses");
    reporter.check(with_fields && with_fields->consumed == 68,
                   "consumed spans the request line, both field lines, and the terminator");

    // Optional whitespace around a value is not part of it (RFC 9110 5.5).
    const auto trimmed = parsed_of(orange::http::parse_request(
        "GET / HTTP/1.1\r\nX-Pad:  spaced  \r\nX-Tab:\tvalue\t\r\nEmpty:\r\nOnly-OWS:   "
        "\r\n\r\n"));
    reporter.check(trimmed && trimmed->request.headers.size() == 4,
                   "four field lines collect before the terminator");
    reporter.check(trimmed && trimmed->request.headers.size() == 4 &&
                       trimmed->request.headers[0].value == "spaced",
                   "OWS around a value is trimmed");
    reporter.check(trimmed && trimmed->request.headers.size() == 4 &&
                       trimmed->request.headers[1].value == "value",
                   "HTAB around a value is trimmed");
    reporter.check(trimmed && trimmed->request.headers.size() == 4 &&
                       trimmed->request.headers[2].value.empty() &&
                       trimmed->request.headers[3].value.empty(),
                   "a value of only OWS trims to the empty value");

    const auto repeated = parsed_of(
        orange::http::parse_request("GET / HTTP/1.1\r\nx-a: 1\r\nX-B: 2\r\nx-a: 3\r\n\r\n"));
    reporter.check(repeated && repeated->request.headers.size() == 3,
                   "duplicate field names stay separate entries in received order");
    reporter.check(repeated && repeated->request.headers.size() == 3 &&
                       repeated->request.headers[0].name == "x-a" &&
                       repeated->request.headers[2].value == "3",
                   "field-name case is preserved as received");
    reporter.check(repeated && repeated->request.headers.size() == 3 &&
                       repeated->request.headers[1].name == "X-B" &&
                       repeated->request.headers[1].value == "2",
                   "the middle field keeps its position and value");
    reporter.check(repeated && repeated->request.headers.size() == 3 &&
                       repeated->request.headers[2].name == "x-a",
                   "the repeated name is preserved verbatim, not combined");

    const auto agent = parsed_of(orange::http::parse_request(
        "GET / HTTP/1.1\r\nUser-Agent: orange study (loopback)\r\n\r\n"));
    reporter.check(agent && !agent->request.headers.empty() &&
                       agent->request.headers[0].value == "orange study (loopback)",
                   "internal spaces inside a value are kept");

    constexpr std::string_view noted_line = "GET / HTTP/1.1\r\nX-Note: caf\xC3\xA9\r\n\r\n"sv;
    const auto noted = parsed_of(orange::http::parse_request(noted_line));
    reporter.check(noted && !noted->request.headers.empty() &&
                       noted->request.headers[0].value == "caf\xC3\xA9"sv,
                   "obs-text octets are allowed in a field value (RFC 9110 5.5)");

    // --- the header block overflows the remaining head budget (431) ------
    reporter.check(
        is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-Pad: " + std::string(cap, 'a')),
                 orange::http::ParseError::headers_too_long),
        "a header block that cannot end inside the budget is headers_too_long (431)");
    reporter.check(is_error(orange::http::parse_request("GET /" + std::string(cap - 17, 'a') +
                                                        " HTTP/1.1\r\n\r\n"),
                            orange::http::ParseError::headers_too_long),
                   "a head one byte past the budget is headers_too_long");
    const auto fitted = parsed_of(
        orange::http::parse_request("GET /" + std::string(cap - 18, 'a') + " HTTP/1.1\r\n\r\n"));
    reporter.check(fitted && fitted->consumed == cap,
                   "a head ending exactly on the budget's last byte parses");
    reporter.check(is_error(orange::http::parse_request(
                                "GET / HTTP/1.1\r\nX-Pad: " + std::string(cap, 'a') + "\r"),
                            orange::http::ParseError::headers_too_long),
                   "a CR on the budget's last byte is half of a CRLF, not a malformed tail");

    // An overflowing tail that already breaks the field-line grammar stays
    // malformed (400), like the request-line classifier.
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-Pad: a\x7F" +
                                                        std::string(cap, 'a')),
                            orange::http::ParseError::malformed),
                   "DEL in an overflowing field tail is malformed");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-Pad: a\rb" +
                                                        std::string(cap, 'a')),
                            orange::http::ParseError::malformed),
                   "a bare CR in an overflowing field tail is malformed");

    // --- the header block so far is incomplete ---------------------------
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\n"),
                            orange::http::ParseError::incomplete),
                   "a request line without its terminating empty line is incomplete");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nHost: x"),
                            orange::http::ParseError::incomplete),
                   "a partial field line inside the budget is incomplete");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nHost: x\r\n"),
                            orange::http::ParseError::incomplete),
                   "a field block without its terminating empty line is incomplete");

    // --- malformed: the bytes cannot be a field line ---------------------
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nNoColonHere\r\n\r\n"),
                            orange::http::ParseError::malformed),
                   "a field line without a colon is malformed");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\n: v\r\n\r\n"),
                            orange::http::ParseError::malformed),
                   "an empty field name is malformed");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nBad Name: v\r\n\r\n"),
                            orange::http::ParseError::malformed),
                   "whitespace between field name and colon is rejected (RFC 9112 5.1)");
    reporter.check(
        is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-A: 1\r\n continued\r\n\r\n"),
                 orange::http::ParseError::malformed),
        "obs-fold continued by a space is rejected (RFC 9112 5.2)");
    reporter.check(
        is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-A: 1\r\n\tcontinued\r\n\r\n"),
                 orange::http::ParseError::malformed),
        "obs-fold continued by a tab is rejected (RFC 9112 5.2)");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-A: v\n\r\n"),
                            orange::http::ParseError::malformed),
                   "LF without CR does not terminate a field line");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-A: a\rb\r\n\r\n"),
                            orange::http::ParseError::malformed),
                   "a bare CR inside a value is rejected (RFC 9112 2.2)");
    constexpr std::string_view nul_value = "GET / HTTP/1.1\r\nX-A: a\0b\r\n\r\n"sv;
    reporter.check(nul_value.size() == 28 && nul_value[22] == '\0',
                   "NUL fixture carries its NUL byte inside the value");
    reporter.check(
        is_error(orange::http::parse_request(nul_value), orange::http::ParseError::malformed),
        "NUL in a field value is rejected");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-A: a\vb\r\n\r\n"),
                            orange::http::ParseError::malformed),
                   "VT in a field value is rejected");
    reporter.check(is_error(orange::http::parse_request("GET / HTTP/1.1\r\nX-A: a\x7F"
                                                        "b\r\n\r\n"),
                            orange::http::ParseError::malformed),
                   "DEL in a field value is rejected");

    check_parse_allocations(reporter);
    check_deferred_field_copies(reporter);
    check_field_value_octets(reporter);
    check_field_value_boundaries(reporter);
    check_ows_trimming(reporter);
    check_ows_edge_octets(reporter);
    return reporter.exit_status();
}
