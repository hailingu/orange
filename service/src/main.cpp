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
#include "http_server.hpp"
#include "routes.hpp"
#include "static_files.hpp"
#include "tls.hpp"
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>

namespace {
constexpr std::string_view usage =
    "usage: orange_service [--root DIR] [--cert PATH] [--key PATH]\n"
    "                      [--port N] [--tls-port N] [--acceptors N] [--idle-ms MS]\n";

constexpr std::uint16_t default_plain_port = 8080;
constexpr std::uint16_t default_tls_port = 8443;
constexpr int default_acceptors = 1;
constexpr long default_idle_ms = 5000;
constexpr std::string_view default_root = "web/dist";
constexpr std::string_view default_certificate = "run/cert.pem";
constexpr std::string_view default_key = "run/key.pem";

// The signal bridge: the handler only stores to an atomic flag, which every
// practical runtime accepts inside a handler.
std::atomic_bool *stop_target = nullptr;
void on_signal(int) {
    if (stop_target != nullptr)
        stop_target->store(true);
}

struct Options {
    std::string root{default_root};
    std::string certificate{default_certificate};
    std::string key{default_key};
    std::uint16_t plain_port = default_plain_port;
    std::uint16_t tls_port = default_tls_port;
    int acceptors = default_acceptors;
    std::chrono::milliseconds idle{default_idle_ms};
};

// One strict positive-integer parse of at most 9 digits.
std::optional<long> parse_positive(std::string_view digits) {
    if (digits.empty() || digits.size() > 9)
        return std::nullopt;
    long value = 0;
    for (const char digit : digits) {
        if (digit < '0' || digit > '9')
            return std::nullopt;
    }
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size())
        return std::nullopt;
    return value;
}

// Parses the documented flags; nullopt prints the complaint and returns 2.
std::optional<Options> parse_options(int argc, char *argv[], int &exit_code) {
    Options options;
    for (int at = 1; at < argc; ++at) {
        const std::string_view flag = argv[at];
        const auto value_string = [&]() -> std::optional<std::string_view> {
            if (at + 1 >= argc) {
                std::fprintf(stderr, "orange: %s needs a value\n%s", std::string(flag).c_str(),
                             usage.data());
                return std::nullopt;
            }
            return std::string_view(argv[++at]);
        };
        if (flag == "--help") {
            std::fputs(usage.data(), stdout);
            exit_code = 0;
            return std::nullopt;
        }
        if (flag == "--root") {
            if (const auto value = value_string())
                options.root = std::string(*value);
            else {
                exit_code = 2;
                return std::nullopt;
            }
        } else if (flag == "--cert") {
            if (const auto value = value_string())
                options.certificate = std::string(*value);
            else {
                exit_code = 2;
                return std::nullopt;
            }
        } else if (flag == "--key") {
            if (const auto value = value_string())
                options.key = std::string(*value);
            else {
                exit_code = 2;
                return std::nullopt;
            }
        } else if (flag == "--port") {
            const auto value = value_string();
            const auto number = value ? parse_positive(*value) : std::nullopt;
            if (!number || *number < 1 || *number > 65535) {
                std::fprintf(stderr, "orange: --port needs 1..65535\n%s", usage.data());
                exit_code = 2;
                return std::nullopt;
            }
            options.plain_port = static_cast<std::uint16_t>(*number);
        } else if (flag == "--tls-port") {
            const auto value = value_string();
            const auto number = value ? parse_positive(*value) : std::nullopt;
            if (!number || *number < 1 || *number > 65535) {
                std::fprintf(stderr, "orange: --tls-port needs 1..65535\n%s", usage.data());
                exit_code = 2;
                return std::nullopt;
            }
            options.tls_port = static_cast<std::uint16_t>(*number);
        } else if (flag == "--acceptors") {
            const auto value = value_string();
            const auto number = value ? parse_positive(*value) : std::nullopt;
            if (!number || *number < 1 || *number > 64) {
                std::fprintf(stderr, "orange: --acceptors needs 1..64\n%s", usage.data());
                exit_code = 2;
                return std::nullopt;
            }
            options.acceptors = static_cast<int>(*number);
        } else if (flag == "--idle-ms") {
            const auto value = value_string();
            const auto number = value ? parse_positive(*value) : std::nullopt;
            if (!number) {
                std::fprintf(stderr, "orange: --idle-ms needs a positive count\n%s", usage.data());
                exit_code = 2;
                return std::nullopt;
            }
            options.idle = std::chrono::milliseconds(*number);
        } else {
            std::fprintf(stderr, "orange: unknown flag %s\n%s", std::string(flag).c_str(),
                         usage.data());
            exit_code = 2;
            return std::nullopt;
        }
    }
    return options;
}

