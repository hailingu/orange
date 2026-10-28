// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Pins the T-2 static-serving contract: one startup load owns the served
// tree (media types from the declared MIME map, binary-safe bytes), targets
// resolve by query-stripping, percent-decoding, the root index mapping and
// traversal rejection without filesystem access, and GET serves the declared
// single-range behavior — 206 with a satisfied Content-Range, 416 with
// bytes */total, and ignored Range headers answering the full file.
#include "routes.hpp"
#include "static_files.hpp"
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace {
using orange::http::BodyFraming;
using orange::http::ContentRange;
using orange::http::handle_request;
using orange::http::parse_message;
using orange::http::ParsedMessage;
using orange::http::Response;
using orange::http::StaticAsset;
using orange::http::StaticFileStore;
using orange::http::StaticLoadError;
using orange::http::write_response;

int failures = 0;
std::size_t checks = 0;
// Gives CTest a nonzero exit for any observable static-serving mismatch.
void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "CHECK failed: %s\n", description);
    }
}

// One complete parsed message from its full wire form; the optional owns the
// message, so no view dangles into a variant temporary.
std::optional<ParsedMessage> message_of(std::string_view wire) {
    auto parsed = parse_message(wire);
    if (auto *message = std::get_if<ParsedMessage>(&parsed))
        return std::move(*message);
    return std::nullopt;
}

