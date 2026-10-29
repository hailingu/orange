// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
// Incremental request-message decoding for one connection. The head parser
// remains available separately; this layer owns body framing and decoded bytes.
#ifndef ORANGE_HTTP_REQUEST_DECODER_HPP
#define ORANGE_HTTP_REQUEST_DECODER_HPP
#include "http_messages.hpp"
#include <memory>
#include <string_view>
#include <variant>
namespace orange::http {
// Aggregate wire budget for chunk size lines, extensions, delimiters and
// discarded trailers, independent of the decoded-body and request-head caps.
// Exceeding it returns chunk_metadata_too_long (HTTP 413).
// See RFC 9112 section 7.1.1.
inline constexpr std::size_t chunk_metadata_cap = 8192;
// One complete message with owned head fields and binary-safe decoded content.
struct ParsedMessage {
    Request request;
    std::string body;
    // Counts wire bytes, including chunk framing and discarded trailer fields.
    std::size_t consumed = 0;
    // The request's Connection: close option (RFC 9110 7.6.1), resolved during
    // the same single head-field pass that fixes the body framing.
    bool close = false;
};
// Bytes taken from this fragment and either a complete message, an error,
// or monostate while more input is needed. Pipeline bytes stay with the caller
// on success. On ANY error, consumed is discard accounting, not a reusable
// message boundary; discard the connection's remaining input and close it.
struct DecodeResult {
    std::size_t consumed = 0;
    std::variant<std::monostate, ParsedMessage, ParseError> outcome;
};
// Owns one connection's decoding progress. Feed only newly received bytes;
// reset explicitly before the next message. Instances share no mutable state.
class RequestDecoder {
  public:
    // Creates an idle decoder with no borrowed input or body allocation.
    RequestDecoder() noexcept;
    // Releases all buffered request data and parsing state.
    ~RequestDecoder();
    RequestDecoder(const RequestDecoder &) = delete;
    RequestDecoder &operator=(const RequestDecoder &) = delete;
    // Consumes at most one message. Errors remain sticky until reset;
    // after completion, later feeds consume zero bytes until reset. Allocation
    // failure returns resource_exhausted. No error exposes a reusable input
    // boundary; reset is for an independent request on a fresh input stream.
    [[nodiscard]] DecodeResult feed(std::string_view fragment);
    // Discards the previous message's state and starts an independent request.
    void reset();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool allocation_failed_ = false;
};
// Parses one complete message. Missing framing bytes return incomplete;
// trailing pipelined input is excluded from the consumed count.
[[nodiscard]] std::variant<ParsedMessage, ParseError> parse_message(std::string_view raw);
} // namespace orange::http
#endif
