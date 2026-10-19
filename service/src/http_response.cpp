// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Wiring-only placeholder: the behavioral contract in
// service/tests/http_response_tests.cpp lands red before the serializer
// replaces this stub in the green change.
#include "http_response.hpp"

namespace orange::http {

// Returns the typed error channel only, so the committed red suite is
// exercised against a linkable, behavior-free boundary.
std::variant<std::string, ResponseError> write_response(const Response &response) {
    (void)response;
    return ResponseError::resource_exhausted;
}

} // namespace orange::http
