// Governed by docs/adr/ADR-0003-local-runtime-path.md.

#include "http_messages.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace orange::http {
namespace {

// Byte-classification table for the request-line grammar: one 256-entry
// bitmask built at compile time, so every classifier below is a single
// table load encoding the same byte sets the grammar names (RFC 9110
// 5.6.2, RFC 3986 2.1-2.3, RFC 9112 3.2).
struct ByteClassTable {
    enum Mask : std::uint8_t {
        digit = 1 << 0,
        hexdig = 1 << 1,
        tchar = 1 << 2,
        target_char = 1 << 3,
    };

    std::uint8_t masks[256] = {};

    constexpr ByteClassTable() {
        constexpr std::string_view tchar_specials = "!#$%&'*+-.^_`|~";
        constexpr std::string_view target_specials = "-._~!$&'()*+,;=:@/?[]";
        for (int i = 0; i < 256; ++i) {
            const char c = static_cast<char>(i);
            std::uint8_t mask = 0;
            const bool dig = c >= '0' && c <= '9';
            const bool alp = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if (dig) {
                mask |= digit;
            }
            if (dig || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                mask |= hexdig;
            }
            if (dig || alp || tchar_specials.find(c) != std::string_view::npos) {
                mask |= tchar;
            }
            if (dig || alp || target_specials.find(c) != std::string_view::npos) {
                mask |= target_char;
            }
            masks[i] = mask;
        }
    }
};

constexpr ByteClassTable byte_classes{};

constexpr bool is_digit(char c) {
    return (byte_classes.masks[static_cast<unsigned char>(c)] & ByteClassTable::digit) != 0;
}

constexpr bool is_hexdig(char c) {
    return (byte_classes.masks[static_cast<unsigned char>(c)] & ByteClassTable::hexdig) != 0;
}

// RFC 9110 5.6.2 token characters: the only bytes a method may contain.
constexpr bool is_tchar(char c) {
    return (byte_classes.masks[static_cast<unsigned char>(c)] & ByteClassTable::tchar) != 0;
}

// A URI character a request-target may carry outside a percent-encoding:
// RFC 3986 unreserved and sub-delims plus the gen-delims the request-target
// forms use (RFC 9112 3.2). '#' is excluded because no form carries a
// fragment; every other byte, including SP, controls, DEL, and non-ASCII,
// must arrive percent-encoded.
constexpr bool is_target_char(char c) {
    return (byte_classes.masks[static_cast<unsigned char>(c)] & ByteClassTable::target_char) != 0;
}

bool is_valid_method(std::string_view method) {
    return !method.empty() && std::all_of(method.begin(), method.end(), is_tchar);
}

// Checks target octets against the URI character set with well-formed
// `%` HEXDIG HEXDIG encodings, indexing directly instead of slicing a view
// per escape. A `truncated` target is a prefix cut off by the head budget,
// so a percent-encoding cut at its end is acceptable.
bool is_valid_target_octets(std::string_view target, bool truncated) {
    const std::size_t size = target.size();
    for (std::size_t i = 0; i < size; ++i) {
        const char c = target[i];
        if (c != '%') {
            if (!is_target_char(c)) {
                return false;
            }
            continue;
        }
        if (i + 2 < size) {
            if (!is_hexdig(target[i + 1]) || !is_hexdig(target[i + 2])) {
                return false;
            }
            i += 2;
        } else {
            if (i + 1 < size && !is_hexdig(target[i + 1])) {
                return false;
            }
            return truncated;
        }
    }
    return true;
}

// True when `version` is a prefix of HTTP-version = %s"HTTP" "/" DIGIT "."
// DIGIT (RFC 9112 2.3); a complete version is the full eight bytes.
bool is_version_prefix(std::string_view version) {
    constexpr std::string_view shape = "HTTP/0.0";
    if (version.size() > shape.size()) {
        return false;
    }
    for (std::size_t i = 0; i < version.size(); ++i) {
        const bool matches = shape[i] == '0' ? is_digit(version[i]) : version[i] == shape[i];
        if (!matches) {
            return false;
        }
    }
    return true;
}

// Classifies a request line whose CRLF did not arrive inside the head
// budget. A prefix that already breaks the grammar is malformed; otherwise
// the line is too long (RFC 9112 3), blamed on the longer of method and
// target, with a tie going to the target's MUST-level 414.
ParseError classify_unterminated(std::string_view partial) {
    if (partial.ends_with('\r')) {
        // The first half of a CRLF that ends past the budget, not a bare CR.
        partial.remove_suffix(1);
    }
    if (partial.find('\r') != std::string_view::npos) {
        return ParseError::malformed;
    }
    const std::size_t first_space = partial.find(' ');
    const std::string_view method = partial.substr(0, first_space);
    if (!is_valid_method(method)) {
        return ParseError::malformed;
    }
    if (first_space == std::string_view::npos) {
        return ParseError::method_too_long;
    }
    const std::string_view rest = partial.substr(first_space + 1);
    const std::size_t second_space = rest.find(' ');
    const std::string_view target = rest.substr(0, second_space);
    if (!is_valid_target_octets(target, second_space == std::string_view::npos)) {
        return ParseError::malformed;
    }
    if (second_space != std::string_view::npos &&
        (target.empty() || !is_version_prefix(rest.substr(second_space + 1)))) {
        return ParseError::malformed;
    }
    return method.size() > target.size() ? ParseError::method_too_long
                                         : ParseError::target_too_long;
}

} // namespace

// Parses the CRLF-terminated request line `method SP request-target SP
// HTTP-version` into owned Request fields, skipping leading empty lines
// first (RFC 9112 2.2). The line end is located by finding the first '\n'
// and verifying the byte before it is '\r': finding the first '\n'
// preceded by '\r' is equivalent to finding the first CRLF (an earlier pair
// would contain an earlier '\n'), and a single-char search has no restart
// loop, so CR-dense garbage cannot degrade it. Every search is bounded by
// header_block_cap: without a line end inside the budget the line is
// classified by classify_unterminated (never a rescan of unbounded input),
// and while the budget lasts it is incomplete. A bare CR inside the line is
// rejected (RFC 9112 2.2), and each field must pass its grammar (see the
// header). Bytes after the request line are not consumed yet — header and
// body parsing are later tasks of the T-1 ladder.
std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw) {
    const std::size_t limit = std::min(raw.size(), header_block_cap);

    std::size_t skipped = 0;
    while (skipped + 1 < limit && raw[skipped] == '\r' && raw[skipped + 1] == '\n') {
        skipped += 2;
    }

    const std::string_view window = raw.substr(skipped, limit - skipped);
    const std::size_t line_feed = window.find('\n');
    if (line_feed == std::string_view::npos) {
        return raw.size() >= header_block_cap ? classify_unterminated(window)
                                              : ParseError::incomplete;
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

    const bool target_valid = !target.empty() && is_valid_target_octets(target, false);
    const bool version_valid = version.size() == 8 && is_version_prefix(version);
    if (!is_valid_method(method) || !target_valid || !version_valid) {
        return ParseError::malformed;
    }

    return ParsedRequest{
        Request{std::string(method), std::string(target), std::string(version)},
        skipped + line_feed + 1,
    };
}

} // namespace orange::http
