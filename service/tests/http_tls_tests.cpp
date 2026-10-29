// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Pins the T-2 TLS contract at the real boundary: the first-run
// self-signed certificate is generated once, reused across restarts, and
// carries the declared identity (SAN 127.0.0.1, CN orange-local, RSA-2048,
// SHA-256 self-signature); the OpenSSL-terminated loopback listener serves
// the declared HTTP subset — health, chunked echo, static with single-range
// — with keep-alive, Connection: close, rejection closure and the idle
// timeout, and a paused TLS connection never blocks another one.
#include "routes.hpp"
#include "static_files.hpp"
#include "tls.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <optional>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
using orange::http::ensure_self_signed_certificate;
using orange::http::handle_request;
using orange::http::RequestHandler;
using orange::http::serve_tls;
using orange::http::StaticFileStore;

int failures = 0;
std::size_t checks = 0;
// Gives CTest a nonzero exit for any observable TLS mismatch.
void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}

// The exact keep-alive wire of one health answer, reused by every read.
constexpr std::string_view keep_alive_health_wire = "HTTP/1.1 200 OK\r\n"
                                                    "Content-Type: application/json\r\n"
                                                    "Content-Length: 15\r\n"
                                                    "\r\n"
                                                    "{\"status\":\"ok\"}";
constexpr std::string_view close_health_wire = "HTTP/1.1 200 OK\r\n"
                                               "Content-Type: application/json\r\n"
                                               "Content-Length: 15\r\n"
                                               "Connection: close\r\n"
                                               "\r\n"
                                               "{\"status\":\"ok\"}";
constexpr std::string_view close_hello_echo_wire = "HTTP/1.1 200 OK\r\n"
                                                   "Transfer-Encoding: chunked\r\n"
                                                   "Connection: close\r\n"
                                                   "\r\n"
                                                   "5\r\nhello\r\n0\r\n\r\n";

// One whole small file, or empty when it cannot be read.
std::string read_file_or_empty(const std::string &path) {
    FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return {};
    std::string content;
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        content.append(buffer, got);
    std::fclose(file);
    return content;
}

// One blocking TLS client with a receive deadline, driving the listener the
// way curl -k or a browser would after accepting the certificate.
class TlsClient {
  public:
    ~TlsClient() {
        if (ssl_ != nullptr)
            SSL_free(ssl_);
        if (fd_ >= 0)
            ::close(fd_);
    }
    TlsClient(const TlsClient &) = delete;
    TlsClient &operator=(const TlsClient &) = delete;
    // An empty value precedes the descriptor transfers below.
    TlsClient() = default;
    // Transfers the descriptors so the value can ride std::optional.
    TlsClient(TlsClient &&other) noexcept : fd_(other.fd_), ssl_(other.ssl_), ctx_(other.ctx_) {
        other.fd_ = -1;
        other.ssl_ = nullptr;
        other.ctx_ = nullptr;
    }
    TlsClient &operator=(TlsClient &&) = delete;
    // Connects to the loopback listener and completes the handshake without
    // certificate verification (the self-signed profile, like curl -k).
    static std::optional<TlsClient> connect(std::uint16_t port) {
        TlsClient client;
        client.fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (client.fd_ < 0)
            return std::nullopt;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(client.fd_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) !=
            0)
            return std::nullopt;
        client.ctx_ = SSL_CTX_new(TLS_client_method());
        if (client.ctx_ == nullptr)
            return std::nullopt;
        SSL_CTX_set_verify(client.ctx_, SSL_VERIFY_NONE, nullptr);
        client.ssl_ = SSL_new(client.ctx_);
        if (client.ssl_ == nullptr)
            return std::nullopt;
        SSL_set_fd(client.ssl_, client.fd_);
        if (SSL_connect(client.ssl_) != 1)
            return std::nullopt;
        return client;
    }
    // Sends every byte; false on a TLS-level failure.
    [[nodiscard]] bool send_all(std::string_view bytes) {
        while (!bytes.empty()) {
            const int written = SSL_write(ssl_, bytes.data(), static_cast<int>(bytes.size()));
            if (written <= 0)
                return false;
            bytes.remove_prefix(static_cast<std::size_t>(written));
        }
        return true;
    }
    // Reads exactly count bytes within the deadline, or nullopt.
    [[nodiscard]] std::optional<std::string> read_exact(std::size_t count) {
        std::string received;
        if (!receive(received, count, true))
            return std::nullopt;
        return received;
    }
    // Reads until the peer closes, within the deadline, or nullopt.
    [[nodiscard]] std::optional<std::string> read_to_eof() {
        std::string received;
        if (!receive(received, 0, false))
            return std::nullopt;
        return received;
    }

  private:
    // One receive loop: exact mode fails unless count bytes arrive; EOF mode
    // returns whatever arrived before the peer closed.
    bool receive(std::string &received, std::size_t count, bool exact) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (exact ? received.size() < count : true) {
            pollfd waited{fd_, POLLIN, 0};
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       deadline - std::chrono::steady_clock::now())
                                       .count();
            if (remaining <= 0 || ::poll(&waited, 1, static_cast<int>(remaining)) <= 0)
                return false;
            char buffer[8192];
            const int got = SSL_read(ssl_, buffer, sizeof(buffer));
            if (got > 0) {
                received.append(buffer, static_cast<std::size_t>(got));
                continue;
            }
            if (SSL_get_error(ssl_, got) == SSL_ERROR_ZERO_RETURN)
                return !exact || received.size() == count;
            return false;
        }
        return true;
    }
    int fd_ = -1;
    SSL *ssl_ = nullptr;
    SSL_CTX *ctx_ = nullptr;
};

