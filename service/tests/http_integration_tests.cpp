// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Pins the T-2 both-listener assembly at the process boundary: the orange
// service executable binds the documented fixed ports 127.0.0.1:8080 and
// 127.0.0.1:8443, serves the declared routes and static content on both
// listeners with exact wires, answers curl and curl -k end-to-end (AC-2),
// keeps a paused plaintext connection from blocking another one (AC-7),
// exits 0 on SIGTERM, and reports a coded startup error on a port conflict.
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <optional>
#include <poll.h>
#include <signal.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
constexpr std::string_view service_path = ORANGE_SERVICE_PATH;
constexpr std::uint16_t plain_port = 8080;
constexpr std::uint16_t tls_port = 8443;
constexpr const char *curl_path = "/usr/bin/curl";

int failures = 0;
std::size_t checks = 0;
// Gives CTest a nonzero exit for any observable assembly mismatch.
void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}

// One forked child with optional captured output; the pipe is read after the
// child exits, so writers cannot block on a full pipe for these workloads.
struct Child {
    pid_t pid = -1;
    int output_fd = -1;
};
std::optional<Child> spawn(const char *binary, const std::vector<std::string> &arguments) {
    int created[2];
    if (::pipe(created) != 0)
        return std::nullopt;
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(created[0]);
        ::close(created[1]);
        return std::nullopt;
    }
    if (pid == 0) {
        ::close(created[0]);
        ::dup2(created[1], STDOUT_FILENO);
        ::dup2(created[1], STDERR_FILENO);
        ::close(created[1]);
        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(binary));
        for (const std::string &argument : arguments)
            argv.push_back(const_cast<char *>(argument.c_str()));
        argv.push_back(nullptr);
        ::execv(binary, argv.data());
        _exit(127);
    }
    ::close(created[1]);
    return Child{pid, created[0]};
}

