// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Authorization: docs/reviews/2026-10-16-owned-request-storage.md.
// Verifies allocation bounds and independently owned request lifetimes.
#include "http_request_decoder.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace {
bool counting = false;
std::size_t allocations = 0;
std::size_t fail_at = 0;
int failures = 0;
} // namespace
// Observes real ordinary allocations; fixture setup is outside the count.
void *operator new(std::size_t size) {
    if (fail_at != 0 && --fail_at == 0)
        throw std::bad_alloc{};
    if (void *memory = std::malloc(size == 0 ? 1 : size)) {
        if (counting)
            ++allocations;
        return memory;
    }
    throw std::bad_alloc{};
}
// Releases the same real allocations that the counting replacement observes.
void operator delete(void *memory) noexcept { std::free(memory); }

namespace {
// CTest receives a failing exit status with the violated behavior.
void check(bool condition, const char *description) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}
// Keeps the experiment test usable against both request representations.
template <class T> std::string_view target(const T &request) {
    if constexpr (requires { request.target(); })
        return request.target();
    else
        return request.target;
}
// Reads fields without imposing the candidate's storage layout on expectations.
template <class T> decltype(auto) fields(const T &request) {
    if constexpr (requires { request.headers(); })
        return request.headers();
    else
        return (request.headers);
}
// Long names, values and target expose separate per-string allocations.
std::string long_head() {
    std::string wire = "CUSTOM-METHOD-WITH-LONG-NAME /" + std::string(64, 'p') + " HTTP/1.1\r\n";
    for (int i = 0; i < 16; ++i)
        wire += "Long-Header-Name-For-Storage: " + std::string(128, 'v') + "\r\n";
    return wire + "\r\n";
}
// This fails if materialization allocates separately for each long field.
void check_allocations() {
    auto wire = long_head();
    allocations = 0;
    counting = true;
    const auto result = orange::http::parse_request(wire);
    counting = false;
    std::printf("long fields: %zu ordinary allocations\n", allocations);
    check(std::holds_alternative<orange::http::ParsedRequest>(result) && allocations <= 2,
          "a complete head owns all long components with at most two allocations");
    wire.resize(wire.size() - 2);
    allocations = 0;
    counting = true;
    const auto partial = orange::http::parse_request(wire);
    counting = false;
    check(std::get_if<orange::http::ParseError>(&partial) && allocations == 0,
          "incomplete heads allocate no result storage");
}
// A copy must not keep views into the original request or input buffer.
void check_copies() {
    auto wire = long_head();
    auto result = orange::http::parse_request(wire);
    auto *parsed = std::get_if<orange::http::ParsedRequest>(&result);
    check(parsed != nullptr, "lifetime fixture parses");
    if (!parsed)
        return;
    auto copy = parsed->request;
    orange::http::Request assigned;
    assigned = copy;
    wire.assign(wire.size(), 'z');
    parsed->request = orange::http::Request{};
    copy = orange::http::Request{};
    check(target(assigned) == "/" + std::string(64, 'p'),
          "copy construction and assignment retain independent owned bytes");
    check(fields(assigned).size() == 16 && fields(assigned)[15].value == std::string(128, 'v'),
          "copied offsets describe the copy's long fields");
}
// Copying discarded OWS must not turn a tiny logical field into a large buffer.
void check_padding_storage() {
    const auto wire = "GET / HTTP/1.1\r\nX: " + std::string(3900, ' ') + "v" +
                      std::string(3900, '\t') + "\r\n\r\n";
    allocations = 0;
    counting = true;
    const auto result = orange::http::parse_request(wire);
    counting = false;
    const auto *parsed = std::get_if<orange::http::ParsedRequest>(&result);
    check(parsed && fields(parsed->request)[0].value == "v" && allocations <= 1,
          "a tiny field surrounded by discarded OWS needs at most one allocation");
}
// Fixed framing declares the exact body length, so fragmented delivery
// must reserve body storage once rather than regrow it geometrically per
// fragment; every allocation site on that path stays a coded outcome.
void check_fixed_body_allocations() {
    const std::size_t body_size = 262145;
    const std::string wire = "POST / HTTP/1.1\r\nContent-Length: " + std::to_string(body_size) +
                             "\r\n\r\n" + std::string(body_size, 'x');
    orange::http::RequestDecoder decoder;
    allocations = 0;
    counting = true;
    bool completed = false;
    for (std::size_t offset = 0; offset < wire.size(); offset += 4096) {
        const auto result = decoder.feed(std::string_view(wire).substr(offset, 4096));
        if (const auto *parsed = std::get_if<orange::http::ParsedMessage>(&result.outcome))
            completed = parsed->body.size() == body_size;
    }
    counting = false;
    const std::size_t total = allocations;
    std::printf("fragmented fixed body: %zu ordinary allocations\n", total);
    check(completed && total <= 6, "a fragmented fixed-length body reserves body storage once");
    for (std::size_t step = 1; step <= total; ++step) {
        orange::http::RequestDecoder failing;
        fail_at = step;
        bool reported = false;
        for (std::size_t offset = 0; offset < wire.size() && !reported; offset += 4096) {
            const auto result = failing.feed(std::string_view(wire).substr(offset, 4096));
            const auto *error = std::get_if<orange::http::ParseError>(&result.outcome);
            reported = error && *error == orange::http::ParseError::resource_exhausted;
        }
        fail_at = 0;
        check(reported, "fixed-body allocation failure returns resource_exhausted");
    }
}
// Moves must preserve short inline strings and heap-backed data alike.
void check_moves() {
    for (const auto &wire : {std::string("GET / HTTP/1.1\r\n\r\n"),
                             std::string("GET /p HTTP/1.1\r\nX: a\r\n\r\n"), long_head()}) {
        auto result = orange::http::parse_request(wire);
        auto *parsed = std::get_if<orange::http::ParsedRequest>(&result);
        if (!parsed) {
            check(false, "move fixture parses");
            continue;
        }
        const auto want = std::string(target(parsed->request));
        auto moved = std::move(parsed->request);
        check(target(parsed->request).empty() && fields(parsed->request).empty(),
              "move construction leaves the source safely empty");
        parsed->request = orange::http::Request{};
        orange::http::Request assigned;
        assigned = std::move(moved);
        moved = orange::http::Request{};
        check(target(assigned) == want, "move construction and assignment preserve owned slices");
        auto *alias = &assigned;
        assigned = *alias;
        check(target(assigned) == want, "self-copy assignment preserves owned slices");
        assigned = std::move(*alias);
        check(target(assigned) == want, "self-move assignment preserves owned slices");
    }
}
// A decoder's next lifecycle must not invalidate a previously returned request.
void check_reset() {
    orange::http::RequestDecoder decoder;
    auto first = decoder.feed(long_head());
    decoder.reset();
    const auto next = decoder.feed("GET /next HTTP/1.1\r\n\r\n");
    auto *saved = std::get_if<orange::http::ParsedMessage>(&first.outcome);
    check(saved && target(saved->request) == "/" + std::string(64, 'p') &&
              fields(saved->request)[0].value == std::string(128, 'v') &&
              std::holds_alternative<orange::http::ParsedMessage>(next.outcome),
          "decoder reset and reuse preserve outstanding request storage");
}
// Allocation failures are coded parse outcomes, never escaping parser exceptions.
void check_failures() {
    const auto wire = long_head();
    for (std::size_t step = 1; step <= 2; ++step) {
        fail_at = step;
        bool returned_error = false;
        try {
            const auto result = orange::http::parse_request(wire);
            const auto *error = std::get_if<orange::http::ParseError>(&result);
            returned_error = error && *error == orange::http::ParseError::resource_exhausted;
        } catch (const std::bad_alloc &) {
        }
        fail_at = 0;
        check(returned_error, "owned head allocation failure returns resource_exhausted");
    }
    for (std::size_t step = 1; step <= 3; ++step) {
        orange::http::RequestDecoder decoder;
        fail_at = step;
        const auto result = decoder.feed(wire);
        fail_at = 0;
        const auto *error = std::get_if<orange::http::ParseError>(&result.outcome);
        const auto again = decoder.feed(wire);
        check(error && *error == orange::http::ParseError::resource_exhausted &&
                  std::get_if<orange::http::ParseError>(&again.outcome) &&
                  std::get<orange::http::ParseError>(again.outcome) == *error,
              "decoder and owned-storage failures remain terminal until reset");
    }
}
// A failed value copy must not pair new bytes with the destination's old offsets.
void check_copy_failure() {
    const auto source = orange::http::parse_request(long_head());
    const auto *parsed = std::get_if<orange::http::ParsedRequest>(&source);
    auto destination = orange::http::parse_request("GET /stable HTTP/1.1\r\nX: old\r\n\r\n");
    auto *saved = std::get_if<orange::http::ParsedRequest>(&destination);
    check(parsed && saved, "copy-failure fixtures parse");
    if (!parsed || !saved)
        return;
    for (std::size_t step = 1; step <= 2; ++step) {
        fail_at = step;
        bool failed = false;
        try {
            saved->request = parsed->request;
        } catch (const std::bad_alloc &) {
            failed = true;
        }
        fail_at = 0;
        check(failed && target(saved->request) == "/stable" &&
                  fields(saved->request)[0].value == "old" &&
                  target(parsed->request) == "/" + std::string(64, 'p'),
              "failed copy assignment preserves both complete request values");
    }
}
} // namespace
// Runs allocation and lifetime gates before and after representation changes.
int main() {
    check_allocations();
    check_copies();
    check_padding_storage();
    check_fixed_body_allocations();
    check_moves();
    check_reset();
    check_failures();
    check_copy_failure();
    return failures ? 1 : 0;
}
