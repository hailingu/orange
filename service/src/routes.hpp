// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Route dispatch of the orange loopback service (ADR-0003, subtask T-2):
// maps one complete parsed message to its response. These rungs own the
// declared diagnostic routes GET /api/health and POST /api/echo and the
// 405/404 outcomes; static serving and the TLS listener land in later
// T-2 rungs.
#ifndef ORANGE_HTTP_ROUTES_HPP
#define ORANGE_HTTP_ROUTES_HPP
#include "http_request_decoder.hpp"
#include "http_response.hpp"

namespace orange::http {

// Serves one complete parsed message by dispatching on its method and
// request-target; the signature is the RequestHandler shape, so the value
// can be handed to serve_loopback directly. Dispatch is exact and
// case-sensitive on the origin-form target: GET /api/health answers 200
// with exactly {"status":"ok"} as application/json; POST /api/echo answers
// 200 with the decoded request body through chunked framing (binary-safe,
// whatever framing carried the request); a method not allowed on a
// declared route answers 405 with an Allow field naming the supported
// methods (RFC 9110 15.5.6); every other target answers 404. Empty bodies
// are framed with Content-Length: 0 so keep-alive clients can delimit the
// response without a connection close (RFC 9112 6). The returned body and
// field views borrow the message and stay valid for the serializing call
// that follows. Policy for methods the parser accepts but the routes do
// not recognize (501 versus 405), for non-1.x request versions (505), and
// for non-origin target forms stays with the T-2 owner determinations
// recorded in ADR-0003 and is not decided by these rungs.
[[nodiscard]] Response handle_request(const ParsedMessage &message);

} // namespace orange::http
#endif // ORANGE_HTTP_ROUTES_HPP
