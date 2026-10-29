// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Implementation of the T-2 static-serving capability: one POSIX load walks
// the document root into owned storage, resolution is pure path algebra, and
// single-range GET serving follows RFC 9110 14 with a by-value ContentRange.
#include "static_files.hpp"
#include "http_syntax.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <dirent.h>
#include <fcntl.h>
#include <new>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>

namespace orange::http {
namespace {

// Every media type the declared MIME map can produce, with the static
// Content-Type field pair for each: serve_get borrows from this table, so no
// response field view ever borrows a request-local buffer (RequestHandler
// borrowing contract).
struct MediaTypeEntry {
    std::string_view type;
    ResponseField field;
};
constexpr MediaTypeEntry media_types[] = {
    {"text/html", {"Content-Type", "text/html"}},
    {"text/css", {"Content-Type", "text/css"}},
    {"text/javascript", {"Content-Type", "text/javascript"}},
    {"application/json", {"Content-Type", "application/json"}},
    {"image/svg+xml", {"Content-Type", "image/svg+xml"}},
    {"image/png", {"Content-Type", "image/png"}},
    {"image/jpeg", {"Content-Type", "image/jpeg"}},
    {"image/gif", {"Content-Type", "image/gif"}},
    {"image/x-icon", {"Content-Type", "image/x-icon"}},
    {"text/plain", {"Content-Type", "text/plain"}},
    {"application/wasm", {"Content-Type", "application/wasm"}},
    {"font/woff2", {"Content-Type", "font/woff2"}},
    {"application/octet-stream", {"Content-Type", "application/octet-stream"}},
};

// The table entry for one lowercase extension, or octet-stream.
std::string_view media_type_of_extension(std::string_view extension) {
    struct Mapping {
        std::string_view extension;
        std::string_view type;
    };
    constexpr Mapping mappings[] = {
        {"html", "text/html"},     {"htm", "text/html"},         {"css", "text/css"},
        {"js", "text/javascript"}, {"mjs", "text/javascript"},   {"json", "application/json"},
        {"svg", "image/svg+xml"},  {"png", "image/png"},         {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},    {"gif", "image/gif"},         {"ico", "image/x-icon"},
        {"txt", "text/plain"},     {"wasm", "application/wasm"}, {"woff2", "font/woff2"},
    };
    for (const Mapping &mapping : mappings) {
        if (extension == mapping.extension)
            return mapping.type;
    }
    return "application/octet-stream";
}

// The media type for one origin-form key from its lowercase final suffix; a
// hidden or absent suffix serves as octet-stream.
std::string_view media_type_for(std::string_view path) {
    const std::size_t slash = path.find_last_of('/');
    const std::string_view name = path.substr(slash == std::string_view::npos ? 0 : slash + 1);
    const std::size_t dot = name.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0)
        return "application/octet-stream";
    std::string extension{name.substr(dot + 1)};
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(c | 0x20); });
    return media_type_of_extension(extension);
}

// Reads one whole file into owned bytes; nullopt leaves errno of the failing
// call. EINTR retries; memory is bounded by the file itself.
std::optional<std::string> read_file(const std::string &path) {
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0)
        return std::nullopt;
    std::string content;
    char buffer[4096];
    for (;;) {
        const ssize_t received = ::read(descriptor, buffer, sizeof(buffer));
        if (received < 0) {
            if (errno == EINTR)
                continue;
            ::close(descriptor);
            return std::nullopt;
        }
        if (received == 0)
            break;
        content.append(buffer, static_cast<std::size_t>(received));
    }
    if (::close(descriptor) != 0)
        return std::nullopt;
    return content;
}