// Creates the parent directory of one path when it is missing; the default
// run/ location for the certificate material.
void ensure_parent_directory(const std::string &path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0)
        return;
    const std::string parent = path.substr(0, slash);
    if (::mkdir(parent.c_str(), 0755) != 0 && errno != EEXIST)
        std::fprintf(stderr, "orange: warning: creating %s failed (errno %d)\n", parent.c_str(),
                     errno);
}

} // namespace

int main(int argc, char *argv[]) {
    int exit_code = 0;
    const auto options = parse_options(argc, argv, exit_code);
    if (!options)
        return exit_code;

    const auto loaded = orange::http::StaticFileStore::load(options->root);
    if (const auto *failure = std::get_if<orange::http::StaticLoadError>(&loaded)) {
        std::fprintf(stderr, "orange: static root '%s' failed: %s (errno %d)\n",
                     options->root.c_str(), failure->path.c_str(), failure->error);
        return 1;
    }
    const auto &store = std::get<orange::http::StaticFileStore>(loaded);
    ensure_parent_directory(options->certificate);
    ensure_parent_directory(options->key);
    const auto material =
        orange::http::ensure_self_signed_certificate(options->certificate, options->key);
    if (!material.ok) {
        std::fprintf(stderr, "orange: certificate material '%s'/'%s' failed: %s\n",
                     options->certificate.c_str(), options->key.c_str(),
                     material.diagnostic.c_str());
        return 1;
    }

    const orange::http::RequestHandler handler =
        [&store](const orange::http::ParsedMessage &message) {
            return orange::http::handle_request(message, &store);
        };

    orange::http::LoopbackServer plain(options->acceptors);
    const int bind_failure = plain.bind(options->plain_port);
    if (bind_failure != 0) {
        std::fprintf(stderr, "orange: bind 127.0.0.1:%u failed: %s (errno %d)\n",
                     options->plain_port, std::strerror(bind_failure), bind_failure);
        return 1;
    }

    std::atomic_bool stop{false};
    int tls_exit = 0;
    std::atomic_bool tls_done{false};
    std::thread tls_thread([&]() {
        tls_exit = orange::http::serve_tls(options->tls_port, options->certificate, options->key,
                                           handler, options->idle, stop, nullptr);
        tls_done.store(true);
    });
    // A bind failure on the https listener surfaces as serve_tls's early
    // coded return; watch briefly so startup errors are reported before the
    // plaintext loop takes over the calling thread.
    const auto watch = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
    while (!tls_done.load() && std::chrono::steady_clock::now() < watch)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (tls_done.load() && tls_exit != 0) {
        stop.store(true);
        tls_thread.join();
        std::fprintf(stderr, "orange: bind 127.0.0.1:%u failed: %s (errno %d)\n", options->tls_port,
                     std::strerror(tls_exit), tls_exit);
        return 1;
    }

    struct sigaction action{};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    stop_target = &stop;
    if (::sigaction(SIGINT, &action, nullptr) != 0 || ::sigaction(SIGTERM, &action, nullptr) != 0) {
        stop.store(true);
        tls_thread.join();
        std::fprintf(stderr, "orange: installing signal handlers failed (errno %d)\n", errno);
        return 1;
    }
    std::fprintf(stdout, "orange serving https://127.0.0.1:%u and http://127.0.0.1:%u from %s\n",
                 options->tls_port, options->plain_port, options->root.c_str());

    const int plain_exit = plain.serve(handler, options->idle, stop);
    stop.store(true);
    tls_thread.join();
    return plain_exit == 0 && tls_exit == 0 ? 0 : 1;
}
