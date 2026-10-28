// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Wiring stub for the T-2 static-serving rung: the public contract is
// declared in static_files.hpp and pinned red by
// service/tests/http_static_files_tests.cpp; the load, resolution and
// single-range serving behavior lands in the implementing change.
#include "static_files.hpp"
#include <cerrno>

namespace orange::http {

std::variant<StaticFileStore, StaticLoadError>
StaticFileStore::load(const std::string &document_root) {
    return StaticLoadError{document_root, ENOSYS};
}

const StaticAsset *StaticFileStore::find(std::string_view target) const {
    static_cast<void>(target);
    return nullptr;
}

Response StaticFileStore::serve_get(const StaticAsset &asset, const ParsedMessage &request) {
    static_cast<void>(&asset);
    static_cast<void>(&request);
    return Response{};
}

} // namespace orange::http