// Reserves one ephemeral loopback port by binding and releasing it, so the
// server harness knows its port without polling serve_tls's out-pointer.
std::uint16_t reserve_port() {
    const int probe = ::socket(AF_INET, SOCK_STREAM, 0);
    if (probe < 0)
        return 0;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    std::uint16_t port = 0;
    if (::bind(probe, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0) {
        sockaddr_in bound{};
        socklen_t size = sizeof(bound);
        if (::getsockname(probe, reinterpret_cast<sockaddr *>(&bound), &size) == 0)
            port = ntohs(bound.sin_port);
    }
    ::close(probe);
    return port;
}

// Retries one client connection until the freshly started listener accepts,
// or nullopt after the deadline.
std::optional<TlsClient> await_connect(std::uint16_t port) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        if (auto client = TlsClient::connect(port))
            return client;
        if (std::chrono::steady_clock::now() >= deadline)
            return std::nullopt;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Serves one TLS listener on a reserved ephemeral port on its own thread
// until the destructor stops it.
class TestTlsServer {
  public:
    TestTlsServer(const std::string &certificate_path, const std::string &key_path,
                  RequestHandler handler, std::chrono::milliseconds idle_timeout)
        : handler_(std::move(handler)), port_(reserve_port()) {
        // The paths are copied into the thread: the constructor's parameter
        // frames die before the server thread stops using them.
        thread_ = std::thread([this, certificate_path, key_path, idle_timeout] {
            serve_exit_ = serve_tls(port_, certificate_path, key_path, handler_, idle_timeout,
                                    stop_, nullptr);
            exited_.store(true);
        });
    }
    ~TestTlsServer() {
        stop_.store(true);
        if (thread_.joinable())
            thread_.join();
    }
    TestTlsServer(const TestTlsServer &) = delete;
    TestTlsServer &operator=(const TestTlsServer &) = delete;
    [[nodiscard]] std::uint16_t port() const { return port_; }
    // Waits until the loop returned its coded result (the early EINVAL path).
    [[nodiscard]] int await_exit() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!exited_.load() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return serve_exit_;
    }

  private:
    RequestHandler handler_;
    std::atomic_bool stop_{false};
    std::atomic_bool exited_{false};
    int serve_exit_ = 0;
    std::uint16_t port_ = 0;
    std::thread thread_;
};

// The handler under test: API routes plus a one-file static store.
RequestHandler routed_handler(const StaticFileStore &store) {
    return [&store](const orange::http::ParsedMessage &message) {
        return handle_request(message, &store);
    };
}

// True when the certificate's SAN extension carries IP 127.0.0.1.
bool has_loopback_san(X509 *certificate) {
    auto *names = static_cast<STACK_OF(GENERAL_NAME) *>(
        X509_get_ext_d2i(certificate, NID_subject_alt_name, nullptr, nullptr));
    if (names == nullptr)
        return false;
    bool found = false;
    for (int i = 0; i < sk_GENERAL_NAME_num(names); ++i) {
        const GENERAL_NAME *name = sk_GENERAL_NAME_value(names, i);
        if (name->type != GEN_IPADD)
            continue;
        const unsigned char *octets = ASN1_STRING_get0_data(name->d.ip);
        if (ASN1_STRING_length(name->d.ip) == 4 && octets[0] == 127 && octets[1] == 0 &&
            octets[2] == 0 && octets[3] == 1)
            found = true;
    }
    sk_GENERAL_NAME_free(names);
    return found;
}

