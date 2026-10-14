// Governed by docs/adr/ADR-0003-local-runtime-path.md.
#include "http_request_decoder.hpp"
#include "http_body_framing.hpp"
#include "http_chunk_line.hpp"
#include "http_syntax.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>
namespace orange::http {
// Saved connection-local cursors and framing state. Head storage is capped;
// chunk metadata and trailer validation retain only finite grammar state.
struct RequestDecoder::Impl {
    enum class Phase {
        head,
        fixed,
        chunk_line,
        chunk_data,
        chunk_cr,
        chunk_lf,
        trailers,
        complete,
        failed
    };
    enum class TrailerPhase { start, name, value, lf, final_lf };
    Phase phase = Phase::head;
    TrailerPhase trailer_phase = TrailerPhase::start;
    std::string head;
    std::size_t line_start = 0;
    bool request_line_seen = false;
    ParsedMessage message;
    std::size_t remaining = 0;
    detail::ChunkLine chunk_line;
    ParseError error = ParseError::malformed;

    // Makes failures terminal so input cannot accidentally become a new request.
    void fail(ParseError failure) {
        error = failure;
        phase = Phase::failed;
    }
    // Resolves framing once after the owned head has been constructed.
    void start_body(ParsedRequest parsed) {
        message.request = std::move(parsed.request);
        const auto framing = detail::body_framing(message.request);
        if (const auto *failure = std::get_if<ParseError>(&framing)) {
            fail(*failure);
            return;
        }
        const auto policy = std::get<detail::BodyFraming>(framing);
        remaining = policy.length;
        if (policy.chunked) {
            phase = Phase::chunk_line;
            chunk_line.reset(request_body_cap);
        } else {
            phase = remaining == 0 ? Phase::complete : Phase::fixed;
        }
        head.clear();
    }
    // Validates each completed fragmented head line once. The final head
    // parse constructs owned fields once and preserves the existing head API.
    void head_line() {
        const auto length = head.size() - line_start;
        if (length < 2 || head[head.size() - 2] != '\r') {
            fail(ParseError::malformed);
            return;
        }
        const auto line = std::string_view(head).substr(line_start, length - 2);
        line_start = head.size();
        if (line.empty()) {
            if (request_line_seen) {
                auto parsed = parse_request(head);
                if (auto *failure = std::get_if<ParseError>(&parsed)) {
                    fail(*failure);
                } else {
                    start_body(std::move(std::get<ParsedRequest>(parsed)));
                }
            }
        } else if (!request_line_seen) {
            if (!detail::is_valid_request_line(line)) {
                fail(ParseError::malformed);
            }
            request_line_seen = true;
        } else if (!detail::is_valid_header_line(line)) {
            fail(ParseError::malformed);
        }
    }
    // Searches only newly received head bytes. A complete first fragment uses
    // the established parser directly, without copying a second head buffer.
    std::size_t read_head(std::string_view input) {
        if (head.empty()) {
            auto parsed = parse_request(input);
            if (auto *ready = std::get_if<ParsedRequest>(&parsed)) {
                const auto used = ready->consumed;
                start_body(std::move(*ready));
                return used;
            }
            const auto failure = std::get<ParseError>(parsed);
            if (failure != ParseError::incomplete) {
                fail(failure);
                return std::min(input.size(), header_block_cap);
            }
        }
        std::size_t used = 0;
        while (used < input.size() && phase == Phase::head) {
            const auto room = header_block_cap - head.size();
            const auto window = input.substr(used, std::min(input.size() - used, room));
            const auto lf = window.find('\n');
            const auto count = lf == std::string_view::npos ? window.size() : lf + 1;
            head.append(window.substr(0, count));
            used += count;
            if (lf != std::string_view::npos) {
                head_line();
            }
            if (phase == Phase::head && head.size() == header_block_cap) {
                // Reuse the head parser's established 400/414/431/501 precedence.
                fail(std::get<ParseError>(parse_request(head)));
            }
        }
        return used;
    }
    // Copies exact content bytes in bulk, without interpreting CR, LF or NUL.
    std::size_t read_content(std::string_view input) {
        const auto count = std::min(remaining, input.size());
        message.body.append(input.substr(0, count));
        remaining -= count;
        if (remaining == 0) {
            phase = phase == Phase::fixed ? Phase::complete : Phase::chunk_cr;
        }
        return count;
    }
    // Advances the constant-storage chunk-line validator one metadata byte.
    void read_chunk_line(char c) {
        const auto result = chunk_line.feed(c);
        if (result == detail::ChunkLineResult::malformed) {
            fail(ParseError::malformed);
        } else if (result == detail::ChunkLineResult::too_large) {
            fail(ParseError::body_too_long);
        } else if (result == detail::ChunkLineResult::complete) {
            remaining = chunk_line.size();
            phase = remaining == 0 ? Phase::trailers : Phase::chunk_data;
        }
    }
    // Discards trailers while checking the same token/value octets as heads.
    // Framing-sensitive fields cannot replace the already resolved head fields.
    void read_trailer(char c) {
        switch (trailer_phase) {
        case TrailerPhase::start:
            if (c == '\r') {
                trailer_phase = TrailerPhase::final_lf;
            } else if (detail::is_tchar(c)) {
                trailer_phase = TrailerPhase::name;
            } else {
                fail(ParseError::malformed);
            }
            break;
        case TrailerPhase::name:
            if (c == ':') {
                trailer_phase = TrailerPhase::value;
            } else if (!detail::is_tchar(c)) {
                fail(ParseError::malformed);
            }
            break;
        case TrailerPhase::value:
            if (c == '\r') {
                trailer_phase = TrailerPhase::lf;
            } else if (!detail::is_field_octet(c)) {
                fail(ParseError::malformed);
            }
            break;
        case TrailerPhase::lf:
        case TrailerPhase::final_lf:
            if (c != '\n') {
                fail(ParseError::malformed);
            } else if (trailer_phase == TrailerPhase::final_lf) {
                phase = Phase::complete;
            } else {
                trailer_phase = TrailerPhase::start;
            }
            break;
        }
    }
    // Consumes framing octets; chunk data itself takes the bulk content path.
    void read_metadata(char c) {
        switch (phase) {
        case Phase::chunk_line:
            read_chunk_line(c);
            break;
        case Phase::chunk_cr:
            if (c != '\r') {
                fail(ParseError::malformed);
            } else {
                phase = Phase::chunk_lf;
            }
            break;
        case Phase::chunk_lf:
            if (c != '\n') {
                fail(ParseError::malformed);
            } else {
                phase = Phase::chunk_line;
                chunk_line.reset(request_body_cap - message.body.size());
            }
            break;
        case Phase::trailers:
            read_trailer(c);
            break;
        default:
            fail(ParseError::malformed);
            break;
        }
    }
    // Counts only this message's wire bytes, including discarded metadata.
    DecodeResult feed(std::string_view input) {
        std::size_t used = 0;
        while (used < input.size() && phase != Phase::complete && phase != Phase::failed) {
            const auto room = std::numeric_limits<std::size_t>::max() - message.consumed;
            if (room == 0) {
                fail(ParseError::malformed);
                break;
            }
            const auto window = input.substr(used, std::min(input.size() - used, room));
            std::size_t count = 1;
            if (phase == Phase::head) {
                count = read_head(window);
            } else if (phase == Phase::fixed || phase == Phase::chunk_data) {
                count = read_content(window);
            } else {
                read_metadata(window.front());
            }
            used += count;
            message.consumed += count;
        }
        if (phase == Phase::failed) {
            return {used, error};
        }
        if (phase == Phase::complete && used != 0) {
            return {used, std::move(message)};
        }
        return {used, std::monostate{}};
    }
};
RequestDecoder::RequestDecoder() noexcept = default;
RequestDecoder::~RequestDecoder() = default;
DecodeResult RequestDecoder::feed(std::string_view fragment) {
    if (allocation_failed_) {
        return {0, ParseError::resource_exhausted};
    }
    if (fragment.empty() && !impl_) {
        return {};
    }
    try {
        if (!impl_) {
            impl_ = std::make_unique<Impl>();
        }
        return impl_->feed(fragment);
    } catch (const std::bad_alloc &) {
        allocation_failed_ = true;
        impl_.reset();
        // No usable input boundary survives a resource failure. The caller
        // must discard this connection's input rather than retry the fragment.
        return {0, ParseError::resource_exhausted};
    }
}
void RequestDecoder::reset() {
    if (impl_) {
        *impl_ = Impl{};
    }
    allocation_failed_ = false;
}
std::variant<ParsedMessage, ParseError> parse_message(std::string_view raw) {
    RequestDecoder decoder;
    auto result = decoder.feed(raw);
    if (auto *parsed = std::get_if<ParsedMessage>(&result.outcome)) {
        return std::move(*parsed);
    }
    if (const auto *error = std::get_if<ParseError>(&result.outcome)) {
        return *error;
    }
    return ParseError::incomplete;
}
} // namespace orange::http
