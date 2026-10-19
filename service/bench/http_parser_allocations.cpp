// ADR: docs/adr/ADR-0008-http-parser-benchmarks.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Authorization: docs/reviews/2026-10-16-owned-request-storage.md.
// Counts ordinary allocation calls/bytes separately from latency measurements.
#include "http_parser_workloads.hpp"
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <new>

namespace {
bool counting = false;
std::size_t calls = 0;
std::size_t bytes = 0;
} // namespace
// Real allocations are counted only around a workload, never fixture creation.
void *operator new(std::size_t size) {
    if (void *memory = std::malloc(size == 0 ? 1 : size)) {
        if (counting) {
            ++calls;
            bytes += size;
        }
        return memory;
    }
    throw std::bad_alloc{};
}
// Arrays are included in the same ordinary-allocation metric.
void *operator new[](std::size_t size) { return ::operator new(size); }
// Uses the allocator paired with the counting replacement.
void operator delete(void *memory) noexcept { std::free(memory); }
// Matches ordinary array allocations.
void operator delete[](void *memory) noexcept { std::free(memory); }

// Reports allocation counts, requested bytes and result checksums without timing.
int main() {
    try {
        const auto cases = orange::bench::fixtures();
        std::cout << "{\"metric\":\"ordinary operator new calls and requested bytes\","
                     "\"request_size\":"
                  << sizeof(orange::http::Request) << ",\"cases\":[";
        for (std::size_t i = 0; i < cases.size(); ++i) {
            calls = bytes = 0;
            counting = true;
            const auto checksum = orange::bench::run(cases[i]);
            counting = false;
            if (checksum != orange::bench::expected_result(cases[i])) {
                std::fprintf(stderr, "invalid allocation fixture: %s\n", cases[i].name.c_str());
                return 1;
            }
            if (i != 0)
                std::cout << ',';
            std::cout << "{\"name\":" << std::quoted(cases[i].name) << ",\"calls\":" << calls
                      << ",\"requested_bytes\":" << bytes << ",\"checksum\":" << checksum << '}';
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception &error) {
        counting = false;
        std::fprintf(stderr, "allocation measurement failed: %s\n", error.what());
        return 1;
    }
}
