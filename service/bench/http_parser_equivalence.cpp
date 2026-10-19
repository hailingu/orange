// ADR: docs/adr/ADR-0008-http-parser-benchmarks.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Authorization: docs/reviews/2026-10-16-owned-request-storage.md.
// Emits deterministic semantic results to compare two parser revisions bytewise.
#include "http_parser_workloads.hpp"
#include <cstdio>
#include <exception>
#include <random>
#include <type_traits>
#include <utility>

namespace {
std::size_t cases = 0;
// Fixed-width lengths frame binary fields unambiguously, including embedded NUL.
void number(std::uint64_t value) {
    for (int i = 0; i < 8; ++i)
        std::putchar(static_cast<unsigned char>(value >> (i * 8)));
}
// Emits every byte, rather than sampling field endpoints or relying on a hash.
void bytes(std::string_view value) {
    number(value.size());
    std::fwrite(value.data(), 1, value.size(), stdout);
}
// Both APIs expose the same grammar; this adapter adds no production dependency.
template <class T> decltype(auto) fields(const T &request) {
    if constexpr (requires { request.headers(); })
        return request.headers();
    else
        return (request.headers);
}
// Includes tokens, ordered duplicates, case and all trimmed field octets.
template <class T> void request(const T &value) {
    if constexpr (requires { value.method(); }) {
        bytes(value.method());
        bytes(value.target());
        bytes(value.version());
    } else {
        bytes(value.method);
        bytes(value.target);
        bytes(value.version);
    }
    number(fields(value).size());
    for (const auto &field : fields(value)) {
        bytes(field.name);
        bytes(field.value);
    }
}
// A tag separates errors, partial decoder state and complete owned messages.
template <class T> void outcome(const T &value) {
    std::visit(
        [](const auto &item) {
            if constexpr (requires { item.request; }) {
                number(0);
                number(item.consumed);
                request(item.request);
                if constexpr (requires { item.body; })
                    bytes(item.body);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(item)>,
                                                orange::http::ParseError>) {
                number(1);
                number(static_cast<unsigned>(item));
            } else {
                number(2);
            }
        },
        value);
}
// Compares stateless and incremental paths, including the first feed's progress.
void observe(const std::string &wire, std::size_t split) {
    ++cases;
    outcome(orange::http::parse_request(wire));
    outcome(orange::http::parse_message(wire));
    orange::http::RequestDecoder decoder;
    const auto first = decoder.feed(std::string_view(wire).substr(0, split));
    number(first.consumed);
    outcome(first.outcome);
    if (std::holds_alternative<std::monostate>(first.outcome)) {
        const auto rest = decoder.feed(std::string_view(wire).substr(split));
        number(rest.consumed);
        outcome(rest.outcome);
    }
}
// Exhausts prefixes of capped fixtures and samples input mutations deterministically.
void corpus() {
    const auto fixtures = orange::bench::fixtures();
    std::vector<std::string> seeds;
    for (const auto &fixture : fixtures) {
        observe(fixture.wire, fixture.wire.size() / 2);
        if (fixture.wire.size() <= orange::http::header_block_cap)
            seeds.push_back(fixture.wire);
    }
    std::string leading_lines;
    for (int i = 0; i < 2000; ++i)
        leading_lines += "\r\n";
    seeds.push_back(leading_lines + "GET / HTTP/1.1\r\n\r\n");
    seeds.push_back(std::string(4000, ' ') + "GET / HTTP/1.1\r\n\r\n");
    for (const auto &wire : seeds)
        for (std::size_t size = 0; size <= wire.size(); ++size)
            observe(wire.substr(0, size), size / 2);
    std::mt19937 random(0x53544F52);
    for (int i = 0; i < 20000; ++i) {
        auto wire = seeds[random() % seeds.size()];
        if (!wire.empty())
            wire[random() % wire.size()] = static_cast<char>(random() & 255);
        if (i % 3 == 0)
            wire.resize(random() % (wire.size() + 1));
        if (i % 5 == 0)
            wire += "NEXT\r\n";
        observe(wire, random() % (wire.size() + 1));
    }
    for (int i = 0; i < 10000; ++i) {
        std::string wire(random() % 513, '\0');
        for (char &byte : wire)
            byte = static_cast<char>(random() & 255);
        observe(wire, random() % (wire.size() + 1));
    }
}
} // namespace
// Compare stdout from identical drivers against two libraries; stderr is metadata.
int main() {
    try {
        corpus();
        std::fprintf(stderr, "semantic cases: %zu; seed: 0x53544F52\n", cases);
        return std::ferror(stdout) ? 1 : 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "equivalence execution failed: %s\n", error.what());
        return 1;
    }
}
