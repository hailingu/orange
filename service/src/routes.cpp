// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Implementation of the T-2 route dispatch: the declared health route and
// the 405/404 outcomes for methods and targets outside it. Echo, static
// serving, and the TLS listener land in later T-2 rungs.
#include "routes.hpp"

namespace orange::http {
namespace {

// The one declared route of this rung (ADR-0003 Scope): exact origin-form
// target, answered with the exact declared status object.
constexpr std::string_view health_target = "/api/health";
constexpr std::string_view health_body = "{\"status\":\"ok\"}";
constexpr ResponseField health_fields[] = {{"Content-Type", "application/json"}};
// What a 405 on the health route advertises as supported (RFC 9110 15.5.6).
constexpr ResponseField health_allow[] = {{"Allow", "GET"}};

// An empty-body outcome framed with Content-Length: 0 so a keep-alive
// client can delimit the response without a connection close (RFC 9112 6).
Response empty_response(unsigned int status, std::span<const ResponseField> fields) {
    return Response{status, BodyFraming::content_length, "", fields, false};
}

} // namespace

Response handle_request(const ParsedMessage &message) {
    if (message.request.target() != health_target)
        return empty_response(404, {});
    if (message.request.method() != "GET")
        return empty_response(405, health_allow);
    return Response{200, BodyFraming::content_length, health_body, health_fields, false};
}

} // namespace orange::http