// One bodyless GET for one target.
std::optional<ParsedMessage> get_of(std::string_view target) {
    return message_of("GET " + std::string(target) + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
}

// One bodyless GET for one target with one method and an added field line.
std::optional<ParsedMessage> request_of(std::string_view method, std::string_view target,
                                        std::string_view extra_field) {
    return message_of(std::string(method) + " " + std::string(target) +
                      " HTTP/1.1\r\nHost: localhost\r\n" + std::string(extra_field) + "\r\n");
}

// The serialized wire bytes, or empty when the response does not serialize.
std::string wire_of(const Response &response) {
    auto result = write_response(response);
    if (const auto *bytes = std::get_if<std::string>(&result))
        return *bytes;
    return {};
}

// The hand-built index asset used by the range checks; it lives in static
// storage because serve_get's returned body and field views borrow the asset
// and must outlive the call.
const StaticAsset &index_asset() {
    static const StaticAsset asset{"/index.html", "text/html", "<html>orange</html>"};
    return asset;
}

// serve_get answers the full asset when no Range header is present.
void check_serve_full(const StaticAsset &asset) {
    const auto request = get_of("/index.html");
    check(request.has_value(), "full request parses");
    if (!request)
        return;
    const Response response = StaticFileStore::serve_get(asset, *request);
    check(response.status == 200, "GET without Range answers 200");
    check(response.framing == BodyFraming::content_length,
          "the full asset is framed with Content-Length");
    check(response.body == "<html>orange</html>", "the full asset body is byte-exact");
    check(response.fields.size() == 1 && response.fields[0].name == "Content-Type" &&
              response.fields[0].value == "text/html",
          "the full asset carries its media type");
    check(response.close == false, "static responses keep the connection reusable");
    check(!response.content_range.has_value(), "a full answer carries no Content-Range");
}

// The response of one ranged GET against the hand-built asset, through
// serve_get directly; the 19-byte body makes every boundary observable.
Response ranged(const std::string_view header) {
    const auto request = request_of("GET", "/index.html", "Range: " + std::string(header) + "\r\n");
    if (!request) {
        check(false, "ranged request parses");
        return Response{};
    }
    return StaticFileStore::serve_get(index_asset(), *request);
}

// Satisfiable single ranges answer 206 with the exact slice.
void check_satisfiable_ranges() {
    const struct {
        std::string_view header;
        std::string_view body;
        ContentRange range;
    } cases[] = {
        {"bytes=0-3", "<htm", ContentRange{19, true, 0, 3}},
        {"bytes=6-", "orange</html>", ContentRange{19, true, 6, 18}},
        {"bytes=-4", "tml>", ContentRange{19, true, 15, 18}},
        {"bytes=-100", "<html>orange</html>", ContentRange{19, true, 0, 18}},
        {"bytes=2-100", "tml>orange</html>", ContentRange{19, true, 2, 18}},
        {"BYTES=0-3", "<htm", ContentRange{19, true, 0, 3}},
    };
    for (const auto &entry : cases) {
        const Response response = ranged(entry.header);
        check(response.status == 206, "a satisfiable single range answers 206");
        check(response.framing == BodyFraming::content_length,
              "a 206 slice is framed with Content-Length");
        check(response.body == entry.body, "the 206 body is the exact slice");
        check(response.content_range.has_value() &&
                  response.content_range->total == entry.range.total &&
                  response.content_range->satisfied == entry.range.satisfied &&
                  response.content_range->first == entry.range.first &&
                  response.content_range->last == entry.range.last,
              "the Content-Range carries the resolved bounds");
        check(response.fields.size() == 1 && response.fields[0].name == "Content-Type" &&
                  response.fields[0].value == "text/html",
              "a 206 keeps the asset media type");
        check(response.close == false, "a 206 keeps the connection reusable");
    }
    check(wire_of(ranged("bytes=0-3")) == "HTTP/1.1 206 Partial Content\r\n"
                                          "Content-Type: text/html\r\n"
                                          "Content-Range: bytes 0-3/19\r\n"
                                          "Content-Length: 4\r\n"
                                          "\r\n"
                                          "<htm",
          "a satisfied 206 serializes its exact wire");
    const auto binary_request = request_of("GET", "/data.bin", "Range: bytes=1-3\r\n");
    const StaticAsset binary{"/data.bin", "application/octet-stream",
                             std::string("\0\r\nA\xff", 5)};
    if (binary_request) {
        const Response slice = StaticFileStore::serve_get(binary, *binary_request);
        check(slice.status == 206 && slice.body == std::string_view("\r\nA", 3),
              "a range slice is binary-safe");
    }
}

// Unsatisfiable ranges answer 416 with bytes */total and no body.
void check_unsatisfiable_ranges(const StaticAsset &asset) {
    const std::string_view headers[] = {"bytes=19-", "bytes=25-30", "bytes=-0"};
    for (const std::string_view header : headers) {
        const Response response = ranged(header);
        check(response.status == 416, "an unsatisfiable range answers 416");
        check(response.content_range.has_value() && response.content_range->total == 19 &&
                  !response.content_range->satisfied,
              "a 416 carries bytes */total as its Content-Range");
        check(response.framing == BodyFraming::content_length && response.body.empty(),
              "a 416 frames its empty body with Content-Length: 0");
        check(response.close == false, "a 416 keeps the connection reusable");
    }
    check(wire_of(ranged("bytes=19-")) == "HTTP/1.1 416 Range Not Satisfiable\r\n"
                                          "Content-Range: bytes */19\r\n"
                                          "Content-Length: 0\r\n"
                                          "\r\n",
          "a 416 serializes its exact wire");
    // An empty representation has no satisfiable byte range at all.
    const StaticAsset empty{"/empty.txt", "text/plain", ""};
    for (const std::string_view header : {"bytes=0-", "bytes=-3", "bytes=0-0"}) {
        const auto request =
            request_of("GET", "/empty.txt", "Range: " + std::string(header) + "\r\n");
        if (!request)
            continue;
        const Response response = StaticFileStore::serve_get(empty, *request);
        check(response.status == 416 && response.content_range.has_value() &&
                  response.content_range->total == 0 && !response.content_range->satisfied,
              "an empty asset answers 416 with bytes */0");
    }
    static_cast<void>(asset);
}

// Ignored Range headers answer the full file (RFC 9110 14.2).
void check_ignored_ranges() {
    const std::string_view headers[] = {
        "bytes=5-2",  "bytes=abc", "bytes=-",  "items=0-1", "bytes=0-1,3-4",
        "bytes= 0-3", "bytes=+2-", "bytes=-a", "nonsense",  "bytes=0-1;x",
    };
    for (const std::string_view header : headers) {
        const Response response = ranged(header);
        check(response.status == 200 && response.body == "<html>orange</html>",
              "an unparseable, multi-range, foreign-unit or inverted Range is ignored");
        check(!response.content_range.has_value(), "an ignored Range adds no Content-Range");
    }
    const auto doubled = message_of("GET /index.html HTTP/1.1\r\nHost: localhost\r\n"
                                    "Range: bytes=0-3\r\nRange: bytes=6-\r\n\r\n");
    check(doubled.has_value(), "doubled-Range request parses");
    if (doubled) {
        const Response response = StaticFileStore::serve_get(index_asset(), *doubled);
        check(response.status == 200 && response.body == "<html>orange</html>",
              "two Range field lines combine into an ignored multi-range");
    }
}

// Creates the generated fixture tree under a fresh temporary directory: the
// media types, nesting, binary bytes, spaces and casing the load checks need.
std::optional<std::string> make_fixture() {
    std::vector<char> pattern;
    const std::string_view base = "/tmp/orange-static-tests-XXXXXX";
    pattern.assign(base.begin(), base.end());
    pattern.push_back('\0');
    if (mkdtemp(pattern.data()) == nullptr)
        return std::nullopt;
    const std::string root = pattern.data();
    if (mkdir((root + "/sub").c_str(), 0755) != 0)
        return std::nullopt;
    const std::pair<std::string, std::string> files[] = {
        {"/index.html", "<html>orange</html>"},
        {"/sub/app.css", "body{margin:0}"},
        {"/data.bin", std::string("\0\r\nA\xff", 5)},
        {"/empty.txt", ""},
        {"/noext", "raw"},
        {"/a b.txt", "hi"},
        {"/pic.PNG", "\x89PNG"},
    };
    for (const auto &[name, content] : files) {
        FILE *file = std::fopen((root + name).c_str(), "wb");
        if (file == nullptr)
            return std::nullopt;
        const std::size_t written = std::fwrite(content.data(), 1, content.size(), file);
        std::fclose(file);
        if (written != content.size())
            return std::nullopt;
    }
    return root;
}

// Removes the fixture tree; its exact shape is known, so plain unlinks do.
void remove_fixture(const std::string &root) {
    for (const std::string_view name : {"/index.html", "/sub/app.css", "/data.bin", "/empty.txt",
                                        "/noext", "/a b.txt", "/pic.PNG"})
        std::remove((root + std::string(name)).c_str());
    std::remove((root + "/sub").c_str());
    std::remove(root.c_str());
}

// One startup load owns the tree; resolution never touches the filesystem.
void check_store_load(const std::string &root) {
    const auto loaded = StaticFileStore::load(root);
    const auto *store = std::get_if<StaticFileStore>(&loaded);
    check(store != nullptr, "the fixture tree loads");
    if (!store)
        return;
    const auto *index = store->find("/index.html");
    check(index != nullptr, "/index.html resolves");
    check(index && index->path == "/index.html" && index->media_type == "text/html" &&
              index->content == "<html>orange</html>",
          "the index asset carries its key, media type and exact bytes");
    check(store->find("/") == index, "the root maps to the directory index");
    check(store->find("/index.html?v=2") == index, "a query is stripped before resolution");
    const auto *css = store->find("/sub/app.css");
    check(css != nullptr && css->media_type == "text/css",
          "a nested file resolves with its media type");
    const auto *space = store->find("/a%20b.txt");
    check(space != nullptr && space->content == "hi",
          "percent-encoded octets decode before matching");
    const auto *bin = store->find("/data.bin");
    check(bin != nullptr && bin->media_type == "application/octet-stream" &&
              bin->content == std::string("\0\r\nA\xff", 5),
          "an unmapped extension serves its binary bytes as octet-stream");
    const auto *png = store->find("/pic.PNG");
    check(png != nullptr && png->media_type == "image/png",
          "extension matching is case-insensitive");
    check(store->find("/noext") != nullptr, "an extensionless file is servable");
    for (const std::string_view target :
         {"/missing.html", "/sub/", "/sub", "/..", "/../etc/passwd", "/sub/../index.html",
          "/a/../index.html", "index.html", "/%2e%2e/etc/passwd"})
        check(store->find(target) == nullptr, "unresolvable targets do not resolve");
    check(store->find("/.") == index && store->find("/./index.html") == index,
          "dot segments normalize away (RFC 3986 6.2.2.3)");
    const auto failed = StaticFileStore::load("/tmp/orange-static-tests-absent");
    const auto *error = std::get_if<StaticLoadError>(&failed);
    check(error != nullptr && error->error != 0,
          "loading an absent tree reports its path and errno");
}

// Dispatch reaches static content after the declared API routes.
void check_static_dispatch(const StaticFileStore &store) {
    const auto page = get_of("/");
    check(page.has_value(), "root request parses");
    if (page) {
        const Response response = handle_request(*page, &store);
        check(response.status == 200 && response.framing == BodyFraming::content_length &&
                  response.body == "<html>orange</html>" && response.close == false,
              "GET / dispatches to the index asset");
        check(wire_of(response) == "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/html\r\n"
                                   "Content-Length: 19\r\n"
                                   "\r\n"
                                   "<html>orange</html>",
              "GET / serializes its exact wire");
    }
    const auto css = get_of("/sub/app.css");
    if (css)
        check(wire_of(handle_request(*css, &store)) == "HTTP/1.1 200 OK\r\n"
                                                       "Content-Type: text/css\r\n"
                                                       "Content-Length: 14\r\n"
                                                       "\r\n"
                                                       "body{margin:0}",
              "a nested asset serializes its exact wire");
    for (const std::string_view method : {"POST", "DELETE", "PUT"}) {
        const auto request = request_of(method, "/index.html", "");
        check(request.has_value(), "static method request parses");
        if (!request)
            continue;
        const Response response = handle_request(*request, &store);
        check(response.status == 405 && response.fields.size() == 1 &&
                  response.fields[0].name == "Allow" && response.fields[0].value == "GET",
              "a non-GET method on a resolved asset answers 405 with Allow: GET");
    }
    const auto missing = get_of("/missing.html");
    if (missing) {
        const Response response = handle_request(*missing, &store);
        check(response.status == 404 && response.body.empty() &&
                  response.framing == BodyFraming::content_length,
              "an unresolved target answers 404");
    }
    const auto post_missing = request_of("POST", "/missing.html", "");
    if (post_missing)
        check(handle_request(*post_missing, &store).status == 404,
              "an unresolved target answers 404 for any method");
    const auto index = get_of("/index.html");
    if (index)
        check(handle_request(*index, nullptr).status == 404,
              "a null store serves no static content");
    const auto health = get_of("/api/health");
    if (health)
        check(handle_request(*health, &store).status == 200 &&
                  handle_request(*health, &store).body == "{\"status\":\"ok\"}",
              "the API routes answer unchanged beside static content");
    const auto ranged = request_of("GET", "/index.html", "Range: bytes=6-\r\n");
    if (ranged) {
        const Response response = handle_request(*ranged, &store);
        check(response.status == 206 && response.body == "orange</html>" &&
                  response.content_range.has_value() && response.content_range->first == 6 &&
                  response.content_range->last == 18,
              "a ranged GET dispatches through the store to a 206 slice");
    }
}

} // namespace

// Executes the static-load, resolution, single-range and dispatch checks.
int main() {
    const StaticAsset &asset = index_asset();
    check_serve_full(asset);
    check_satisfiable_ranges();
    check_unsatisfiable_ranges(asset);
    check_ignored_ranges();
    const auto root = make_fixture();
    check(root.has_value(), "the fixture tree is created");
    if (root) {
        check_store_load(*root);
        const auto loaded = StaticFileStore::load(*root);
        if (const auto *store = std::get_if<StaticFileStore>(&loaded)) {
            check_static_dispatch(*store);
        } else {
            check(false, "the store loads for the dispatch checks");
        }
        remove_fixture(*root);
    }
    std::printf("%zu checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
