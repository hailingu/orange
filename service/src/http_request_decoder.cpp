// Governed by docs/adr/ADR-0003-local-runtime-path.md.
#include "http_request_decoder.hpp"
namespace orange::http {
// Initial wiring delegates to the established head parser; body behavior
// is deliberately absent until the failing message tests drive it.
std::variant<ParsedMessage, ParseError> parse_message(std::string_view raw) {
    auto head = parse_request(raw);
    if (auto *parsed = std::get_if<ParsedRequest>(&head)) {
        return ParsedMessage{std::move(parsed->request), {}, parsed->consumed};
    }
    return std::get<ParseError>(head);
}
} // namespace orange::http
