// ADR: service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md
// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Performance authorization: docs/reviews/2026-10-16-http-performance.md.
#include "http_chunk_line.hpp"
#include "http_syntax.hpp"
namespace orange::http::detail {
namespace {
// BWS and OWS have the same two-octet whitespace set.
bool space(char c) { return c == ' ' || c == '\t'; }
} // namespace
void ChunkLine::reset(std::size_t quota) {
    *this = ChunkLine{};
    quota_ = quota;
}
void ChunkLine::add_digit(char c) {
    digit_seen_ = true;
    const auto digit = static_cast<std::size_t>(is_digit(c) ? c - '0' : (c | 0x20) - 'a' + 10);
    if (digit > quota_ || size_ > (quota_ - digit) / 16) {
        oversized_ = true;
    } else if (!oversized_) {
        size_ = size_ * 16 + digit;
    }
}
ChunkLineResult ChunkLine::size_byte(char c) {
    if (is_hexdig(c)) {
        add_digit(c);
        return ChunkLineResult::pending;
    }
    if (!digit_seen_ || (!space(c) && c != ';' && c != '\r')) {
        return ChunkLineResult::malformed;
    }
    if (oversized_) {
        return ChunkLineResult::too_large;
    }
    return separator(c);
}
ChunkLineResult ChunkLine::separator(char c) {
    if (space(c)) {
        phase_ = phase_ == Phase::size                                    ? Phase::before_separator
                 : (phase_ == Phase::name || phase_ == Phase::after_name) ? Phase::after_name
                                                                          : Phase::after_value;
    } else if (c == ';') {
        phase_ = Phase::before_extension;
        // The next non-BWS octet must be an extension name, not another separator.
    } else if (c == '\r') {
        if (phase_ == Phase::after_name || phase_ == Phase::after_value) {
            return ChunkLineResult::malformed;
        }
        phase_ = Phase::lf;
    } else {
        return ChunkLineResult::malformed;
    }
    return ChunkLineResult::pending;
}
ChunkLineResult ChunkLine::value_byte(char c) {
    if (phase_ == Phase::before_value) {
        if (space(c)) {
            return ChunkLineResult::pending;
        }
        if (c == '"') {
            phase_ = Phase::quoted_value;
        } else if (is_tchar(c)) {
            phase_ = Phase::token_value;
        } else {
            return ChunkLineResult::malformed;
        }
    } else if (phase_ == Phase::quoted_value) {
        if (c == '"') {
            phase_ = Phase::after_quote;
        } else if (c == '\\') {
            phase_ = Phase::escape;
        } else if (!is_field_octet(c)) {
            return ChunkLineResult::malformed;
        }
    } else if (phase_ == Phase::escape) {
        if (!is_field_octet(c)) {
            return ChunkLineResult::malformed;
        }
        phase_ = Phase::quoted_value;
    } else if (phase_ == Phase::token_value && is_tchar(c)) {
        return ChunkLineResult::pending;
    } else {
        return separator(c);
    }
    return ChunkLineResult::pending;
}
ChunkLineResult ChunkLine::feed(char c) {
    if (phase_ == Phase::size) {
        return size_byte(c);
    }
    if (phase_ == Phase::lf) {
        return c == '\n' ? ChunkLineResult::complete : ChunkLineResult::malformed;
    }
    if (phase_ == Phase::before_separator || phase_ == Phase::before_extension) {
        if (space(c)) {
            return ChunkLineResult::pending;
        }
        if (phase_ == Phase::before_separator && c == ';') {
            phase_ = Phase::before_extension;
        } else if (phase_ == Phase::before_extension && is_tchar(c)) {
            phase_ = Phase::name;
        } else {
            return ChunkLineResult::malformed;
        }
        return ChunkLineResult::pending;
    }
    if (phase_ == Phase::name || phase_ == Phase::after_name) {
        if (phase_ == Phase::name && is_tchar(c)) {
            return ChunkLineResult::pending;
        }
        if (c == '=') {
            phase_ = Phase::before_value;
            return ChunkLineResult::pending;
        }
        return separator(c);
    }
    return value_byte(c);
}
ChunkLineStep ChunkLine::feed(std::string_view input) {
    std::size_t consumed = 0;
    while (consumed < input.size()) {
        // Runs stay in one grammar state; only delimiters use byte transitions.
        if (phase_ == Phase::size) {
            while (consumed < input.size() && is_hexdig(input[consumed])) {
                add_digit(input[consumed++]);
            }
        } else if (phase_ == Phase::name || phase_ == Phase::token_value) {
            while (consumed < input.size() && is_tchar(input[consumed])) {
                ++consumed;
            }
        } else if (phase_ == Phase::quoted_value) {
            while (consumed < input.size() && input[consumed] != '"' && input[consumed] != '\\' &&
                   is_field_octet(input[consumed])) {
                ++consumed;
            }
        }
        if (consumed == input.size()) {
            break;
        }
        const auto result = feed(input[consumed++]);
        if (result != ChunkLineResult::pending) {
            return {consumed, result};
        }
    }
    return {consumed, ChunkLineResult::pending};
}
} // namespace orange::http::detail
