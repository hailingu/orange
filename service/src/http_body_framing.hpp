// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Internal interpretation of request-body framing fields (RFC 9112 6).
#ifndef ORANGE_HTTP_BODY_FRAMING_HPP
#define ORANGE_HTTP_BODY_FRAMING_HPP
#include "http_messages.hpp"
namespace orange::http::detail {
// A validated decoded length, or chunked framing whose length is not yet known.
struct BodyFraming {
    bool chunked = false;
    std::size_t length = 0;
};
// Resolves all framing fields together before any content is accepted.
[[nodiscard]] std::variant<BodyFraming, ParseError> body_framing(const Request &request);
} // namespace orange::http::detail
#endif
