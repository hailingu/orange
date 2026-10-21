// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Pins the route-dispatch contract of T-2's first rung: GET /api/health
// answers its exact body and content type, a method not allowed on the
// declared route answers 405 with Allow, every other target answers 404,
// and every outcome keeps the connection reusable with delimited framing.
#include "routes.hpp"
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {
using orange::http::BodyFraming;
using orange::http::handle_request;
using orange::http::ParsedMessage;
using orange::http::Response;
using orange::http::write_response;

int failures = 0;
std::size_t checks = 0;
// Gives CTest a nonzero exit for any observable dispatch mismatch.
void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}

// One complete parsed message from its full wire form; the optional owns
// the message, so no view dangles into a variant temporary.
std::optional<ParsedMessage> message_of(std::string_view wire) {
    auto parsed = orange::http::parse_message(wire);
    if (auto *message = std::get_if<ParsedMessage>(&parsed))
        return std::move(*message);
    return std::nullopt;
}

// One bodyless request from its method and target.
std::optional<ParsedMessage> request_of(std::string_view method, std::string_view target) {
    return message_of(std::string(method) + " " + std::string(target) +
                      " HTTP/1.1\r\nHost: localhost\r\n\r\n");
}

// The serialized wire bytes, or empty when the response does not serialize.
std::string wire_of(const Response &response) {
    auto result = write_response(response);
    if (const auto *bytes = std::get_if<std::string>(&result))
        return *bytes;
    return {};
}

// GET /api/health answers the exact declared body and content type.
void check_health() {
    const auto request = request_of("GET", "/api/health");
    check(request.has_value(), "health request parses");
    if (!request)
        return;
    const Response response = handle_request(*request);
    check(response.status == 200, "health answers 200");
    check(response.framing == BodyFraming::content_length,
          "health frames its body with Content-Length");
    check(response.body == "{\"status\":\"ok\"}",
          "health body is exactly the declared status object");
    check(response.close == false, "health keeps the connection reusable");
    check(response.fields.size() == 1 && response.fields[0].name == "Content-Type" &&
              response.fields[0].value == "application/json",
          "health declares Content-Type: application/json");
    check(wire_of(response) == "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: 15\r\n"
                               "\r\n"
                               "{\"status\":\"ok\"}",
          "health wire bytes are exact");
}

// POST /api/echo answers the decoded request body through chunked framing,
// whatever framing carried it and whatever bytes it contains.
void check_echo() {
    const std::pair<std::string_view, std::string_view> requests[] = {
        {"POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello", "hello"},
        {"POST /api/echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
         "5\r\nhello\r\n0\r\n\r\n",
         "hello"},
        {"POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 7\r\n\r\nhe\r\nllo",
         "he\r\nllo"},
        {"POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", ""},
    };
    for (const auto &[wire, body] : requests) {
        const auto request = message_of(wire);
        check(request.has_value(), "echo request parses");
        if (!request)
            continue;
        const Response response = handle_request(*request);
        check(response.status == 200, "echo answers 200");
        check(response.framing == BodyFraming::chunked, "echo frames its body with chunked");
        check(response.body == body, "echo answers the decoded request body");
        check(response.fields.empty(), "echo adds no fields");
        check(response.close == false, "echo keeps the connection reusable");
    }
    const auto request =
        message_of("POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello");
    if (request)
        check(wire_of(handle_request(*request)) == "HTTP/1.1 200 OK\r\n"
                                                   "Transfer-Encoding: chunked\r\n"
                                                   "\r\n"
                                                   "5\r\nhello\r\n0\r\n\r\n",
              "echo wire bytes are exact");
    for (const std::string_view method : {"GET", "PUT", "DELETE", "PATCH", "OPTIONS", "TRACE"}) {
        const auto other = request_of(method, "/api/echo");
        check(other.has_value(), "echo method request parses");
        if (!other)
            continue;
        const Response response = handle_request(*other);
        check(response.status == 405, "method not allowed on echo answers 405");
        check(response.fields.size() == 1 && response.fields[0].name == "Allow" &&
                  response.fields[0].value == "POST",
              "echo 405 names Allow: POST");
    }
}

// Methods not allowed on the declared route answer 405 with an Allow field.
void check_method_not_allowed() {
    for (const std::string_view method : {"POST", "PUT", "DELETE", "PATCH", "OPTIONS", "TRACE"}) {
        const auto request = request_of(method, "/api/health");
        check(request.has_value(), "method request parses");
        if (!request)
            continue;
        const Response response = handle_request(*request);
        check(response.status == 405, "method not allowed answers 405");
        check(response.framing == BodyFraming::content_length && response.body.empty(),
              "405 frames its empty body with Content-Length: 0");
        check(response.fields.size() == 1 && response.fields[0].name == "Allow" &&
                  response.fields[0].value == "GET",
              "405 names Allow: GET (RFC 9110 15.5.6)");
        check(response.close == false, "405 keeps the connection reusable");
    }
    const auto request = request_of("POST", "/api/health");
    if (request)
        check(wire_of(handle_request(*request)) == "HTTP/1.1 405 Method Not Allowed\r\n"
                                                   "Allow: GET\r\n"
                                                   "Content-Length: 0\r\n"
                                                   "\r\n",
              "405 wire bytes are exact");
}

// Targets outside the declared routes answer 404, exact and case-sensitive.
void check_not_found() {
    for (const std::string_view method : {"GET", "POST"}) {
        for (const std::string_view target : {"/nope", "/api/nope", "/API/HEALTH"}) {
            const auto request = request_of(method, target);
            check(request.has_value(), "unknown-target request parses");
            if (!request)
                continue;
            const Response response = handle_request(*request);
            check(response.status == 404, "unknown target answers 404");
            check(response.framing == BodyFraming::content_length && response.body.empty(),
                  "404 frames its empty body with Content-Length: 0");
            check(response.fields.empty(), "404 adds no fields");
            check(response.close == false, "404 keeps the connection reusable");
        }
    }
    const auto request = request_of("GET", "/nope");
    if (request)
        check(wire_of(handle_request(*request)) == "HTTP/1.1 404 Not Found\r\n"
                                                   "Content-Length: 0\r\n"
                                                   "\r\n",
              "404 wire bytes are exact");
}
} // namespace

// Executes the declared-route dispatch checks.
int main() {
    check_health();
    check_echo();
    check_method_not_allowed();
    check_not_found();
    std::printf("%zu checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
