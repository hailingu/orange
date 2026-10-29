// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Runtime assembly of the orange loopback service (ADR-0003, subtask T-2):
// one executable binds the two documented fixed loopback listeners — the
// plaintext comparison endpoint on 127.0.0.1:8080 through LoopbackServer and
// the OpenSSL-terminated https listener on 127.0.0.1:8443 through serve_tls
// — serving the declared routes and the immutable static store through one
// handler. Startup failures are coded diagnostics on stderr with a nonzero
// exit (a port conflict never falls back to another port); SIGINT/SIGTERM
// stop both loops and exit 0. Certificate material defaults to the gitignored
// run/ location and is generated on first use and reused afterwards.
//
// Usage: orange_service [--root DIR] [--cert PATH] [--key PATH]
//                       [--port N] [--tls-port N] [--acceptors N] [--idle-ms MS]
// Defaults: --root web/dist  --cert run/cert.pem  --key run/key.pem
//           --port 8080  --tls-port 8443  --acceptors 1  --idle-ms 5000
#include <cstdio>

// Wiring stub for the T-2 both-listener assembly rung: the contract above is
// pinned red by service/tests/http_integration_tests.cpp; the assembly lands
// in the implementing change.
int main(int, char *[]) {
    std::fputs("orange_service: assembly not implemented\n", stderr);
    return 1;
}
