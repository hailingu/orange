// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Implementation of the T-2 TLS rung: the first-run self-signed certificate
// (OpenSSL X.509 generation, reused across restarts) and one single-threaded
// kqueue loop terminating TLS on a loopback listener while reusing the
// RequestDecoder/response pipeline of the plaintext loop.
#include "tls.hpp"
#include "http_connection_store.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace orange::http {
namespace {

using detail::OwnedFd;

// Reading pauses while a connection holds this much undelivered output,
// mirroring the plaintext loop's backpressure bound.
constexpr std::size_t read_pause_cap = 1024 * 1024;
constexpr int listen_backlog = 16;
constexpr int event_batch = 64;
// Every round waits at most this long, so the stop flag and the idle sweep
// progress even with no traffic.
constexpr std::chrono::milliseconds loop_tick{50};

// Sets one descriptor non-blocking; errno of the failing call on error.
int set_non_blocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return errno;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return errno;
    return 0;
}

// Opens one output file with owner-only permissions and hands it to the
// writer; false on failure.
bool write_restricted(const std::string &path, const std::function<bool(FILE *)> &writer) {
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0)
        return false;
    FILE *file = ::fdopen(descriptor, "wb");
    if (file == nullptr) {
        ::close(descriptor);
        return false;
    }
    const bool ok = writer(file);
    std::fclose(file);
    return ok;
}

// True when both PEM files exist, parse, and hold a matching certificate
// and key — the reuse path of an ordinary restart (AC-5).
bool loadable_pair(const std::string &certificate_path, const std::string &key_path) {
    FILE *certificate_file = std::fopen(certificate_path.c_str(), "rb");
    if (certificate_file == nullptr)
        return false;
    X509 *certificate = PEM_read_X509(certificate_file, nullptr, nullptr, nullptr);
    std::fclose(certificate_file);
    if (certificate == nullptr)
        return false;
    FILE *key_file = std::fopen(key_path.c_str(), "rb");
    if (key_file == nullptr) {
        X509_free(certificate);
        return false;
    }
    EVP_PKEY *key = PEM_read_PrivateKey(key_file, nullptr, nullptr, nullptr);
    std::fclose(key_file);
    if (key == nullptr) {
        X509_free(certificate);
        return false;
    }
    const bool matches = X509_check_private_key(certificate, key) == 1;
    EVP_PKEY_free(key);
    X509_free(certificate);
    return matches;
}

// Appends the loopback Subject Alternative Name (IP 127.0.0.1 and DNS
// localhost); consumes its temporaries on every path.
bool add_subject_alt_names(X509 *certificate) {
    GENERAL_NAMES *names = sk_GENERAL_NAME_new_null();
    GENERAL_NAME *loopback = names != nullptr ? GENERAL_NAME_new() : nullptr;
    GENERAL_NAME *localhost = names != nullptr ? GENERAL_NAME_new() : nullptr;
    ASN1_OCTET_STRING *address = a2i_IPADDRESS("127.0.0.1");
    ASN1_STRING *dns = ASN1_STRING_new();
    bool ok = loopback != nullptr && localhost != nullptr && address != nullptr && dns != nullptr &&
              ASN1_STRING_set(dns, "localhost", -1) == 1;
    if (ok) {
        GENERAL_NAME_set0_value(loopback, GEN_IPADD, address);
        GENERAL_NAME_set0_value(localhost, GEN_DNS, dns);
        address = nullptr;
        dns = nullptr;
        // sk push returns the new element count, so any positive value is
        // success; a freed name is only one the stack did not take over.
        ok = sk_GENERAL_NAME_push(names, loopback) >= 1;
        if (!ok)
            GENERAL_NAME_free(loopback);
        if (ok) {
            ok = sk_GENERAL_NAME_push(names, localhost) >= 1;
            if (!ok)
                GENERAL_NAME_free(localhost);
        }
    } else {
        GENERAL_NAME_free(loopback);
        GENERAL_NAME_free(localhost);
        ASN1_OCTET_STRING_free(address);
        ASN1_STRING_free(dns);
    }
    if (ok)
        ok =
            X509_add1_ext_i2d(certificate, NID_subject_alt_name, names, 0, X509V3_ADD_REPLACE) == 1;
    sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
    return ok;
}

