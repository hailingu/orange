// Governed by docs/adr/ADR-0003-local-runtime-path.md.
#include "http_request_decoder.hpp"
#include <charconv>
#include <utility>
namespace orange::http {
struct RequestDecoder::Impl {};
RequestDecoder::RequestDecoder() : impl_(std::make_unique<Impl>()) {}
RequestDecoder::~RequestDecoder() = default;
DecodeResult RequestDecoder::feed(std::string_view) { return {}; }
void RequestDecoder::reset() { *impl_ = Impl{}; }
std::variant<ParsedMessage, ParseError> parse_message(std::string_view raw) {
    auto head = parse_request(raw);
    if (auto *parsed = std::get_if<ParsedRequest>(&head)) {
        std::size_t length = 0;
        for (const auto &field : parsed->request.headers) {
            if (field.name == "Content-Length") {
                const auto value = std::string_view(field.value);
                const auto number =
                    std::from_chars(value.data(), value.data() + value.size(), length);
                if (number.ec != std::errc{} || number.ptr != value.data() + value.size()) {
                    return ParseError::malformed;
                }
            }
        }
        if (raw.size() - parsed->consumed < length) {
            return ParseError::incomplete;
        }
        return ParsedMessage{std::move(parsed->request),
                             std::string(raw.substr(parsed->consumed, length)),
                             parsed->consumed + length};
    }
    return std::get<ParseError>(head);
}
} // namespace orange::http
