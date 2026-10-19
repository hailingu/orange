// ADR: service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md
// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Performance authorization: docs/reviews/2026-10-16-http-performance.md.
// Internal constant-storage validation of streaming chunk-size metadata.
#ifndef ORANGE_HTTP_CHUNK_LINE_HPP
#define ORANGE_HTTP_CHUNK_LINE_HPP
#include <cstddef>
#include <string_view>
namespace orange::http::detail {
// A chunk line is either pending, complete, invalid, or above the content quota.
enum class ChunkLineResult { pending, complete, malformed, too_large };
// Number of supplied octets consumed, including a terminal delimiter or bad octet.
struct ChunkLineStep {
    std::size_t consumed;
    ChunkLineResult result;
};
// Validates one size line and ignores syntactically valid extensions. No
// extension text is retained, even when a quoted value spans many fragments.
class ChunkLine {
  public:
    // Starts a fresh size line bounded by the remaining decoded-content quota.
    void reset(std::size_t quota);
    // Advances one metadata octet; complete includes the terminating CRLF.
    [[nodiscard]] ChunkLineResult feed(char c);
    // Advances a bounded input span, stopping at the first non-pending result.
    // The caller supplies only bytes remaining inside its metadata budget.
    [[nodiscard]] ChunkLineStep feed(std::string_view input);
    // Returns the decoded chunk size after a complete size line.
    [[nodiscard]] std::size_t size() const { return size_; }

  private:
    enum class Phase {
        size,
        before_separator,
        before_extension,
        name,
        after_name,
        before_value,
        token_value,
        after_value,
        quoted_value,
        escape,
        after_quote,
        lf
    };
    Phase phase_ = Phase::size;
    std::size_t size_ = 0;
    std::size_t quota_ = 0;
    bool digit_seen_ = false;
    bool oversized_ = false;
    // Adds a known hexadecimal digit with the same overflow/quota rule on both paths.
    void add_digit(char c);
    // Handles the hexadecimal prefix without overflowing its bounded accumulator.
    ChunkLineResult size_byte(char c);
    // Handles extension separators and the end of a complete name/value.
    ChunkLineResult separator(char c);
    // Handles token and quoted extension values.
    ChunkLineResult value_byte(char c);
};
} // namespace orange::http::detail
#endif