// Waits for the child to exit within the deadline; nullopt on timeout.
std::optional<int> await_exit(pid_t pid, std::chrono::milliseconds deadline) {
    const auto limit = std::chrono::steady_clock::now() + deadline;
    for (;;) {
        int status = 0;
        const pid_t done = ::waitpid(pid, &status, WNOHANG);
        if (done == pid)
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (std::chrono::steady_clock::now() >= limit)
            return std::nullopt;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Drains the child's captured pipe after it exited.
std::string read_pipe(int fd) {
    std::string captured;
    char buffer[4096];
    for (;;) {
        const ssize_t got = ::read(fd, buffer, sizeof(buffer));
        if (got <= 0)
            break;
        captured.append(buffer, static_cast<std::size_t>(got));
    }
    ::close(fd);
    return captured;
}

// Runs curl once and returns {exit code, combined output}; nullopt on spawn
// failure or timeout.
struct CurlResult {
    int code = -1;
    std::string output;
};
std::optional<CurlResult> run_curl(const std::vector<std::string> &arguments) {
    auto child = spawn(curl_path, arguments);
    if (!child)
        return std::nullopt;
    const auto code = await_exit(child->pid, std::chrono::milliseconds(10000));
    if (!code.has_value()) {
        ::kill(child->pid, SIGKILL);
        ::waitpid(child->pid, nullptr, 0);
        read_pipe(child->output_fd);
        return std::nullopt;
    }
    return CurlResult{*code, read_pipe(child->output_fd)};
}

// One blocking plaintext client with a receive deadline.
class PlainClient {
  public:
    ~PlainClient() {
        if (fd_ >= 0)
            ::close(fd_);
    }
    PlainClient(const PlainClient &) = delete;
    PlainClient &operator=(const PlainClient &) = delete;
    // An empty value precedes the descriptor transfer below.
    PlainClient() = default;
    // Transfers the descriptor so the value can ride std::optional.
    PlainClient(PlainClient &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    static std::optional<PlainClient> connect(std::uint16_t port) {
        PlainClient client;
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
        return client;
    }
    [[nodiscard]] bool send_all(std::string_view bytes) {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const ssize_t wrote = ::send(fd_, bytes.data() + offset, bytes.size() - offset, 0);
            if (wrote <= 0)
                return false;
            offset += static_cast<std::size_t>(wrote);
        }
        return true;
    }
    [[nodiscard]] std::optional<std::string> read_exact(std::size_t count) {
        std::string received;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (received.size() < count) {
            pollfd waited{fd_, POLLIN, 0};
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       deadline - std::chrono::steady_clock::now())
                                       .count();
            if (remaining <= 0 || ::poll(&waited, 1, static_cast<int>(remaining)) <= 0)
                return std::nullopt;
            char buffer[8192];
            const ssize_t got = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (got <= 0)
                return std::nullopt;
            received.append(buffer, static_cast<std::size_t>(got));
        }
        return received;
    }

  private:
    int fd_ = -1;
};

// One blocking TLS client with a receive deadline (the curl -k profile).
class TlsClient {
  public:
    ~TlsClient() {
        if (ssl_ != nullptr)
            SSL_free(ssl_);
        if (fd_ >= 0)
            ::close(fd_);
        if (ctx_ != nullptr)
            SSL_CTX_free(ctx_);
    }
    TlsClient(const TlsClient &) = delete;
    TlsClient &operator=(const TlsClient &) = delete;
    // An empty value precedes the descriptor transfer below.
    TlsClient() = default;
    // Transfers the descriptors so the value can ride std::optional.
    TlsClient(TlsClient &&other) noexcept : fd_(other.fd_), ssl_(other.ssl_), ctx_(other.ctx_) {
        other.fd_ = -1;
        other.ssl_ = nullptr;
        other.ctx_ = nullptr;
    }
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
    [[nodiscard]] bool send_all(std::string_view bytes) {
        while (!bytes.empty()) {
            const int wrote = SSL_write(ssl_, bytes.data(), static_cast<int>(bytes.size()));
            if (wrote <= 0)
                return false;
            bytes.remove_prefix(static_cast<std::size_t>(wrote));
        }
        return true;
    }
    [[nodiscard]] std::optional<std::string> read_exact(std::size_t count) {
        std::string received;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (received.size() < count) {
            pollfd waited{fd_, POLLIN, 0};
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       deadline - std::chrono::steady_clock::now())
                                       .count();
            if (remaining <= 0 || ::poll(&waited, 1, static_cast<int>(remaining)) <= 0)
                return std::nullopt;
            char buffer[8192];
            const int got = SSL_read(ssl_, buffer, sizeof(buffer));
            if (got <= 0)
                return std::nullopt;
            received.append(buffer, static_cast<std::size_t>(got));
        }
        return received;
    }

  private:
    int fd_ = -1;
    SSL *ssl_ = nullptr;
    SSL_CTX *ctx_ = nullptr;
};

// Retries one plain connection until the port answers, proving readiness.
bool await_plain_ready(std::uint16_t port) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        if (PlainClient::connect(port))
            return true;
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

// Retries one full TLS handshake until the port answers, proving readiness.
bool await_tls_ready(std::uint16_t port) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        if (TlsClient::connect(port))
            return true;
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

// The exact keep-alive health wire shared by both listeners.
constexpr std::string_view keep_alive_health_wire = "HTTP/1.1 200 OK\r\n"
                                                    "Content-Type: application/json\r\n"
                                                    "Content-Length: 15\r\n"
                                                    "\r\n"
                                                    "{\"status\":\"ok\"}";

// One exchange helper per listener kind: send bytes, read exact count.
template <typename Client>
bool exchange(Client &client, std::string_view request, std::string_view expected) {
    return client.send_all(request) &&
           client.read_exact(expected.size()) == std::optional<std::string>(expected);
}

