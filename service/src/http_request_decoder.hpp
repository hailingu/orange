// Governed by docs/adr/ADR-0003-local-runtime-path.md.
// Incremental request-message decoding for one connection. The head parser
// remains available separately; this layer owns body framing and decoded bytes.
#ifndef ORANGE_HTTP_REQUEST_DECODER_HPP
#define ORANGE_HTTP_REQUEST_DECODER_HPP
#include "http_messages.hpp"
#include <memory>
#include <string_view>
#include <variant>
namespace orange::http {
// One complete message with owned head fields and binary-safe decoded content.
struct ParsedMessage {
    Request request;
    std::string body;
    // Counts wire bytes, including chunk framing and discarded trailer fields.
    std::size_t consumed = 0;
};
// Bytes taken from this fragment and either a complete message, an error,
// or monostate while more input is needed. Pipeline bytes stay with the caller.
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
    // after completion, later feeds consume zero bytes until reset.
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
