// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Incremental request-message decoding for one connection. The head parser
// remains available separately; this layer owns body framing and decoded bytes.
#ifndef ORANGE_HTTP_REQUEST_DECODER_HPP
#define ORANGE_HTTP_REQUEST_DECODER_HPP
#include "http_messages.hpp"
#include <string_view>
#include <variant>
namespace orange::http {
// ADR-0003 bounds decoded request content, not chunk framing, to 1 MiB.
inline constexpr std::size_t request_body_cap = 1024 * 1024;
// One complete message with owned head fields and binary-safe decoded content.
struct ParsedMessage {
    Request request;
    std::string body;
    // Counts wire bytes, including chunk framing and discarded trailer fields.
    std::size_t consumed = 0;
};
// Parses one complete message. Missing framing bytes return incomplete;
// trailing pipelined input is excluded from the consumed count.
[[nodiscard]] std::variant<ParsedMessage, ParseError> parse_message(std::string_view raw);
} // namespace orange::http
#endif
