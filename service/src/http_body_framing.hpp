// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Internal interpretation of request head policy fields: body framing
// (RFC 9112 6) and the connection-close option (RFC 9110 7.6.1), resolved
// together in one pass over the validated fields.
#ifndef ORANGE_HTTP_BODY_FRAMING_HPP
#define ORANGE_HTTP_BODY_FRAMING_HPP
#include "http_messages.hpp"
namespace orange::http::detail {
// A validated decoded length, or chunked framing whose length is not yet
// known, plus the request's connection-close option from the same pass.
struct HeadPolicy {
    bool chunked = false;
    std::size_t length = 0;
    bool close = false;
};
// Resolves all head policy fields together before any content is accepted.
[[nodiscard]] std::variant<HeadPolicy, ParseError> head_policy(const Request &request);
} // namespace orange::http::detail
#endif
