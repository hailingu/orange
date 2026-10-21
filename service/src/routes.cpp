// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Implementation of the T-2 route dispatch; wiring-only until its suite
// turns green.
#include "routes.hpp"

namespace orange::http {

// Wiring-only stub: no route outcome is implemented yet.
Response handle_request(const ParsedMessage &) { return Response{}; }

} // namespace orange::http