// Walks one directory depth-first: every regular file becomes an asset keyed
// by its origin-form path, directories recurse, and other file types,
// including symlinks, are skipped. Returns the errno of the first failing
// call with its path, or 0.
int collect_directory(const std::string &disk_path, const std::string &url_path,
                      std::vector<StaticAsset> &assets, std::string &failed_path) {
    DIR *directory = ::opendir(disk_path.c_str());
    if (directory == nullptr) {
        failed_path = disk_path;
        return errno;
    }
    errno = 0;
    while (const dirent *entry = ::readdir(directory)) {
        const std::string_view name = entry->d_name;
        if (name == "." || name == "..") {
            errno = 0;
            continue;
        }
        const std::string child_disk = disk_path + "/" + std::string(name);
        const std::string child_url = url_path + "/" + std::string(name);
        struct stat status{};
        if (::lstat(child_disk.c_str(), &status) != 0) {
            failed_path = child_disk;
            const int error = errno;
            ::closedir(directory);
            return error;
        }
        if (S_ISDIR(status.st_mode)) {
            const int error = collect_directory(child_disk, child_url, assets, failed_path);
            if (error != 0) {
                ::closedir(directory);
                return error;
            }
        } else if (S_ISREG(status.st_mode)) {
            std::optional<std::string> content = read_file(child_disk);
            if (!content) {
                failed_path = child_disk;
                const int error = errno;
                ::closedir(directory);
                return error;
            }
            assets.push_back(StaticAsset{child_url, std::string(media_type_for(child_url)),
                                         std::move(*content)});
        }
        errno = 0;
    }
    const int read_error = errno;
    if (::closedir(directory) != 0) {
        failed_path = disk_path;
        return errno;
    }
    return read_error;
}

// One honored bytes-range spec (RFC 9110 14.1.2).
struct RangeSpec {
    enum class Form { first_last, first_open, suffix };
    Form form;
    std::size_t first = 0;
    std::size_t second = 0;
};

// Parses one decimal run of at most 20 digits; nullopt on any other octet or
// on overflow past size_t.
std::optional<std::size_t> parse_number(std::string_view digits) {
    if (digits.empty() || digits.size() > 20)
        return std::nullopt;
    for (const char digit : digits) {
        if (!detail::is_digit(digit))
            return std::nullopt;
    }
    std::size_t value = 0;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size())
        return std::nullopt;
    return value;
}

// Parses the single `bytes=` range the slice honors; nullopt for anything
// else — absent, split across fields, multi-range, a foreign or malformed
// unit, or first greater than last (RFC 9110 14.1.2) — means the header is
// ignored. The unit name is case-insensitive.
std::optional<RangeSpec> parse_range(std::string_view value) {
    constexpr std::string_view unit = "bytes=";
    if (value.size() < unit.size() || !detail::iequals(value.substr(0, unit.size()), unit))
        return std::nullopt;
    const std::string_view spec = value.substr(unit.size());
    if (spec.find(',') != std::string_view::npos)
        return std::nullopt;
    const std::size_t dash = spec.find('-');
    if (dash == std::string_view::npos)
        return std::nullopt;
    const std::string_view left = spec.substr(0, dash);
    const std::string_view right = spec.substr(dash + 1);
    if (left.empty()) {
        const auto suffix = parse_number(right);
        if (!suffix)
            return std::nullopt;
        return RangeSpec{RangeSpec::Form::suffix, 0, *suffix};
    }
    const auto first = parse_number(left);
    if (!first)
        return std::nullopt;
    if (right.empty())
        return RangeSpec{RangeSpec::Form::first_open, *first, 0};
    const auto last = parse_number(right);
    if (!last || *last < *first)
        return std::nullopt;
    return RangeSpec{RangeSpec::Form::first_last, *first, *last};
}

// The static Content-Type field for one media type; always found because
// load assigns types from this same table, with octet-stream as the default.
const ResponseField &content_type_field(std::string_view media_type) {
    for (const MediaTypeEntry &entry : media_types) {
        if (entry.type == media_type)
            return entry.field;
    }
    return media_types[std::size(media_types) - 1].field;
}

// The single Range field value, only when the request carries exactly one;
// none or several (which combine into a multi-range, RFC 9110 5.2) read as
// absent.
std::optional<std::string_view> single_range_value(const ParsedMessage &request) {
    std::optional<std::string_view> value;
    for (const HeaderField &field : request.request.headers()) {
        if (!detail::iequals(field.name, "range"))
            continue;
        if (value.has_value())
            return std::nullopt;
        value = field.value;
    }
    return value;
}

// The directory index the segment-less root resolves to.
constexpr std::string_view root_index = "/index.html";

} // namespace