// One fresh RSA-2048 self-signed certificate for the loopback origin, or
// nullptr with the caller keeping the key.
X509 *make_certificate(EVP_PKEY *key) {
    X509 *certificate = X509_new();
    if (certificate == nullptr)
        return nullptr;
    const bool built =
        X509_set_version(certificate, 2) == 1 && X509_set_pubkey(certificate, key) == 1 &&
        X509_gmtime_adj(X509_getm_notBefore(certificate), 0) != nullptr &&
        X509_gmtime_adj(X509_getm_notAfter(certificate), 10L * 365 * 24 * 3600) != nullptr;
    if (!built)
        goto failed;
    {
        BIGNUM *random = BN_new();
        ASN1_INTEGER *serial = nullptr;
        bool ok = random != nullptr && BN_rand(random, 64, -1, 0) == 1;
        if (ok) {
            serial = BN_to_ASN1_INTEGER(random, nullptr);
            ok = serial != nullptr && X509_set_serialNumber(certificate, serial) == 1;
            ASN1_INTEGER_free(serial);
        }
        BN_free(random);
        if (!ok)
            goto failed;
    }
    {
        X509_NAME *name = X509_get_subject_name(certificate);
        const bool named =
            name != nullptr &&
            X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                       reinterpret_cast<const unsigned char *>("orange-local"), -1,
                                       -1, 0) == 1 &&
            X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                                       reinterpret_cast<const unsigned char *>("orange"), -1, -1,
                                       0) == 1 &&
            X509_set_issuer_name(certificate, name) == 1;
        if (!named)
            goto failed;
    }
    if (!add_subject_alt_names(certificate))
        goto failed;
    if (X509_sign(certificate, key, EVP_sha256()) == 0)
        goto failed;
    return certificate;
failed:
    X509_free(certificate);
    return nullptr;
}

