// Governed by docs/adr/ADR-0003-local-runtime-path.md.

#include "http_messages.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace orange::http {
namespace {

constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }

// RFC 9110 5.6.2 token characters: the only bytes a method may contain.
constexpr bool is_tchar(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '!' ||
           c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' ||
           c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
}

// A byte the request target may contain: anything but SP, the C0 controls,
// and DEL. The full origin-form grammar is a later task; control bytes must
// not ride through unvalidated.
constexpr bool is_target_byte(char c) {
    const auto byte = static_cast<unsigned char>(c);
    return byte > 0x20 && byte != 0x7F;
}

} // namespace

// Parses the CRLF-terminated request line `method SP request-target SP
// HTTP-version` into owned Request fields, skipping leading empty lines
// first (RFC 9112 2.2). The line end is located by finding the first '\n'
// and verifying the byte before it is '\r': finding the first '\n'
// preceded by '\r' is equivalent to finding the first CRLF (an earlier pair
// would contain an earlier '\n'), and a single-char search has no restart
// loop, so CR-dense garbage cannot degrade it. Every search is bounded by
// header_block_cap: without a line end inside the budget the outcome is
// too_large (never a rescan of unbounded input), and inside it,
// incomplete. A bare CR inside the line is rejected (RFC 9112 2.2), and
// each field must pass its grammar (see the header). Bytes after the
// request line are not consumed yet — header and body parsing are later
// tasks of the T-1 ladder.
std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw) {
    const std::size_t limit = std::min(raw.size(), header_block_cap);

    std::size_t skipped = 0;
    while (skipped + 1 < limit && raw[skipped] == '\r' && raw[skipped + 1] == '\n') {
        skipped += 2;
    }

    const std::string_view window = raw.substr(skipped, limit - skipped);
    const std::size_t line_feed = window.find('\n');
    if (line_feed == std::string_view::npos) {
        return ParseError::incomplete;
    }
    if (line_feed == 0 || window[line_feed - 1] != '\r') {
        return ParseError::malformed;
    }
    const std::string_view line = window.substr(0, line_feed - 1);
    if (line.find('\r') != std::string_view::npos) {
        return ParseError::malformed;
    }

    const std::size_t first_space = line.find(' ');
    if (first_space == std::string_view::npos) {
        return ParseError::malformed;
    }
    const std::size_t second_space = line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos) {
        return ParseError::malformed;
    }
    const std::string_view method = line.substr(0, first_space);
    const std::string_view target = line.substr(first_space + 1, second_space - first_space - 1);
    const std::string_view version = line.substr(second_space + 1);

    const bool method_valid =
        !method.empty() && std::all_of(method.begin(), method.end(), is_tchar);
    const bool version_valid = version.size() == 8 && version.starts_with("HTTP/") &&
                               is_digit(version[5]) && version[6] == '.' && is_digit(version[7]);
    const bool target_valid =
        !target.empty() && std::all_of(target.begin(), target.end(), is_target_byte);
    if (!method_valid || !version_valid || !target_valid) {
        return ParseError::malformed;
    }

    return ParsedRequest{
        Request{std::string(method), std::string(target), std::string(version)},
        skipped + line_feed + 1,
    };
}

} // namespace orange::http
