// Governed by docs/adr/ADR-0003-local-runtime-path.md.

#include "http_messages.hpp"

#include <string>
#include <string_view>

namespace orange::http {

// Parses the CRLF-terminated request line `method SP request-target SP
// HTTP-version` into owned Request fields. Three non-empty tokens are
// required: an empty token, a missing or extra token, or a missing CRLF
// means the bytes cannot be a request line and yield std::nullopt. Bytes
// after the request line are not consumed yet — header and body parsing
// are later tasks of the T-1 ladder.
std::optional<Request> parse_request(std::string_view raw) {
    if (raw.empty()) {
        return std::nullopt;
    }
    const std::size_t line_end = raw.find("\r\n");
    if (line_end == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view line = raw.substr(0, line_end);
    const std::size_t first_space = line.find(' ');
    if (first_space == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t second_space = line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view method = line.substr(0, first_space);
    const std::string_view target = line.substr(first_space + 1, second_space - first_space - 1);
    const std::string_view version = line.substr(second_space + 1);
    if (method.empty() || target.empty() || version.empty() ||
        version.find(' ') != std::string_view::npos) {
        return std::nullopt;
    }
    return Request{std::string(method), std::string(target), std::string(version)};
}

} // namespace orange::http
