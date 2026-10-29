// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Implementation of the T-2 route dispatch: the declared diagnostic routes
// and the 405/404 outcomes for methods and targets outside them, plus static
// serving through the immutable store. The TLS listener lands in a later
// T-2 rung.
#include "routes.hpp"
#include <optional>
#include <span>
#include <string_view>

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
// A resolved static asset serves GET only; the pair has static storage so
// the returned field views outlive the call (RequestHandler borrowing
// contract).
constexpr ResponseField static_allow[] = {{"Allow", "GET"}};

// An empty-body outcome framed with Content-Length: 0 so a keep-alive
// client can delimit the response without a connection close (RFC 9112 6).
Response empty_response(unsigned int status, std::span<const ResponseField> fields) {
    return Response{status, BodyFraming::content_length, "", fields, false};
}

// The declared API routes, when the message addresses one; nullopt leaves
// the target to static resolution or the 404 outcome.
std::optional<Response> api_route(const ParsedMessage &message) {
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
    return std::nullopt;
}

} // namespace

Response handle_request(const ParsedMessage &message) {
    if (const auto response = api_route(message))
        return *response;
    return empty_response(404, {});
}

Response handle_request(const ParsedMessage &message, const StaticFileStore *static_files) {
    if (const auto response = api_route(message))
        return *response;
    if (static_files != nullptr) {
        if (const StaticAsset *asset = static_files->find(message.request.target())) {
            if (message.request.method() != "GET")
                return empty_response(405, static_allow);
            return StaticFileStore::serve_get(*asset, message);
        }
    }
    return empty_response(404, {});
}

} // namespace orange::http