// AC-2's protocol clauses over one listener through one client connection.
template <typename Client> void check_declared_behaviour(const char *listener, std::uint16_t port) {
    if (auto client = Client::connect(port)) {
        check(exchange(*client, "GET /api/health HTTP/1.1\r\nHost: localhost\r\n\r\n",
                       keep_alive_health_wire),
              "health answers its exact keep-alive wire over the listener");
        check(exchange(*client, "POST /api/echo HTTP/1.1\r\nHost: localhost\r\n\r\n",
                       "HTTP/1.1 200 OK\r\n"
                       "Transfer-Encoding: chunked\r\n"
                       "\r\n"
                       "0\r\n\r\n"),
              "echo answers an empty chunked body over the listener");
        check(exchange(*client,
                       "GET /index.html HTTP/1.1\r\nHost: localhost\r\n"
                       "Range: bytes=0-3\r\n\r\n",
                       "HTTP/1.1 206 Partial Content\r\n"
                       "Content-Type: text/html\r\n"
                       "Content-Range: bytes 0-3/19\r\n"
                       "Content-Length: 4\r\n"
                       "\r\n"
                       "<htm"),
              "a single range answers its exact 206 wire over the listener");
        check(exchange(*client,
                       "GET /index.html HTTP/1.1\r\nHost: localhost\r\n"
                       "Range: bytes=500-600\r\n\r\n",
                       "HTTP/1.1 416 Range Not Satisfiable\r\n"
                       "Content-Range: bytes */19\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n"),
              "an unsatisfiable range answers its exact 416 wire over the listener");
        check(exchange(*client, "POST /api/health HTTP/1.1\r\nHost: localhost\r\n\r\n",
                       "HTTP/1.1 405 Method Not Allowed\r\n"
                       "Allow: GET\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n"),
              "a wrong method answers 405 with Allow over the listener");
        check(exchange(*client, "GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n",
                       "HTTP/1.1 404 Not Found\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n"),
              "an unknown target answers 404 over the listener");
        static_cast<void>(listener);
    } else {
        check(false, "the listener accepts a protocol client");
    }
}

} // namespace

