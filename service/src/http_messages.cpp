// ADR: service/docs/adr/ADR-0003-fragmented-head-offsets.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Performance authorization: docs/reviews/2026-10-16-http-performance.md.
// Storage authorization: docs/reviews/2026-10-16-owned-request-storage.md.

#include "http_messages.hpp"
#include "http_head_fields.hpp"
#include "http_syntax.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace orange::http {
namespace {

using detail::HeaderFieldOffsets;
using detail::is_digit;
using detail::is_field_octet;
using detail::is_hexdig;
using detail::is_target_char;
using detail::is_token;

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
    if (!is_token(method)) {
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

// RFC 9110 5.6.3 permits exactly SP and HTAB in optional whitespace.
constexpr bool is_ows(char c) { return c == ' ' || c == '\t'; }

constexpr std::size_t ows_group_size = 16;

// Skips only full, all-OWS groups at either edge; scalar checks locate
// the first non-OWS byte and handle the remainder without out-of-range reads.
std::string_view trim_ows_groups(std::string_view value) {
    while (value.size() >= ows_group_size) {
        unsigned int non_ows = 0;
        for (std::size_t i = 0; i < ows_group_size; ++i) {
            non_ows |= !is_ows(value[i]);
        }
        if (non_ows != 0) {
            break;
        }
        value.remove_prefix(ows_group_size);
    }
    while (!value.empty() && is_ows(value.front())) {
        value.remove_prefix(1);
    }
    while (value.size() >= ows_group_size) {
        unsigned int non_ows = 0;
        const std::size_t begin = value.size() - ows_group_size;
        for (std::size_t i = 0; i < ows_group_size; ++i) {
            non_ows |= !is_ows(value[begin + i]);
        }
        if (non_ows != 0) {
            break;
        }
        value.remove_suffix(ows_group_size);
    }
    while (!value.empty() && is_ows(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

// Removes surrounding OWS without touching interior whitespace. Short
// values and the common single-space edge keep the scalar, inlinable path.
std::string_view trim_ows(std::string_view value) {
    if (value.size() >= 2 * ows_group_size &&
        ((is_ows(value.front()) && is_ows(value[1])) ||
         (is_ows(value.back()) && is_ows(value[value.size() - 2])))) {
        return trim_ows_groups(value);
    }
    while (!value.empty() && is_ows(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_ows(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

constexpr std::size_t field_octet_group_size = 16;

// Checks full groups with independent range comparisons so the compiler
// can vectorize them, rejecting between groups. Only complete groups are
// read; the remainder uses the shared byte-class table.
bool is_valid_field_value_groups(std::string_view value) {
    std::size_t offset = 0;
    while (value.size() - offset >= field_octet_group_size) {
        unsigned int invalid = 0;
        for (std::size_t i = 0; i < field_octet_group_size; ++i) {
            const auto byte = static_cast<unsigned char>(value[offset + i]);
            invalid |= (byte < 0x20 && byte != 0x09) || byte == 0x7F;
        }
        if (invalid != 0) {
            return false;
        }
        offset += field_octet_group_size;
    }
    return std::all_of(value.begin() + offset, value.end(), is_field_octet);
}

// Field-content after OWS trimming: a CTL or DEL anywhere is malformed.
// Keep short-value checks inlinable independently of the grouped loop.
bool is_valid_field_value(std::string_view value) {
    return value.size() < field_octet_group_size
               ? std::all_of(value.begin(), value.end(), is_field_octet)
               : is_valid_field_value_groups(value);
}

// Classifies a header block whose terminating empty line did not arrive
// inside the remaining head budget. A tail octet that cannot appear
// anywhere in a field line — after allowing one trailing '\r' that may be
// the first half of a CRLF — already breaks the grammar: malformed.
// Otherwise the head can never finish under the cap and the outcome is the
// header-fields 431 (RFC 6585 5).
ParseError classify_unterminated_header(std::string_view tail) {
    if (tail.ends_with('\r')) {
        tail.remove_suffix(1);
    }
    return is_valid_field_value(tail) ? ParseError::headers_too_long : ParseError::malformed;
}

// A validated header block's end and field count, used to allocate the
// owned result only after the entire head has passed its grammar.
struct HeaderBlock {
    std::size_t consumed;
    std::size_t field_count;
};

// Borrowed validation results remain inside the parser; callers only receive
// owned requests. Both complete and incremental parsing use these grammars.
struct FieldViews {
    std::string_view name;
    std::string_view value;
};
std::optional<FieldViews> validate_field_line(std::string_view line) {
    const auto colon = line.find(':');
    const auto name = line.substr(0, colon);
    if (colon == std::string_view::npos || !is_token(name)) {
        return std::nullopt;
    }
    const auto value = trim_ows(line.substr(colon + 1));
    return is_valid_field_value(value) ? std::optional<FieldViews>(FieldViews{name, value})
                                       : std::nullopt;
}

// Three validated line tokens, borrowed only until the owned head is copied.
struct RequestLineViews {
    std::string_view method;
    std::string_view target;
    std::string_view version;
};
std::optional<RequestLineViews> validate_request_line(std::string_view line) {
    if (line.find('\r') != std::string_view::npos) {
        return std::nullopt;
    }
    const auto first = line.find(' ');
    if (first == std::string_view::npos) {
        return std::nullopt;
    }
    const auto second = line.find(' ', first + 1);
    if (second == std::string_view::npos) {
        return std::nullopt;
    }
    const auto method = line.substr(0, first);
    const auto target = line.substr(first + 1, second - first - 1);
    const auto version = line.substr(second + 1);
    if (version.size() != 8 || !is_version_prefix(version) || !is_token(method) || target.empty() ||
        !is_valid_target_octets(target, false)) {
        return std::nullopt;
    }
    return RequestLineViews{method, target, version};
}

static_assert(header_block_cap <= std::numeric_limits<std::uint16_t>::max());
using HeaderFields = std::array<HeaderFieldOffsets, header_block_cap / 4>;

// Validates the header block that follows an already-validated request line:
// zero or more CRLF-terminated field lines, ended by one empty line, all
// inside the head budget left after the request line. Field names are
// tokens (RFC 9112 5.1 rejects whitespace before the colon), values are
// OWS-trimmed field-content, and obsolete line folding — a line opening
// with SP or HTAB — is rejected (RFC 9112 5.2). The block spends only that
// remaining budget: exhausted before the terminating empty line arrives,
// classify_unterminated_header answers 431 or malformed, never a rescan of
// unbounded input. Each line end is located by the first '\n' plus
// one-byte look-back (see parse_request for the equivalence argument).
std::variant<HeaderBlock, ParseError>
validate_header_block(std::string_view raw, std::size_t head_end, HeaderFields &fields) {
    std::size_t consumed = head_end;
    std::size_t field_count = 0;
    while (true) {
        const std::string_view window =
            raw.substr(consumed, std::min(raw.size() - consumed, header_block_cap - consumed));
        const std::size_t line_feed = window.find('\n');
        if (line_feed == std::string_view::npos) {
            return raw.size() >= header_block_cap ? classify_unterminated_header(window)
                                                  : ParseError::incomplete;
        }
        if (line_feed == 0 || window[line_feed - 1] != '\r') {
            return ParseError::malformed;
        }
        if (line_feed == 1) {
            return HeaderBlock{consumed + line_feed + 1, field_count};
        }
        const std::string_view line = window.substr(0, line_feed - 1);
        const auto field = validate_field_line(line);
        if (!field) {
            return ParseError::malformed;
        }
        fields[field_count++] = HeaderFieldOffsets{
            static_cast<std::uint16_t>(consumed), static_cast<std::uint16_t>(field->name.size()),
            static_cast<std::uint16_t>(field->value.data() - raw.data()),
            static_cast<std::uint16_t>(field->value.size())};
        consumed += line_feed + 1;
    }
}

} // namespace

Request::Request(std::string_view head, detail::RequestLineOffsets line,
                 std::span<const detail::HeaderFieldOffsets> fields)
    : fields_(fields.begin(), fields.end()), line_(line) {
    std::size_t text_bytes = line.method_size + line.target_size + 8;
    for (const auto field : fields)
        text_bytes += field.name_size + field.value_size;
    // Full copies are fastest for dense fields. Compact only when text fits
    // existing inline capacity, or at least three quarters of input is unused.
    if (!fields.empty() && (text_bytes <= head_.capacity() || text_bytes <= head.size() / 4)) {
        head_.reserve(text_bytes);
        append_slice(head, line_.method_begin, line_.method_size);
        append_slice(head, line_.target_begin, line_.target_size);
        append_slice(head, line_.version_begin, 8);
        for (auto &field : fields_) {
            append_slice(head, field.name_begin, field.name_size);
            append_slice(head, field.value_begin, field.value_size);
        }
    } else {
        head_.assign(head);
    }
}
void Request::append_slice(std::string_view source, std::uint16_t &begin, std::uint16_t size) {
    const auto slice = source.substr(begin, size);
    begin = static_cast<std::uint16_t>(head_.size());
    head_.append(slice);
}

Request::Request(Request &&other) noexcept
    : head_(std::move(other.head_)), fields_(std::move(other.fields_)),
      line_(std::exchange(other.line_, {})) {
    other.head_.clear();
    other.fields_.clear();
}
Request &Request::operator=(Request &&other) noexcept {
    if (this != &other) {
        head_ = std::move(other.head_);
        fields_ = std::move(other.fields_);
        line_ = std::exchange(other.line_, {});
        other.head_.clear();
        other.fields_.clear();
    }
    return *this;
}
Request &Request::operator=(const Request &other) {
    if (this != &other) {
        Request copy(other);
        swap(copy);
    }
    return *this;
}
void Request::swap(Request &other) noexcept {
    head_.swap(other.head_);
    fields_.swap(other.fields_);
    std::swap(line_, other.line_);
}

namespace detail {
HeadLineResult HeadFields::append_line(std::string_view line, std::size_t begin) {
    if (begin > header_block_cap - 2 || line.size() > header_block_cap - begin - 2) {
        return HeadLineResult::malformed;
    }
    if (line.empty()) {
        return request_line_seen_ ? HeadLineResult::complete : HeadLineResult::pending;
    }
    if (!request_line_seen_) {
        const auto tokens = validate_request_line(line);
        if (!tokens) {
            return HeadLineResult::malformed;
        }
        request_line_ = {static_cast<std::uint16_t>(begin),
                         static_cast<std::uint16_t>(tokens->method.size()),
                         static_cast<std::uint16_t>(begin + tokens->method.size() + 1),
                         static_cast<std::uint16_t>(tokens->target.size()),
                         static_cast<std::uint16_t>(begin + line.size() - 8)};
        request_line_seen_ = true;
        return HeadLineResult::pending;
    }
    const auto field = validate_field_line(line);
    if (!field) {
        return HeadLineResult::malformed;
    }
    if (fields_.empty()) {
        fields_.reserve(16);
    }
    fields_.push_back({static_cast<std::uint16_t>(begin),
                       static_cast<std::uint16_t>(field->name.size()),
                       static_cast<std::uint16_t>(
                           begin + static_cast<std::size_t>(field->value.data() - line.data())),
                       static_cast<std::uint16_t>(field->value.size())});
    return HeadLineResult::pending;
}
ParsedRequest HeadFields::finish(std::string_view head, std::size_t consumed) const {
    return {Request(head.substr(0, consumed), request_line_, fields_), consumed};
}
} // namespace detail

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
// header). validate_header_block records field offsets; RequestDecoder owns
// body decoding. Owned storage is materialized only after the whole head
// has been validated.
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
    const auto tokens = validate_request_line(line);
    if (!tokens) {
        return ParseError::malformed;
    }

    const std::size_t head_end = skipped + line_feed + 1;
    HeaderFields fields;
    const auto block = validate_header_block(raw, head_end, fields);
    if (const auto *error = std::get_if<ParseError>(&block)) {
        return *error;
    }
    const auto ready = std::get<HeaderBlock>(block);
    const detail::RequestLineOffsets offsets{
        static_cast<std::uint16_t>(skipped), static_cast<std::uint16_t>(tokens->method.size()),
        static_cast<std::uint16_t>(skipped + tokens->method.size() + 1),
        static_cast<std::uint16_t>(tokens->target.size()),
        static_cast<std::uint16_t>(head_end - 10)};
    try {
        return ParsedRequest{Request(raw.substr(0, ready.consumed), offsets,
                                     std::span(fields.data(), ready.field_count)),
                             ready.consumed};
    } catch (const std::bad_alloc &) {
        return ParseError::resource_exhausted;
    }
}

} // namespace orange::http
