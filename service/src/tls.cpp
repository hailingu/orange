// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Wiring stub for the T-2 TLS rung: the public contract is declared in
// tls.hpp and pinned red by service/tests/http_tls_tests.cpp; the
// certificate material and the OpenSSL-terminated loop land in the
// implementing change.
#include "tls.hpp"
#include <cerrno>

namespace orange::http {

CertificateResult ensure_self_signed_certificate(const std::string &certificate_path,
                                                 const std::string &key_path) {
    static_cast<void>(certificate_path);
    static_cast<void>(key_path);
    return CertificateResult{};
}

int serve_tls(std::uint16_t port, const std::string &certificate_path, const std::string &key_path,
              RequestHandler handler, std::chrono::milliseconds idle_timeout,
              const std::atomic_bool &stop, std::uint16_t *bound_port) {
    static_cast<void>(port);
    static_cast<void>(certificate_path);
    static_cast<void>(key_path);
    static_cast<void>(handler);
    static_cast<void>(idle_timeout);
    static_cast<void>(stop);
    static_cast<void>(bound_port);
    return EIO;
}

} // namespace orange::http
