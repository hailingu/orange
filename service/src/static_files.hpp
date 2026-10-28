// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Immutable static content of the orange loopback service (ADR-0003, subtask
// T-2): one startup load owns every served file, so request dispatch never
// touches the filesystem and holds no mutable state — one store can be shared
// by concurrent acceptor loops (RequestHandler contract).
#ifndef ORANGE_HTTP_STATIC_FILES_HPP
#define ORANGE_HTTP_STATIC_FILES_HPP
#include "http_request_decoder.hpp"
#include "http_response.hpp"
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace orange::http {

// Why a static tree could not be loaded: the failing path and the errno of
// the failing operation, for the assembly-time diagnostic (CON-07).
struct StaticLoadError {
    std::string path;
    int error = 0;
};

// One served file: the normalized origin-form key ("/index.html", the root
// index mapping already applied), the media type from the declared MIME map,
// and the owned bytes (binary-safe; an empty file is a valid asset).
struct StaticAsset {
    std::string path;
    std::string media_type;
    std::string content;
};

// Owns the static tree of one deployment, loaded once before serving. After a
// successful load the value is immutable and safe to read from every acceptor
// loop; do not move or mutate it once dispatch starts, because response
// bodies and field values borrow asset storage for the serializing call that
// follows each response.
class StaticFileStore {
  public:
    // Loads every regular file under document_root (recursively; other file
    // types are skipped) into owned storage, keyed by each file's origin-form
    // path with '/' separators. Returns the first StaticLoadError — failing
    // path and errno — when the directory cannot be walked or a file cannot
    // be read.
    [[nodiscard]] static std::variant<StaticFileStore, StaticLoadError>
    load(const std::string &document_root);

    // Resolves one origin-form request-target to its asset: strips any query,
    // percent-decodes the path, applies the "/" → "/index.html" root mapping,
    // and rejects any target whose decoded path does not start with '/' or
    // carries a "." or ".." segment. Returns nullptr when no loaded asset
    // matches; resolution never touches the filesystem.
    [[nodiscard]] const StaticAsset *find(std::string_view target) const;

    // Serves one resolved asset for a GET request (RFC 9110 14): the full
    // file as 200, or the request's single `bytes=` range as 206 with a
    // satisfied Content-Range, or 416 with `bytes */total` when the range is
    // unsatisfiable (a first byte at or past the end, any range on an empty
    // file, or suffix length zero). A Range header that is absent, malformed,
    // multi-range, in another unit, or first-greater-than-last is ignored and
    // the full file is served. The returned body and field values borrow the
    // asset and stay valid for the serializing call that follows.
    [[nodiscard]] static Response serve_get(const StaticAsset &asset, const ParsedMessage &request);

  private:
    // Sorted by path; find() binary-searches this vector.
    std::vector<StaticAsset> assets_;
    // Binary search for one normalized stored key.
    [[nodiscard]] const StaticAsset *find_by_key(const std::string &key) const;
};

} // namespace orange::http
#endif // ORANGE_HTTP_STATIC_FILES_HPP
