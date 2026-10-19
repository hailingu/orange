// ADR: service/docs/adr/ADR-0003-fragmented-head-offsets.md
// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Performance authorization: docs/reviews/2026-10-16-http-performance.md.
// Internal shared octet grammar for request heads, framing and trailers.
#ifndef ORANGE_HTTP_SYNTAX_HPP
#define ORANGE_HTTP_SYNTAX_HPP
#include <algorithm>
#include <cstdint>
#include <string_view>
namespace orange::http::detail {
// Byte-classification table for the request-head grammar: one 256-entry
// bitmask built at compile time, so every classifier below is a single
// table load encoding the same byte sets the grammar names (RFC 9110
// 5.6.2, RFC 3986 2.1-2.3, RFC 9112 3.2, RFC 9110 5.5).
struct ByteClassTable {
    enum Mask : std::uint8_t {
        digit = 1 << 0,
        hexdig = 1 << 1,
        tchar = 1 << 2,
        target_char = 1 << 3,
        field_octet = 1 << 4,
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
            // VCHAR, SP, and HTAB (field-content plus its optional
            // whitespace) and obs-text in a value (RFC 9110 5.5): every
            // octet a field line may carry outside its CRLF.
            if ((i >= 0x21 && i <= 0x7E) || i == 0x20 || i == 0x09 || i >= 0x80) {
                mask |= field_octet;
            }
            masks[i] = mask;
        }
    }
};

inline constexpr ByteClassTable byte_classes{};

// RFC decimal digit, with signed-char octets safely indexed as unsigned.
constexpr bool is_digit(char c) {
    return (byte_classes.masks[static_cast<unsigned char>(c)] & ByteClassTable::digit) != 0;
}

// RFC hexadecimal digit used by URI escapes and chunk sizes.
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

// An octet a field line may carry outside its CRLF: field-content octets
// (VCHAR or obs-text, RFC 9110 5.5) plus the SP/HTAB of optional
// whitespace. A CTL — including CR and LF, which appear only as the line
// end — DEL, or any other byte cannot be part of a field name, colon, or
// value.
constexpr bool is_field_octet(char c) {
    return (byte_classes.masks[static_cast<unsigned char>(c)] & ByteClassTable::field_octet) != 0;
}

// RFC 9110 5.6.2 token: one or more tchar characters — the grammar of a
// method and of a field-name alike.
constexpr bool is_token(std::string_view token) {
    return !token.empty() && std::all_of(token.begin(), token.end(), is_tchar);
}

} // namespace orange::http::detail
#endif