// One TLS server context with the certificate material and the declared
// options: TLS 1.2 floor, moving-write-buffer acceptance (responses append
// to the outgoing string between SSL_write retries), no client certs.
SSL_CTX *make_server_context(const std::string &certificate_path, const std::string &key_path) {
    SSL_CTX *context = SSL_CTX_new(TLS_server_method());
    if (context == nullptr)
        return nullptr;
    SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
    SSL_CTX_set_mode(context, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
    if (SSL_CTX_use_certificate_chain_file(context, certificate_path.c_str()) != 1 ||
        SSL_CTX_use_PrivateKey_file(context, key_path.c_str(), SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(context) != 1) {
        SSL_CTX_free(context);
        return nullptr;
    }
    return context;
}

// One served TLS connection and its place in the loop's state.
struct TlsConnection {
    int fd = -1;
    SSL *ssl = nullptr;
    std::unique_ptr<RequestDecoder> decoder;
    std::string outgoing;
    std::size_t outgoing_offset = 0;
    bool handshake_done = false;
    bool peer_eof = false;
    bool closing_after_flush = false;
    bool want_write_io = false;
    bool dead = false;
    std::chrono::steady_clock::time_point idle_deadline{};
};

// The read/write interest the loop wants for one connection right now.
struct WantedInterest {
    bool read;
    bool write;
};
WantedInterest wanted_interest(const TlsConnection &connection) {
    if (!connection.handshake_done)
        return {!connection.want_write_io, connection.want_write_io};
    return {!connection.closing_after_flush && !connection.peer_eof &&
                connection.outgoing.size() - connection.outgoing_offset < read_pause_cap,
            connection.outgoing_offset < connection.outgoing.size()};
}

// One tracked kernel filter state, the plaintext loop's absent/enabled/
// disabled shape.
enum class Interest { absent, enabled, disabled };
struct FilterInterest {
    Interest read = Interest::absent;
    Interest write = Interest::absent;
};

// True when the tracked state differs from the wanted state.
bool transition_pending(Interest state, bool want_enabled) {
    return state == Interest::absent ? want_enabled : want_enabled != (state == Interest::enabled);
}

// Queues the changelist entries that move one connection to its wanted
// interest; returns the number of entries appended.
std::size_t queue_interest(struct kevent *changes, std::size_t capacity, int descriptor,
                           FilterInterest &tracked, const WantedInterest &wanted) {
    std::size_t count = 0;
    if (transition_pending(tracked.read, wanted.read)) {
        const unsigned short flags =
            tracked.read == Interest::absent ? EV_ADD : (wanted.read ? EV_ENABLE : EV_DISABLE);
        EV_SET(&changes[count++], descriptor, EVFILT_READ, flags, 0, 0, nullptr);
        tracked.read = wanted.read ? Interest::enabled : Interest::disabled;
    }
    if (count < capacity && transition_pending(tracked.write, wanted.write)) {
        const unsigned short flags =
            tracked.write == Interest::absent ? EV_ADD : (wanted.write ? EV_ENABLE : EV_DISABLE);
        EV_SET(&changes[count++], descriptor, EVFILT_WRITE, flags, 0, 0, nullptr);
        tracked.write = wanted.write ? Interest::enabled : Interest::disabled;
    }
    return count;
}

// Binds the loopback listener; errno of the failing call through `error`.
OwnedFd make_tls_listener(std::uint16_t port, std::uint16_t *bound_port, int &error) {
    OwnedFd descriptor{::socket(AF_INET, SOCK_STREAM, 0)};
    if (!descriptor.valid()) {
        error = errno;
        return descriptor;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int reuse = 1;
    if (set_non_blocking(descriptor.get()) != 0 ||
        ::setsockopt(descriptor.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        ::bind(descriptor.get(), reinterpret_cast<const sockaddr *>(&address), sizeof(address)) <
            0 ||
        ::listen(descriptor.get(), listen_backlog) < 0) {
        error = errno;
        descriptor.reset();
        return descriptor;
    }
    sockaddr_in bound{};
    socklen_t size = sizeof(bound);
    if (::getsockname(descriptor.get(), reinterpret_cast<sockaddr *>(&bound), &size) == 0 &&
        bound_port != nullptr)
        *bound_port = ntohs(bound.sin_port);
    return descriptor;
}

// Advances one connection's handshake; 0 while it continues, 1 when
// established, -1 on a TLS-level failure.
int step_handshake(TlsConnection &connection) {
    const int result = SSL_accept(connection.ssl);
    if (result == 1) {
        connection.handshake_done = true;
        connection.want_write_io = false;
        return 1;
    }
    const int failure = SSL_get_error(connection.ssl, result);
    if (failure == SSL_ERROR_WANT_READ) {
        connection.want_write_io = false;
        return 0;
    }
    if (failure == SSL_ERROR_WANT_WRITE) {
        connection.want_write_io = true;
        return 0;
    }
    return -1;
}

// Feeds decrypted bytes to the decoder and appends every completed
// response to the outgoing backlog — the plaintext pipeline verbatim.
void drive_messages(TlsConnection &connection, std::string_view input,
                    const RequestHandler &handler) {
    std::size_t offset = 0;
    while (offset < input.size() && !connection.closing_after_flush) {
        DecodeResult result = connection.decoder->feed(input.substr(offset));
        offset += result.consumed;
        if (auto *message = std::get_if<ParsedMessage>(&result.outcome)) {
            Response response = handler(*message);
            response.close = response.close || message->close;
            if (write_response_into(response, connection.outgoing).has_value()) {
                connection.outgoing.append(error_wire(ParseError::resource_exhausted));
                connection.closing_after_flush = true;
            } else if (response.close) {
                connection.closing_after_flush = true;
            }
            connection.decoder->reset();
        } else if (auto *failure = std::get_if<ParseError>(&result.outcome)) {
            connection.outgoing.append(error_wire(*failure));
            connection.closing_after_flush = true;
            return;
        } else if (result.consumed == 0) {
            return;
        }
    }
}

// Handles read readiness: handshake progress or decrypted bytes; drops the
// connection on a TLS-level failure.
void on_readable(TlsConnection &connection, const RequestHandler &handler,
                 std::chrono::milliseconds idle) {
    const auto now = std::chrono::steady_clock::now();
    if (!connection.handshake_done) {
        if (step_handshake(connection) < 0)
            connection.dead = true;
        connection.idle_deadline = now + idle;
        return;
    }
    char buffer[16 * 1024];
    const int got = SSL_read(connection.ssl, buffer, sizeof(buffer));
    if (got > 0) {
        drive_messages(connection, std::string_view(buffer, static_cast<std::size_t>(got)),
                       handler);
        connection.idle_deadline = now + idle;
        return;
    }
    const int failure = SSL_get_error(connection.ssl, got);
    if (failure == SSL_ERROR_ZERO_RETURN) {
        connection.peer_eof = true;
        if (connection.outgoing_offset >= connection.outgoing.size()) {
            SSL_shutdown(connection.ssl);
            connection.dead = true;
        }
        return;
    }
    if (failure == SSL_ERROR_WANT_READ)
        return;
    if (failure == SSL_ERROR_WANT_WRITE) {
        connection.want_write_io = true;
        return;
    }
    connection.dead = true;
}

// Handles write readiness: flushes the outgoing backlog, then closes the
// connection when its last response declared closure.
void on_writable(TlsConnection &connection, std::chrono::milliseconds idle) {
    const auto now = std::chrono::steady_clock::now();
    if (!connection.handshake_done) {
        if (step_handshake(connection) < 0)
            connection.dead = true;
        connection.idle_deadline = now + idle;
        return;
    }
    while (connection.outgoing_offset < connection.outgoing.size()) {
        const std::size_t remaining = connection.outgoing.size() - connection.outgoing_offset;
        const int wrote =
            SSL_write(connection.ssl, connection.outgoing.data() + connection.outgoing_offset,
                      static_cast<int>(remaining));
        if (wrote > 0) {
            connection.outgoing_offset += static_cast<std::size_t>(wrote);
            continue;
        }
        const int failure = SSL_get_error(connection.ssl, wrote);
        if (failure == SSL_ERROR_WANT_WRITE || failure == SSL_ERROR_WANT_READ)
            return;
        connection.dead = true;
        return;
    }
    connection.outgoing.clear();
    connection.outgoing_offset = 0;
    connection.idle_deadline = now + idle;
    if (connection.closing_after_flush || connection.peer_eof) {
        SSL_shutdown(connection.ssl);
        connection.dead = true;
    }
}

} // namespace

CertificateResult ensure_self_signed_certificate(const std::string &certificate_path,
                                                 const std::string &key_path) {
    if (loadable_pair(certificate_path, key_path))
        return CertificateResult{true, false, ""};
    ERR_clear_error();
    EVP_PKEY *key = EVP_RSA_gen(2048);
    if (key == nullptr)
        return CertificateResult{false, false, "EVP_RSA_gen failed"};
    X509 *certificate = make_certificate(key);
    if (certificate == nullptr) {
        EVP_PKEY_free(key);
        return CertificateResult{false, false, "certificate generation failed"};
    }
    const bool written =
        write_restricted(
            certificate_path,
            [certificate](FILE *file) { return PEM_write_X509(file, certificate) == 1; }) &&
        write_restricted(key_path, [key](FILE *file) {
            return PEM_write_PrivateKey(file, key, nullptr, nullptr, 0, nullptr, nullptr) == 1;
        });
    X509_free(certificate);
    EVP_PKEY_free(key);
    if (!written) {
        std::remove(certificate_path.c_str());
        std::remove(key_path.c_str());
        return CertificateResult{false, false, "writing the PEM files failed"};
    }
    return CertificateResult{true, true, ""};
}

int serve_tls(std::uint16_t port, const std::string &certificate_path, const std::string &key_path,
              RequestHandler handler, std::chrono::milliseconds idle_timeout,
              const std::atomic_bool &stop, std::uint16_t *bound_port) {
    if (!handler)
        return EINVAL;
    if (!ensure_self_signed_certificate(certificate_path, key_path).ok)
        return EINVAL;
    SSL_CTX *context = make_server_context(certificate_path, key_path);
    if (context == nullptr)
        return EINVAL;
    int error = 0;
    OwnedFd listener = make_tls_listener(port, bound_port, error);
    if (!listener.valid()) {
        SSL_CTX_free(context);
        return error;
    }
    OwnedFd queue{::kqueue()};
    if (!queue.valid()) {
        error = errno;
        SSL_CTX_free(context);
        return error;
    }
    struct kevent listener_change{};
    EV_SET(&listener_change, listener.get(), EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (::kevent(queue.get(), &listener_change, 1, nullptr, 0, nullptr) < 0) {
        error = errno;
        SSL_CTX_free(context);
        return error;
    }

    std::vector<TlsConnection> connections;
    std::vector<FilterInterest> tracked;
    int first_error = 0;
    while (!stop.load()) {
        const auto now = std::chrono::steady_clock::now();
        for (TlsConnection &connection : connections) {
            if (!connection.dead && now >= connection.idle_deadline)
                connection.dead = true;
        }
        struct kevent changes[event_batch];
        std::size_t change_count = 0;
        for (std::size_t i = 0; i < connections.size() && change_count + 2 <= event_batch; ++i) {
            if (connections[i].dead)
                continue;
            change_count +=
                queue_interest(changes + change_count, event_batch - change_count,
                               connections[i].fd, tracked[i], wanted_interest(connections[i]));
        }
        struct kevent events[event_batch];
        const timespec tick{0, static_cast<long>(loop_tick.count()) * 1000 * 1000};
        const int received = ::kevent(queue.get(), changes, static_cast<int>(change_count), events,
                                      event_batch, &tick);
        if (received < 0) {
            if (errno == EINTR)
                continue;
            first_error = errno;
            break;
        }
        for (int e = 0; e < received; ++e) {
            const struct kevent &event = events[e];
            if (static_cast<int>(event.ident) == listener.get()) {
                if (event.flags & EV_ERROR) {
                    first_error = static_cast<int>(event.data);
                    break;
                }
                for (;;) {
                    const int accepted = ::accept(listener.get(), nullptr, nullptr);
                    if (accepted < 0)
                        break;
                    const int option = 1;
                    ::setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));
                    ::setsockopt(accepted, SOL_SOCKET, SO_NOSIGPIPE, &option, sizeof(option));
                    set_non_blocking(accepted);
                    SSL *ssl = SSL_new(context);
                    if (ssl == nullptr) {
                        ::close(accepted);
                        break;
                    }
                    SSL_set_fd(ssl, accepted);
                    TlsConnection connection;
                    connection.fd = accepted;
                    connection.ssl = ssl;
                    connection.decoder = std::make_unique<RequestDecoder>();
                    connection.idle_deadline = std::chrono::steady_clock::now() + idle_timeout;
                    step_handshake(connection);
                    connections.push_back(std::move(connection));
                    tracked.push_back(FilterInterest{});
                }
                continue;
            }
            for (TlsConnection &connection : connections) {
                if (connection.dead || connection.fd != static_cast<int>(event.ident))
                    continue;
                if (event.flags & EV_ERROR) {
                    connection.dead = true;
                    break;
                }
                if (event.filter == EVFILT_READ)
                    on_readable(connection, handler, idle_timeout);
                else if (event.filter == EVFILT_WRITE)
                    on_writable(connection, idle_timeout);
                break;
            }
        }
        if (first_error != 0)
            break;
        // Release dropped connections after the round, sending close_notify
        // best-effort, so a descriptor number can never be reused by a
        // same-round accept.
        for (std::size_t i = connections.size(); i-- > 0;) {
            if (!connections[i].dead)
                continue;
            SSL_shutdown(connections[i].ssl);
            SSL_free(connections[i].ssl);
            ::close(connections[i].fd);
            connections.erase(connections.begin() + static_cast<std::ptrdiff_t>(i));
            tracked.erase(tracked.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    for (TlsConnection &connection : connections)
        SSL_free(connection.ssl);
    SSL_CTX_free(context);
    return first_error;
}

} // namespace orange::http
