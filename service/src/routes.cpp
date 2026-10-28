// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Implementation of the T-2 route dispatch: the declared diagnostic routes
// and the 405/404 outcomes for methods and targets outside them. Static
// serving and the TLS listener land in later T-2 rungs.
#include "routes.hpp"

namespace orange::http {
namespace {

// The declared diagnostic routes (ADR-0003 Scope): exact origin-form
// targets, each with the one method it allows (RFC 9110 15.5.6 Allow).
constexpr std::string_view health_target = "/api/health";
constexpr std::string_view health_body = "{\"status\":\"ok\"}";
constexpr ResponseField health_fields[] = {{"Content-Type", "application/json"}};
constexpr ResponseField health_allow[] = {{"Allow", "GET"}};
constexpr std::string_view echo_target = "/api/echo";
constexpr ResponseField echo_allow[] = {{"Allow", "POST"}};

// An empty-body outcome framed with Content-Length: 0 so a keep-alive
// client can delimit the response without a connection close (RFC 9112 6).
Response empty_response(unsigned int status, std::span<const ResponseField> fields) {
    return Response{status, BodyFraming::content_length, "", fields, false};
}

} // namespace

Response handle_request(const ParsedMessage &message) {
    const std::string_view target = message.request.target();
    const std::string_view method = message.request.method();
    if (target == health_target) {
        if (method != "GET")
            return empty_response(405, health_allow);
        return Response{200, BodyFraming::content_length, health_body, health_fields, false};
    }
    if (target == echo_target) {
        if (method != "POST")
            return empty_response(405, echo_allow);
        // The decoder already normalized Content-Length and chunked request
        // framing into message.body, so the echo is framing-agnostic and
        // binary-safe; the body view borrows the message for the serializing
        // call that follows (RequestHandler contract).
        return Response{200, BodyFraming::chunked, message.body, {}, false};
    }
    return empty_response(404, {});
}

// Wiring stub for the T-2 static-serving rung: ignores the store so every
// non-API target answers 404 exactly as before; the static dispatch lands in
// the implementing change together with static_files.cpp.
Response handle_request(const ParsedMessage &message, const StaticFileStore *static_files) {
    static_cast<void>(static_files);
    return handle_request(message);
}

} // namespace orange::http
