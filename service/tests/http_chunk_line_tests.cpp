// ADR: service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md
// Authorization: docs/reviews/2026-10-16-http-performance.md.
// Compares span advancement with the established octet grammar and exact boundary.
#include "http_chunk_line.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {
using orange::http::detail::ChunkLine;
using orange::http::detail::ChunkLineResult;
int failures = 0;
std::size_t checks = 0;
// Gives CTest a nonzero exit for any observable grammar or cursor mismatch.
void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}
// Stopping at CRLF, a bad octet, or quota rejection must leave the same suffix.
void compare(std::string_view input, std::size_t quota, std::size_t fragment) {
    ChunkLine scalar;
    ChunkLine bulk;
    scalar.reset(quota);
    bulk.reset(quota);
    std::size_t offset = 0;
    while (offset < input.size()) {
        const auto window = input.substr(offset, fragment);
        auto expected = ChunkLineResult::pending;
        std::size_t count = 0;
        while (count < window.size() && expected == ChunkLineResult::pending) {
            expected = scalar.feed(window[count++]);
        }
        const auto actual = bulk.feed(window);
        check(actual.consumed == count && actual.result == expected && bulk.size() == scalar.size(),
              "span preserves byte grammar, size and stop boundary");
        offset += count;
        if (expected != ChunkLineResult::pending)
            break;
    }
}
// Covers short syntax splits, long runs, quoted escapes, and numeric overflow.
void check_lines() {
    const std::vector<std::string> lines = {"0\r\nNEXT",
                                            "Af\r\nNEXT",
                                            "000\r\n",
                                            "100000\r\n",
                                            "100001\r\n",
                                            "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFx",
                                            "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n",
                                            "1 \t; foo \t= \t\"a\\\"b\";bar=token\r\nNEXT",
                                            "0;foo \r\n",
                                            "0;foo=x \r\n",
                                            "0;foo=\"x\" \r\n",
                                            "0;\r\n",
                                            "0;x=\r\n",
                                            "0\n",
                                            "0;x=" + std::string(7000, 'x') + "\r\nNEXT",
                                            "0;x=\"" + std::string(7000, 'x') +
                                                "\\\"end\"\r\nNEXT"};
    for (const auto &line : lines) {
        for (const std::size_t quota : {std::size_t{0}, std::size_t{175}, std::size_t{1048576}}) {
            for (const auto fragment :
                 {std::size_t{1}, std::size_t{3}, std::size_t{17}, line.size()})
                compare(line, quota, fragment);
        }
    }
    ChunkLine line;
    line.reset(16);
    const auto step = line.feed(std::string_view("10\r\nNEXT"));
    check(step.result == ChunkLineResult::complete && step.consumed == 4 && line.size() == 16,
          "literal hexadecimal result stops before pipeline suffix");
    line.reset(15);
    check(line.feed(std::string_view("10\r\n")).result == ChunkLineResult::too_large,
          "literal quota rejects before content");
}
// Each extension state sees all octets; batching must not skip invalid controls.
void check_octets() {
    for (unsigned int byte = 0; byte < 256; ++byte) {
        for (const auto prefix : {"0", "0;", "0;x", "0;x=", "0;x=\"", "0;x=\"a\\"}) {
            const auto line = std::string(prefix) + static_cast<char>(byte) + "x\"\r\nNEXT";
            for (std::size_t split = 1; split <= line.size(); ++split)
                compare(line, 1048576, split);
        }
    }
}
} // namespace
// Executes grammar equivalence and independent hexadecimal/quota checks.
int main() {
    check_lines();
    check_octets();
    std::printf("%zu checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
