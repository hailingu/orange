// ADR: docs/adr/ADR-0008-http-parser-benchmarks.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Authorization: docs/reviews/2026-10-16-http-performance.md.
// Allocation optimization: docs/reviews/2026-10-16-owned-request-storage.md.
// Reproducible parser-only hot-cache timings; fixtures are built outside timing.
#include "http_parser_workloads.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace orange::bench;
struct Timing {
    std::size_t iterations;
    std::vector<double> samples;
};
// Batches timer reads and emits the checksum after all measurements, preventing DCE.
double batch(const Case &fixture, std::size_t iterations, std::uint64_t &checksum) {
    const auto begin = Clock::now();
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < iterations; ++i) {
        sum += run(fixture);
    }
    const auto elapsed = Clock::now() - begin;
    checksum += sum;
    return std::chrono::duration<double, std::nano>(elapsed).count();
}
// Calibration warms the path and keeps each sample long relative to timer cost.
Timing measure(const Case &fixture, bool quick, std::uint64_t &checksum) {
    std::size_t iterations = 1;
    const double minimum_ns = quick ? 1000000 : 5000000;
    while (batch(fixture, iterations, checksum) < minimum_ns && iterations < (1U << 26)) {
        iterations *= 2;
    }
    Timing timing{iterations, {}};
    for (int sample = 0; sample < (quick ? 3 : 11); ++sample) {
        timing.samples.push_back(batch(fixture, iterations, checksum) / iterations);
    }
    std::sort(timing.samples.begin(), timing.samples.end());
    return timing;
}
// Positive finite scales deliberately select a magnitude gate; default is report-only.
bool arguments(int argc, char **argv, bool &quick, double &scale) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--quick") {
            quick = true;
        } else if (std::string_view(argv[i]) == "--smoke-scale" && i + 1 < argc) {
            char *end = nullptr;
            scale = std::strtod(argv[++i], &end);
            if (*end != '\0' || !std::isfinite(scale) || scale <= 0) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}
// Reports both the distribution and the explicit gate used by the caller.
bool report(const Case &fixture, const Timing &timing, double scale) {
    const double median = timing.samples[timing.samples.size() / 2];
    const bool pass = scale == 0 || median <= fixture.limit_ns * scale;
    std::cout << "{\"name\":" << std::quoted(fixture.name)
              << ",\"wire_bytes\":" << fixture.wire.size()
              << ",\"content_bytes\":" << fixture.content << ",\"iterations\":" << timing.iterations
              << ",\"repetitions\":" << timing.samples.size() << ",\"median_ns\":" << median
              << ",\"min_ns\":" << timing.samples.front() << ",\"max_ns\":" << timing.samples.back()
              << ",\"smoke_limit_ns\":" << fixture.limit_ns * scale
              << ",\"pass\":" << (pass ? "true" : "false") << "}";
    return pass;
}
} // namespace

// Runs validated fixtures and returns 1 for a fixture or selected latency failure.
int main(int argc, char **argv) {
    bool quick = false;
    double scale = 0;
    if (!arguments(argc, argv, quick, scale)) {
        std::fprintf(stderr, "usage: http_parser_bench [--quick] [--smoke-scale POSITIVE]\n");
        return 2;
    }
    try {
        const auto cases = fixtures();
        for (const auto &fixture : cases) {
            if (run(fixture) != expected_result(fixture)) {
                std::fprintf(stderr, "invalid benchmark fixture: %s\n", fixture.name.c_str());
                return 1;
            }
        }
        std::cout << std::fixed << std::setprecision(3)
                  << "{\"compiler\":" << std::quoted(ORANGE_BENCH_COMPILER)
                  << ",\"flags\":" << std::quoted(ORANGE_BENCH_FLAGS)
                  << ",\"architecture\":" << std::quoted(ORANGE_BENCH_ARCH) << ",\"cases\":[";
        std::uint64_t checksum = 0;
        bool pass = true;
        for (std::size_t i = 0; i < cases.size(); ++i) {
            if (i != 0)
                std::cout << ',';
            pass = report(cases[i], measure(cases[i], quick, checksum), scale) && pass;
        }
        std::cout << "],\"checksum\":" << checksum << "}\n";
        return pass ? 0 : 1;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "benchmark failed: %s\n", error.what());
        return 1;
    }
}
