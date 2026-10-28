// ADR: docs/adr/ADR-0003-local-runtime-path.md
// TLS termination of the orange loopback service (ADR-0003, subtask T-2):
// ensures the first-run self-signed certificate and serves the declared
// HTTP subset over one OpenSSL-terminated loopback listener. Only the
// protocol layer rides the pinned OpenSSL library; no crypto primitive is
// ever hand-rolled (ADR-0003 Decision).
#ifndef ORANGE_HTTP_TLS_HPP
#define ORANGE_HTTP_TLS_HPP
#include "http_server.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace orange::http {

// The outcome of ensuring the https listener's certificate material.
struct CertificateResult {
    // Both paths hold usable material when this run ended.
    bool ok = false;
    // True when a new certificate was generated; false when existing files
    // were loaded and reused, which is the ordinary restart path (AC-5
    // requires the same certificate across restarts).
    bool generated = false;
    // Coded one-line reason on failure for the assembly diagnostic (CON-07);
    // empty on success.
    std::string diagnostic;
};

// Makes the self-signed certificate available at the two PEM paths. When
// both files exist, parse, and match, they are reused unchanged; otherwise
// an RSA-2048 self-signed certificate for IP 127.0.0.1 (SAN, CN
// orange-local) with long validity is generated, signed with SHA-256, and
// written with owner-only file permissions. All cryptography comes from the
// pinned OpenSSL library.
[[nodiscard]] CertificateResult ensure_self_signed_certificate(const std::string &certificate_path,
                                                               const std::string &key_path);

// The https sibling of serve_loopback: binds 127.0.0.1 only — loopback by
// construction (CON-02), port 0 requesting an ephemeral port reported
// through bound_port — then runs one single-threaded kqueue loop that
// terminates TLS through OpenSSL and serves complete parsed messages
// through `handler` on the same RequestDecoder/write_response pipeline as
// the plaintext loop: keep-alive with `Connection: close` honored, terminal
// parse errors answered with their precomputed rejection wire followed by
// closure and no pipelined follow-up, an idle connection closed after
// idle_timeout without progress, and reading paused while a connection
// holds 1 MiB of undelivered output. A TLS-level failure (handshake or I/O)
// drops that connection without HTTP bytes. Returns the errno-style code of
// the first failing phase — OpenSSL failures map to EIO, unusable
// certificate material to EINVAL — or 0 after a clean stop.
[[nodiscard]] int serve_tls(std::uint16_t port, const std::string &certificate_path,
                            const std::string &key_path, RequestHandler handler,
                            std::chrono::milliseconds idle_timeout, const std::atomic_bool &stop,
                            std::uint16_t *bound_port = nullptr);

} // namespace orange::http
#endif // ORANGE_HTTP_TLS_HPP