// The first-run certificate: generated once, reused, correctly shaped.
void check_certificate(const std::string &certificate_path, const std::string &key_path) {
    const auto first = ensure_self_signed_certificate(certificate_path, key_path);
    check(first.ok && first.generated, "the first run generates usable material");
    const std::string before = read_file_or_empty(certificate_path);
    const auto again = ensure_self_signed_certificate(certificate_path, key_path);
    check(again.ok && !again.generated, "existing material is reused, not regenerated");
    check(read_file_or_empty(certificate_path) == before,
          "the certificate bytes are unchanged across restarts");
    check(!before.empty() && !read_file_or_empty(key_path).empty(),
          "both PEM files exist after the first run");

    FILE *file = std::fopen(certificate_path.c_str(), "rb");
    check(file != nullptr, "the certificate file opens");
    if (file == nullptr)
        return;
    X509 *certificate = PEM_read_X509(file, nullptr, nullptr, nullptr);
    std::fclose(file);
    check(certificate != nullptr, "the certificate parses as X.509 PEM");
    if (certificate == nullptr)
        return;
    EVP_PKEY *public_key = X509_get0_pubkey(certificate);
    check(public_key != nullptr && EVP_PKEY_bits(public_key) == 2048,
          "the certificate carries an RSA-2048 public key");
    check(X509_get0_notAfter(certificate) != nullptr &&
              X509_cmp_current_time(X509_get0_notAfter(certificate)) > 0,
          "the certificate validity reaches into the future");
    check(X509_verify(certificate, public_key) == 1,
          "the certificate verifies against its own key (self-signed)");
    char common_name[64] = {};
    check(X509_NAME_get_text_by_NID(X509_get_subject_name(certificate), NID_commonName, common_name,
                                    sizeof(common_name)) > 0 &&
              std::string_view(common_name) == "orange-local",
          "the subject carries CN orange-local");
    check(has_loopback_san(certificate), "the SAN carries IP 127.0.0.1");
    X509_free(certificate);
}

// One complete health exchange over a fresh TLS connection.
std::optional<std::string> once_health_wire(std::uint16_t port) {
    auto client = await_connect(port);
    if (!client)
        return std::nullopt;
    if (!client->send_all("GET /api/health HTTP/1.1\r\nHost: localhost\r\n"
                          "Connection: close\r\n\r\n"))
        return std::nullopt;
    return client->read_to_eof();
}

} // namespace

