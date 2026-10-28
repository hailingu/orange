// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
#include "http_body_framing.hpp"
#include "http_syntax.hpp"
#include <algorithm>
#include <optional>
namespace orange::http::detail {
namespace {
// ASCII folding is independent of locales and preserves non-ASCII octets.
bool equal_ascii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        if (lower(a[i]) != lower(b[i])) {
            return false;
        }
    }
    return true;
}
// Advances a cursor past RFC BWS/OWS, without allocating normalized fields.
void skip_space(std::string_view &value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
}
// True when one Connection field value carries the close option (RFC 9110
// 7.6.1): a comma-separated, case-insensitive option list with optional
// whitespace. The head parser already validated every field octet.
bool has_close_option(std::string_view value) {
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t comma = value.find(',', begin);
        auto option = value.substr(begin, comma == std::string_view::npos ? std::string_view::npos
                                                                          : comma - begin);
        while (!option.empty() && (option.front() == ' ' || option.front() == '\t')) {
            option.remove_prefix(1);
        }
        while (!option.empty() && (option.back() == ' ' || option.back() == '\t')) {
            option.remove_suffix(1);
        }
        if (iequals(option, "close")) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1;
    }
    return false;
}
// Consumes one token, leaving delimiters for the field-specific grammar.
std::string_view take_token(std::string_view &value) {
    std::size_t size = 0;
    while (size < value.size() && is_tchar(value[size])) {
        ++size;
    }
    const auto token = value.substr(0, size);
    value.remove_prefix(size);
    return token;
}
// Validates a quoted-string including quoted-pair (RFC 9110 5.6.4).
bool take_quoted(std::string_view &value) {
    value.remove_prefix(1);
    while (!value.empty()) {
        const char c = value.front();
        value.remove_prefix(1);
        if (c == '"') {
            return true;
        }
        if (c == '\\') {
            if (value.empty() || !is_field_octet(value.front())) {
                return false;
            }
            value.remove_prefix(1);
        } else if (!is_field_octet(c)) {
            return false;
        }
    }
    return false;
}
// Interprets a CL comma list using normalized decimal views. Overflow cannot
// affect equality; the numeric cap is checked only after every value agrees.
bool collect_length(std::string_view value, std::optional<std::string_view> &canonical) {
    bool found = false;
    while (!value.empty()) {
        const auto comma = value.find(',');
        auto item = value.substr(0, comma);
        skip_space(item);
        while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) {
            item.remove_suffix(1);
        }
        if (!item.empty()) {
            if (!std::all_of(item.begin(), item.end(), is_digit)) {
                return false;
            }
            while (item.size() > 1 && item.front() == '0') {
                item.remove_prefix(1);
            }
            if (canonical && *canonical != item) {
                return false;
            }
            canonical = item;
            found = true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        value.remove_prefix(comma + 1);
    }
    return found;
}
// Tracks the coding sequence across duplicate TE fields in received order.
struct Codings {
    bool chunked = false;
    bool unsupported = false;
};
// Transfer parameters require name=value; a chunked coding has none.
bool take_parameter(std::string_view &value) {
    value.remove_prefix(1);
    skip_space(value);
    if (take_token(value).empty()) {
        return false;
    }
    skip_space(value);
    if (value.empty() || value.front() != '=') {
        return false;
    }
    value.remove_prefix(1);
    skip_space(value);
    return !value.empty() &&
           (value.front() == '"' ? take_quoted(value) : !take_token(value).empty());
}
// Parses a TE list without mistaking commas inside quoted parameters for separators.
bool collect_codings(std::string_view value, Codings &codings) {
    bool found = false;
    while (!value.empty()) {
        skip_space(value);
        if (value.empty()) {
            break;
        }
        if (value.front() == ',') {
            value.remove_prefix(1);
            continue;
        }
        const auto name = take_token(value);
        if (name.empty() || codings.chunked) {
            return false;
        }
        found = true;
        codings.chunked = equal_ascii(name, "chunked");
        codings.unsupported |= !codings.chunked;
        skip_space(value);
        while (!value.empty() && value.front() == ';') {
            if (codings.chunked || !take_parameter(value)) {
                return false;
            }
            skip_space(value);
        }
        if (!value.empty()) {
            if (value.front() != ',') {
                return false;
            }
            value.remove_prefix(1);
        }
    }
    return found;
}
} // namespace
std::variant<HeadPolicy, ParseError> head_policy(const Request &request) {
    std::optional<std::string_view> length;
    Codings codings;
    bool has_length = false;
    bool has_codings = false;
    bool close = false;
    for (const auto &field : request.headers()) {
        if (equal_ascii(field.name, "content-length")) {
            has_length = true;
            if (!collect_length(field.value, length)) {
                return ParseError::malformed;
            }
        } else if (equal_ascii(field.name, "transfer-encoding")) {
            has_codings = true;
            if (!collect_codings(field.value, codings)) {
                return ParseError::malformed;
            }
        } else if (equal_ascii(field.name, "connection")) {
            close = close || has_close_option(field.value);
        }
    }
    if (has_codings) {
        if (has_length || request.version() == "HTTP/1.0" || !codings.chunked) {
            return ParseError::malformed;
        }
        return codings.unsupported
                   ? std::variant<HeadPolicy, ParseError>(ParseError::unsupported_transfer_coding)
                   : std::variant<HeadPolicy, ParseError>(HeadPolicy{true, 0, close});
    }
    std::size_t bytes = 0;
    if (length) {
        for (char c : *length) {
            const auto digit = static_cast<std::size_t>(c - '0');
            if (bytes > (request_body_cap - digit) / 10) {
                return ParseError::body_too_long;
            }
            bytes = bytes * 10 + digit;
        }
    }
    return HeadPolicy{false, bytes, close};
}
} // namespace orange::http::detail