std::variant<StaticFileStore, StaticLoadError>
StaticFileStore::load(const std::string &document_root) {
    try {
        std::vector<StaticAsset> assets;
        std::string failed_path;
        const int error = collect_directory(document_root, "", assets, failed_path);
        if (error != 0)
            return StaticLoadError{failed_path, error};
        std::sort(assets.begin(), assets.end(),
                  [](const StaticAsset &left, const StaticAsset &right) {
                      return left.path < right.path;
                  });
        StaticFileStore store;
        store.assets_ = std::move(assets);
        return store;
    } catch (const std::bad_alloc &) {
        return StaticLoadError{document_root, ENOMEM};
    }
}

const StaticAsset *StaticFileStore::find(std::string_view target) const {
    // Query first, then percent-decode, then normalize segments.
    const std::size_t query = target.find('?');
    if (query != std::string_view::npos)
        target = target.substr(0, query);
    if (target.empty() || target.front() != '/')
        return nullptr;
    std::string decoded;
    decoded.reserve(target.size());
    for (std::size_t at = 0; at < target.size();) {
        if (target[at] != '%') {
            decoded.push_back(target[at]);
            ++at;
            continue;
        }
        if (at + 2 >= target.size() || !detail::is_hexdig(target[at + 1]) ||
            !detail::is_hexdig(target[at + 2]))
            return nullptr;
        const auto hex_value = [](char c) {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            return c - 'A' + 10;
        };
        decoded.push_back(
            static_cast<char>((hex_value(target[at + 1]) << 4) | hex_value(target[at + 2])));
        at += 3;
    }
    // Segment normalization: "." and empty segments drop, ".." never resolves
    // because it can only escape the tree, and a segment-less result is the
    // root key.
    std::string key;
    std::size_t begin = 1;
    while (begin <= decoded.size()) {
        const std::size_t end = decoded.find('/', begin);
        const std::size_t length =
            end == std::string_view::npos ? decoded.size() - begin : end - begin;
        const std::string_view segment = std::string_view(decoded).substr(begin, length);
        if (segment == "..")
            return nullptr;
        if (!segment.empty() && segment != ".") {
            if (!key.empty())
                key.push_back('/');
            key.append(segment);
        }
        if (end == std::string_view::npos)
            break;
        begin = end + 1;
    }
    std::string stored_key = "/";
    stored_key += key;
    if (stored_key == "/")
        stored_key = root_index;
    return find_by_key(stored_key);
}

// Binary search for one normalized stored key.
const StaticAsset *StaticFileStore::find_by_key(const std::string &key) const {
    const auto asset = std::lower_bound(assets_.begin(), assets_.end(), key,
                                        [](const StaticAsset &candidate, const std::string &path) {
                                            return candidate.path < path;
                                        });
    if (asset == assets_.end() || asset->path != key)
        return nullptr;
    return &*asset;
}

Response StaticFileStore::serve_get(const StaticAsset &asset, const ParsedMessage &request) {
    const ResponseField &type = content_type_field(asset.media_type);
    const std::size_t total = asset.content.size();
    const auto range = single_range_value(request);
    const auto spec = range ? parse_range(*range) : std::nullopt;
    if (!spec)
        return Response{200, BodyFraming::content_length, asset.content, {&type, 1}, false};
    // An empty representation has no satisfiable byte range (RFC 9110 14.2);
    // otherwise a start at or past the end, or a zero suffix length, is the
    // unsatisfiable 416 answer.
    bool satisfied = false;
    std::size_t first = 0;
    std::size_t last = 0;
    switch (spec->form) {
    case RangeSpec::Form::first_last:
        satisfied = spec->first < total;
        if (satisfied) {
            first = spec->first;
            last = std::min(spec->second, total - 1);
        }
        break;
    case RangeSpec::Form::first_open:
        satisfied = spec->first < total;
        if (satisfied) {
            first = spec->first;
            last = total - 1;
        }
        break;
    case RangeSpec::Form::suffix:
        satisfied = total != 0 && spec->second != 0;
        if (satisfied) {
            first = total > spec->second ? total - spec->second : 0;
            last = total - 1;
        }
        break;
    }
    if (!satisfied)
        return Response{416,   BodyFraming::content_length,     "", {},
                        false, ContentRange{total, false, 0, 0}};
    return Response{206,
                    BodyFraming::content_length,
                    std::string_view(asset.content).substr(first, last - first + 1),
                    {&type, 1},
                    false,
                    ContentRange{total, true, first, last}};
}

} // namespace orange::http