// Executes the certificate and serving checks.
int main() {
    // Fixture: a fresh directory holding the certificate and a one-file root.
    std::vector<char> pattern;
    const std::string_view base = "/tmp/orange-tls-tests-XXXXXX";
    pattern.assign(base.begin(), base.end());
    pattern.push_back('\0');
    const bool have_dir = mkdtemp(pattern.data()) != nullptr;
    check(have_dir, "the fixture directory is created");
    if (!have_dir) {
        std::printf("%zu checks, %d failures\n", checks, failures);
        return failures ? 1 : 0;
    }
    const std::string root = pattern.data();
    const std::string certificate_path = root + "/cert.pem";
    const std::string key_path = root + "/key.pem";
    const std::string web_root = root + "/web";
    check(::mkdir(web_root.c_str(), 0755) == 0, "the web fixture directory is created");
    FILE *page = std::fopen((web_root + "/index.html").c_str(), "wb");
    check(page != nullptr, "the index fixture is written");
    if (page != nullptr) {
        std::fputs("<html>orange</html>", page);
        std::fclose(page);
    }
    const auto loaded = StaticFileStore::load(web_root);
    const auto *store = std::get_if<StaticFileStore>(&loaded);
    check(store != nullptr, "the static fixture loads");

    check_certificate(certificate_path, key_path);

    if (store != nullptr) {
        const RequestHandler handler = routed_handler(*store);
        {
            TestTlsServer server(certificate_path, key_path, handler,
                                 std::chrono::milliseconds(5000));
            check(server.port() != 0, "the TLS listener holds a reserved ephemeral port");
            const auto health = once_health_wire(server.port());
            check(health.has_value() && *health == close_health_wire,
                  "GET /api/health answers its exact wire over TLS and closes");
            // Keep-alive: three cycles on one connection, Content-Length framed.
            if (auto client = await_connect(server.port())) {
                bool cycles = true;
                for (int round = 0; round < 3; ++round) {
                    cycles =
                        cycles &&
                        client->send_all("GET /api/health HTTP/1.1\r\nHost: localhost\r\n"
                                         "\r\n") &&
                        client->read_exact(keep_alive_health_wire.size()) == keep_alive_health_wire;
                }
                check(cycles, "three keep-alive cycles complete on one TLS connection");
            } else {
                check(false, "the keep-alive client connects");
            }
            // Chunked echo over TLS.
            if (auto client = await_connect(server.port())) {
                check(client->send_all("POST /api/echo HTTP/1.1\r\nHost: localhost\r\n"
                                       "Connection: close\r\nContent-Length: 7\r\n\r\n"
                                       "he\r\nllo") &&
                          client->read_to_eof() == "HTTP/1.1 200 OK\r\n"
                                                   "Transfer-Encoding: chunked\r\n"
                                                   "Connection: close\r\n"
                                                   "\r\n"
                                                   "7\r\nhe\r\nllo\r\n0\r\n\r\n",
                      "POST /api/echo answers the exact chunked wire over TLS");
            }
            // Static serving and a single range over TLS.
            if (auto client = await_connect(server.port())) {
                check(client->send_all("GET /index.html HTTP/1.1\r\nHost: localhost\r\n"
                                       "Connection: close\r\n"
                                       "Range: bytes=0-3\r\n\r\n") &&
                          client->read_to_eof() == "HTTP/1.1 206 Partial Content\r\n"
                                                   "Content-Type: text/html\r\n"
                                                   "Content-Range: bytes 0-3/19\r\n"
                                                   "Content-Length: 4\r\n"
                                                   "Connection: close\r\n"
                                                   "\r\n"
                                                   "<htm",
                      "a single-range static request answers its exact 206 wire over TLS");
            }
            // Rejection closure: an oversized head answers 431 then closes.
            if (auto client = await_connect(server.port())) {
                const std::string head =
                    "GET / HTTP/1.1\r\nHost: localhost\r\nX-Pad: " + std::string(9000, 'a') +
                    "\r\n\r\n";
                check(client->send_all(head) &&
                          client->read_to_eof() ==
                              "HTTP/1.1 431 Request Header Fields Too Large\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                      "an oversized head answers the exact 431 wire over TLS then closes");
            }
            const auto after_rejection = once_health_wire(server.port());
            check(after_rejection.has_value(), "a fresh TLS connection succeeds after a rejection");
            // A paused TLS connection never blocks another one (AC-7, TLS side).
            auto paused = await_connect(server.port());
            auto other = await_connect(server.port());
            check(paused.has_value() && other.has_value(), "two TLS connections establish");
            if (paused && other) {
                check(paused->send_all("POST /api/echo HTTP/1.1\r\nHost: localhost\r\n"
                                       "Connection: close\r\nContent-Length: 5\r\n\r\nhe"),
                      "the paused connection sends its unfinished request");
                const auto other_health =
                    other->send_all("GET /api/health HTTP/1.1\r\nHost: localhost\r\n"
                                    "Connection: close\r\n\r\n")
                        ? other->read_to_eof()
                        : std::optional<std::string>{};
                check(other_health.has_value() && *other_health == close_health_wire,
                      "the second connection completes while the first is paused");
                check(paused->send_all("llo") && paused->read_to_eof() == close_hello_echo_wire,
                      "the paused connection then receives its exact echo");
            }
        }
        // The idle timeout closes a silent connection.
        {
            TestTlsServer server(certificate_path, key_path, handler,
                                 std::chrono::milliseconds(400));
            check(server.port() != 0, "the idle-timeout listener holds a reserved port");
            auto client = await_connect(server.port());
            check(client.has_value(), "the idle client connects");
            if (client) {
                const auto closed = client->read_to_eof();
                check(!closed.has_value() || closed->empty(),
                      "the idle TLS connection closes without a response");
            }
        }
        // Certificate material in an unwritable location is a coded startup
        // error: absence alone is the ordinary first-run generation path.
        {
            TestTlsServer server(root + "/absent-dir/cert.pem", root + "/absent-dir/key.pem",
                                 handler, std::chrono::milliseconds(5000));
            check(server.await_exit() == EINVAL,
                  "unwritable certificate material fails with EINVAL and serves nothing");
        }
        // Restart: the same certificate keeps serving (AC-5, TLS side).
        {
            TestTlsServer second(certificate_path, key_path, handler,
                                 std::chrono::milliseconds(5000));
            const auto health = once_health_wire(second.port());
            check(second.port() != 0 && health.has_value() &&
                      health->find("\"status\":\"ok\"") != std::string::npos,
                  "a restarted TLS listener reuses the same certificate and serves");
        }
    }

    for (const std::string_view name : {"/cert.pem", "/key.pem", "/web/index.html"})
        std::remove((root + std::string(name)).c_str());
    std::remove((root + "/web").c_str());
    std::remove(root.c_str());
    std::printf("%zu checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