// Executes the assembly, curl, AC-7, shutdown and conflict checks.
int main() {
    // Fixture: a one-file static root and certificate material in a temp dir.
    std::vector<char> pattern;
    const std::string_view base = "/tmp/orange-integration-XXXXXX";
    pattern.assign(base.begin(), base.end());
    pattern.push_back('\0');
    const bool have_dir = mkdtemp(pattern.data()) != nullptr;
    check(have_dir, "the fixture directory is created");
    if (!have_dir) {
        std::printf("%zu checks, %d failures\n", checks, failures);
        return failures ? 1 : 0;
    }
    const std::string root = pattern.data();
    const std::string web_root = root + "/web";
    check(::mkdir(web_root.c_str(), 0755) == 0, "the web fixture directory is created");
    FILE *page = std::fopen((web_root + "/index.html").c_str(), "wb");
    check(page != nullptr, "the index fixture is written");
    if (page != nullptr) {
        std::fputs("<html>orange</html>", page);
        std::fclose(page);
    }

    auto service =
        spawn(service_path.data(), {"--root", web_root, "--cert", root + "/cert.pem", "--key",
                                    root + "/key.pem", "--port", "8080", "--tls-port", "8443"});
    check(service.has_value(), "the service process spawns");
    if (!service) {
        std::printf("%zu checks, %d failures\n", checks, failures);
        return 1;
    }
    check(await_plain_ready(plain_port), "the plaintext fixed port becomes ready");
    check(await_tls_ready(tls_port), "the TLS fixed port completes handshakes");

    // AC-2 protocol clauses on both listeners.
    check_declared_behaviour<PlainClient>("plaintext", plain_port);
    check_declared_behaviour<TlsClient>("tls", tls_port);

    // AC-2's curl clauses against the documented fixed origins.
    const auto health_plain = run_curl({"-fsS", "http://127.0.0.1:8080/api/health"});
    check(health_plain && health_plain->code == 0 && health_plain->output == "{\"status\":\"ok\"}",
          "curl -fsS on the plaintext origin returns the exact health body");
    const auto health_tls = run_curl({"-kfsS", "https://127.0.0.1:8443/api/health"});
    check(health_tls && health_tls->code == 0 && health_tls->output == "{\"status\":\"ok\"}",
          "curl -kfsS on the https origin returns the exact health body");
    const auto echo_plain = run_curl(
        {"-fsS", "-X", "POST", "--data-binary", "hello", "http://127.0.0.1:8080/api/echo"});
    check(echo_plain && echo_plain->code == 0 && echo_plain->output == "hello",
          "curl posts an echo body through the plaintext origin");
    const auto echo_tls = run_curl(
        {"-kfsS", "-X", "POST", "--data-binary", "hello", "https://127.0.0.1:8443/api/echo"});
    check(echo_tls && echo_tls->code == 0 && echo_tls->output == "hello",
          "curl posts an echo body through the https origin");
    const auto page_curl =
        run_curl({"-fsS", "-r", "0-3", "-w", "|%{http_code}", "http://127.0.0.1:8080/index.html"});
    check(page_curl && page_curl->code == 0 && page_curl->output == "<htm|206",
          "curl -r 0-3 receives the exact 206 slice with its status");
    const auto unsatisfiable = run_curl({"-s", "-o", "/dev/null", "-w", "%{http_code}", "-r",
                                         "500-600", "http://127.0.0.1:8080/index.html"});
    check(unsatisfiable && unsatisfiable->code == 0 && unsatisfiable->output == "416",
          "curl receives 416 for an unsatisfiable range");
    const auto root_curl = run_curl({"-kfsS", "https://127.0.0.1:8443/"});
    check(root_curl && root_curl->code == 0 && root_curl->output == "<html>orange</html>",
          "curl -k on the https root receives the index page");
    const auto method = run_curl({"-s", "-o", "/dev/null", "-w", "%{http_code}", "-X", "POST",
                                  "http://127.0.0.1:8080/api/health"});
    check(method && method->code == 0 && method->output == "405",
          "curl observes 405 for POST on the health route");
    const auto missing =
        run_curl({"-s", "-o", "/dev/null", "-w", "%{http_code}", "http://127.0.0.1:8080/missing"});
    check(missing && missing->code == 0 && missing->output == "404",
          "curl observes 404 for an unknown target");

    // AC-7's plaintext half: a paused echo connection blocks nothing.
    if (auto paused = PlainClient::connect(plain_port)) {
        if (auto other = PlainClient::connect(plain_port)) {
            check(paused->send_all("POST /api/echo HTTP/1.1\r\nHost: localhost\r\n"
                                   "Content-Length: 5\r\n\r\nhe"),
                  "the paused plaintext connection sends its unfinished request");
            check(exchange(*other, "GET /api/health HTTP/1.1\r\nHost: localhost\r\n\r\n",
                           keep_alive_health_wire),
                  "the second plaintext connection completes while one is paused");
            constexpr std::string_view exact_echo_wire = "HTTP/1.1 200 OK\r\n"
                                                         "Transfer-Encoding: chunked\r\n"
                                                         "\r\n"
                                                         "5\r\nhello\r\n"
                                                         "0\r\n\r\n";
            check(paused->send_all("llo") && paused->read_exact(exact_echo_wire.size()) ==
                                                 std::optional<std::string>(exact_echo_wire),
                  "the paused connection then receives its exact echo");
        } else {
            check(false, "the second plaintext connection establishes");
        }
    } else {
        check(false, "the paused plaintext connection establishes");
    }

    // A port conflict is a coded startup failure, and the original survives.
    auto conflicted =
        spawn(service_path.data(), {"--root", web_root, "--cert", root + "/cert.pem", "--key",
                                    root + "/key.pem", "--port", "8080", "--tls-port", "8443"});
    check(conflicted.has_value(), "the conflicting process spawns");
    if (conflicted) {
        const auto code = await_exit(conflicted->pid, std::chrono::milliseconds(5000));
        const std::string output = read_pipe(conflicted->output_fd);
        check(code.has_value() && *code != 0,
              "a second instance on the busy fixed ports exits nonzero");
        check(output.find("orange:") == 0, "the conflict diagnostic is a coded orange: message");
    }
    const auto still_alive = run_curl({"-fsS", "http://127.0.0.1:8080/api/health"});
    check(still_alive && still_alive->code == 0,
          "the original service keeps serving after the conflict refusal");

    // SIGTERM is a clean shutdown.
    ::kill(service->pid, SIGTERM);
    const auto exited = await_exit(service->pid, std::chrono::milliseconds(5000));
    if (!exited.has_value()) {
        ::kill(service->pid, SIGKILL);
        ::waitpid(service->pid, nullptr, 0);
    }
    check(exited.has_value() && *exited == 0, "SIGTERM stops the service with exit 0");
    read_pipe(service->output_fd);

    std::remove((web_root + "/index.html").c_str());
    std::remove((root + "/cert.pem").c_str());
    std::remove((root + "/key.pem").c_str());
    std::remove(web_root.c_str());
    std::remove(root.c_str());
    std::printf("%zu checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
