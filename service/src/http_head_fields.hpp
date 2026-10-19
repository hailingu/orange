// ADR: service/docs/adr/ADR-0003-fragmented-head-offsets.md
// ADR: service/docs/adr/ADR-0004-owned-request-storage.md
// Authorization: docs/reviews/2026-10-16-http-performance.md.
// Storage authorization: docs/reviews/2026-10-16-owned-request-storage.md.
// Internal validated offsets shared by complete and fragmented head construction.
#ifndef ORANGE_HTTP_HEAD_FIELDS_HPP
#define ORANGE_HTTP_HEAD_FIELDS_HPP
#include "http_messages.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace orange::http::detail {
// Empty leading lines stay pending; only an empty line after the request completes.
enum class HeadLineResult { pending, complete, malformed };
// Retains offsets, never borrowed views. Descriptor storage grows only for received
// fields and is bounded by the decoder's head cap, rather than a per-connection
// maximum-sized array. Allocation failures propagate to RequestDecoder's handler.
class HeadFields {
  public:
    // Supplies one CRLF-stripped line at its absolute offset in a capped head.
    [[nodiscard]] HeadLineResult append_line(std::string_view line, std::size_t begin);
    // Constructs owned fields after append_line reports complete. The unchanged
    // head must contain all supplied lines; no grammar scan is repeated here.
    [[nodiscard]] ParsedRequest finish(std::string_view head, std::size_t consumed) const;

  private:
    RequestLineOffsets request_line_{};
    bool request_line_seen_ = false;
    std::vector<HeaderFieldOffsets> fields_;
};
} // namespace orange::http::detail
#endif
