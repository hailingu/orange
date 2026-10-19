# ADR-0003: One documented local runtime path with a loopback health endpoint

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Not Started
- **Date**: 2026-10-13
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-19T01:31:13Z
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Approve
- **Approval Context Revision [Optional — informational and non-binding]**: Commit 50a0075 (exactly represents the approved document content; not approval evidence)
- **Rejector [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Time [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Evidence [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Reason [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Retired By [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Time [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Evidence [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Reason [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Blocked From [Conditionally Required — Implementation Status is `Blocked`]**: N/A — blocker resolved
- **Blocker And Evidence [Conditionally Required — Implementation Status is `Blocked`]**: Resolved 2026-10-14: ADR-0004 (docs/adr/ADR-0004-engineering-standards.md) was Accepted and completed with both standards in place and cataloged in docs/README.md; the exit criterion's inputs are recorded in ADR-0004's AC-4
- **Blocker Owner [Conditionally Required — Implementation Status is `Blocked`]**: N/A — blocker resolved
- **Blocker Exit Or Recheck Criterion [Conditionally Required — Implementation Status is `Blocked`]**: Satisfied 2026-10-14 per ADR-0004's AC-4 inspection record; Implementation Status returned to the recorded prior status `In Progress` and T-1 restarts under the standards
- **Related [Optional]**: GitHub issue #6 (P3 planning item tracking this candidate); docs/adr/ADR-0001-orange-issue-governance.md governs its closure
- **Architecture Source [Conditionally Required — product demand]**: docs/architecture/ADD-0001-orange-v1-baseline.md — CAND-0001
- **Supersedes [Conditionally Required — this ADR replaces another]**: None
- **Superseded By [Conditionally Required — this ADR is replaced]**: None — not superseded

## Requirement Level Legend [Required]

- **`[Required]`**: The section or field always applies and MUST remain present
  with complete, verifiable content. Use `None — <reason>` only when the
  template explicitly permits an empty result; never leave it blank.
- **`[Conditionally Required — <trigger>]`**: The section or field MUST be
  completed when its stated trigger applies. When the trigger does not apply,
  retain `N/A — <reason>` unless the template explicitly instructs removal or
  retention as inactive future-lifecycle guidance. A missing trigger assessment
  is incomplete content.
- **`[Optional]`**: The section may be removed without affecting acceptance,
  execution, completion, or verification. If retained, it MUST be accurate and
  complete; optional content MUST NOT substitute for required evidence.

Unlabeled fields inside a `[Required]` section are required.

## Context And Problem Statement [Required]

The repository has no runnable product entry point. ADD-0001 CAP-06 requires
that one developer can build and start the backend service and web client
locally through one documented path, with no external services; CON-02 fixes
the compute service as loopback-only with no LAN or cloud mode; and the
CAND-0001 acceptance context requires that a clean local checkout starts both
sides, the client reaches the health endpoint, an ordinary restart preserves
the browser origin, and no LAN listener exists. Every later implementation
slice assumes this skeleton exists.

This ADR turns CAND-0001 of the Current ADD-0001 into one complete,
independently reviewable implementation slice: a minimal C++20 loopback
service exposing a health route and serving a minimal TypeScript-built page
that displays that health, started through one documented command. Developing
orange is itself a study of the C++ and Web technologies it uses (D-03), and
the repository owner directed on 2026-10-13 that this learning purpose makes
a hand-written POSIX socket HTTP server the preferred transport, with a
pinned library available only as a bounded fallback.

## Scope [Required]

In scope:

- `service/` — a C++20 CMake project building one executable that binds
  127.0.0.1 on two documented fixed ports, implements the declared HTTP/1.1
  feature set by hand over POSIX sockets, and serves the client and the
  declared diagnostic routes.
- The declared HTTP/1.1 feature set (owner-directed 2026-10-13 for the
  learning purpose): persistent connections (HTTP/1.1 keep-alive with
  `Connection: close` honored, idle connections closed after a declared
  timeout); request parsing of the request line, headers, and bodies
  (`Content-Length` and chunked transfer decoding); response framing with
  `Content-Length` and a chunked encoder; single byte-range requests on
  static files (HTTP 206 and 416); request-body support through a declared
  `POST /api/echo` diagnostic route that returns the received body using
  chunked framing; `GET /api/health` returning HTTP 200 with body
  `{"status":"ok"}`; static file serving from `web/dist/` at `/` with a
  small MIME map; HTTP 405 for unsupported methods and 404 for unknown
  paths; an 8 KiB header-block cap answering 431, a 1 MiB decoded-content
  cap, and an independent 8 KiB aggregate chunk-metadata cap, the latter
  two answering 413 (CC-8). Each 431/413 response includes `Connection: close`; the
  server completes that rejection response and closes the connection without
  processing a following request on it. The client reconnects for subsequent
  requests. Ordinary valid requests retain keep-alive. A single-threaded `poll()`
  event-loop connection model:
  one thread multiplexes every open socket through readiness-driven reads
  and writes, so several connections (for example, multiple browser tabs)
  are served concurrently without threads or head-of-line blocking.
  Verification covers both one persistent client on a stable connection for
  valid keep-alive cycles and two independent connections where an unfinished
  request on one does not block the other. A client reconnects after a
  431/413 rejection. Idle connections are closed after the declared
  5-second timeout to bound descriptor use.
- TLS: an `https://127.0.0.1:8443` listener terminated through OpenSSL
  (exactly pinned; crypto primitives are never hand-rolled — only the
  protocol layer is), with a self-signed certificate generated on first run
  into a gitignored location and reused across restarts.
- `web/` — a minimal TypeScript client: one page and a `status` module that
  fetches `/api/health` and renders the result, compiled by a pinned `tsc`
  toolchain to `web/dist/`. A successful response visibly renders `ok`;
  a failed request replaces loading with a connection-error message and
  removes any success state within two seconds of the failure. This
  failure-feedback budget is proposed for this status page by AC-8.
- `vcpkg.json` — manifest pinning the OpenSSL dependency.
- `scripts/dev.sh` — the one documented local path: generate the certificate
  if absent, build both sides, start both listeners, and print the stable
  origin URL.
- Root `README.md` — the documented run instructions, including the toolchain
  bootstrap, the fixed origins, and the one-time browser acceptance of the
  self-signed certificate.

Out of scope:

- Any domain capability: expression evaluation, matrix computation, content,
  exercises, progress, or tutoring.
- Compute endpoints beyond the health and echo diagnostic routes,
  domain-payload limits, or overload-rejection policy (owned by the domain
  candidates).
- HTTP/2 and later, WebSockets, response compression, conditional requests
  (`ETag`, `If-Modified-Since`, `If-Range`), and multi-range multipart
  responses — each requires an explicit revision of this ADR's declared
  feature set.
- Client tooling beyond `tsc` (bundler, dev server, framework choices belong
  to the presentation candidates).
- CI, packaging, deployment tooling, and any remote or LAN exposure.

The authorized fallback, `cpp-httplib` exactly pinned, is added only when the
fallback trigger recorded in Decision fires; it is not part of the default
build.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Educational readability versus implementation risk in the HTTP layer | A hand-rolled server maximizes learning exposure (the project's stated purpose, D-03) but carries protocol-defect risk; a library hides exactly what is being studied | Hand-write the POSIX HTTP server within a declared minimal HTTP/1.1 subset, test-first, with cpp-httplib as an exactly pinned fallback that fires only on the recorded trigger |
| TN-2 | One fixed origin versus flexible ports | A floating port breaks the stable-origin requirement and localStorage semantics later | Fix the documented default port and document conflict handling as an error with a coded message |

### Constraints [Required]

- C++20 backend and TypeScript frontend are fixed by D-03 and CON-01.
- The service binds loopback only; no LAN or cloud listener (CON-02).
- Ordinary restart must preserve the browser origin (CAND-0001).
- Dependencies are exactly pinned; development is test-first (AGENTS.md).
- No domain endpoint semantics may sneak into this slice.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

| ID | Question | Owner | Due | Status | Resolution and Evidence |
| --- | --- | --- | --- | --- | --- |
| Q-1 | Which HTTP layer serves the loopback service? | @hailingu | 2026-10-13 | Resolved | Owner direction 2026-10-13: hand-written POSIX socket HTTP server, because learning the platform is a stated product purpose (D-03); cpp-httplib stays the exactly pinned fallback under Decision's recorded trigger |
| Q-2 | Which ports are the stable origins? | @hailingu | 2026-10-13 | Resolved | `https://127.0.0.1:8443` is the documented stable origin (self-signed certificate, accepted once per browser profile); `http://127.0.0.1:8080` remains as the plain comparison endpoint used by the test suite; a port conflict produces a coded startup error rather than a silent alternative port |
| Q-3 | Which HTTP features belong to the learning subset? | @hailingu | 2026-10-13 | Resolved | Owner direction 2026-10-13: keep-alive, chunked transfer (both directions), single-range requests, TLS, and request-body parsing are all in; HTTP/2, WebSockets, compression, conditional requests, and multi-range responses stay out until this ADR is revised |
| Q-4 | After a 413/431 rejection, reuse the connection or reconnect? | @hailingu | 2026-10-15 | Resolved | Owner answer in this session: respond and close the connection; subsequent requests use a new connection. This changes the previous resource-risk acceptance and requires reapproval |

## Decision Drivers [Required]

1. **Skeleton first**: every later candidate depends on a runnable local origin.
2. **Loopback-only by construction**: binding behavior must make LAN exposure impossible, not merely undocumented.
3. **Learning the platform (owner-directed)**: the transport is hand-written within a declared minimal subset; libraries are fallbacks, not defaults.
4. **Exactly pinned dependencies where libraries are used**: reproducible clean-checkout builds.

## Options Considered [Required]

### Option: cpp-httplib through a vcpkg manifest

One pinned header-only library provides the loopback HTTP server, static file
serving, and routing used by the health endpoint.

Pros:

- Minimal surface to read and learn; no framework ceremony.
- vcpkg manifest gives an exactly pinned, reproducible dependency.

Cons:

- Not an industrial framework; later high-concurrency needs (none in V1's
  single-user scope) would require revisiting.

### Option: Boost.Beast

Full-featured asynchronous HTTP on Asio.

Pros:

- Industrial capability and ecosystem.

Cons:

- Heavy dependency and steep reading cost for a single-user local skeleton;
  misaligned with the study-goal driver.

### Option: Hand-rolled POSIX socket HTTP server

Implement HTTP/1.1 parsing directly over sockets.

Pros:

- Maximum learning exposure to the wire protocol.

Cons:

- Spends this slice on HTTP plumbing instead of the orange skeleton; higher
  defect risk for no capability gain.

## Decision [Required]

**Selected option**: Hand-rolled POSIX socket HTTP server.

**Rationale**: learning the platform is a stated product purpose of orange
(D-03), and the repository owner directed on 2026-10-13 that the transport
layer be hand-written for that purpose, covering keep-alive, chunked
transfer, range requests, request bodies, and a TLS listener, with the
crypto primitives themselves taken from the approved OpenSSL library rather
than hand-rolled. The declared feature set bounds the protocol surface to
what this slice and its tests exercise, the test-first plan catches protocol
defects at the smallest scale, and the exactly pinned cpp-httplib fallback
keeps the slice deliverable even if the hand-written path hits its recorded
trigger.

**Fallback trigger**: adopt the cpp-httplib fallback within this ADR's
accepted scope — recording the evidence in the Change Log and touching no
other decision content — when either (a) the repository owner directs the
fallback, or (b) the hand-written server cannot make its T-1 or T-2 suite
green without protocol machinery beyond the declared feature set. The
default outcome remains the hand-written server.

### Consequences [Required]

Positive:

- The transport layer is fully readable and modifiable by the learner, and
  the HTTP wire protocol — persistent connections, chunked transfer, range
  serving, and TLS termination — is studied directly (the owner's stated
  purpose).
- A clean checkout reaches a running local origin through one documented path
  with the only runtime library dependency being the approved crypto library.

Negative:

- Hand-written HTTP over a broad feature set carries real protocol-defect
  risk a vetted library would not, and the declared feature set must grow
  deliberately in later slices.
- The readiness-driven state machines are structurally more intricate than
  an iterative loop; protocol defects now hide in event ordering as well as
  parsing.

Mitigations:

- The feature set is declared in this ADR; anything beyond it (HTTP/2,
  WebSockets, compression, conditional requests, multi-range) requires an
  explicit revision here rather than quiet scope creep.
- The test-first suites cover parsing, keep-alive cycles, chunked decode and
  encode, range responses, both caps, the idle timeout, loopback binding on
  both listeners, and one persistent multi-cycle client before the server
  is trusted.
- The `poll()` loop serves additional connections (multiple browser tabs,
  as anticipated by ADD-0001 INV-16 and GO-02) without head-of-line
  blocking. AC-7 verifies progress on a second connection while the first
  request is unfinished, in addition to the persistent-client cycles.
- The exactly pinned cpp-httplib fallback is pre-authorized with a binary
  trigger, so adopting it does not reopen the decision.

## Implementation Plan [Required]

**Complete task outcome**: One documented local path builds and starts the
TypeScript client and the loopback C++20 service implementing the declared
HTTP/1.1 feature set at the stable origin `https://127.0.0.1:8443` (with the
plain `http://127.0.0.1:8080` comparison endpoint), where the page renders,
`/api/health` answers `{"status":"ok"}` over both listeners, keep-alive,
chunked, range, and echo-body behaviors work as declared, restart preserves
the origins, and no non-loopback listener exists.

**Primary implementation boundary**: Runtime assembly.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Hand-written HTTP/1.1 transport core, developed test-first | `service/` CMake project and POSIX socket code: single-threaded `poll()` event loop with readiness-driven read/write state machines, keep-alive connection lifecycle with the declared 5-second idle timeout, request-line/header/body parsing (Content-Length and chunked decode), response framing (Content-Length and chunked encode), the 8 KiB header cap (431), 1 MiB decoded-content cap and independent 8 KiB aggregate chunk-metadata cap (both 413, CC-8), each answered with `Connection: close` followed by connection closure; CTest unit suite for every behavior, with one persistent client completing multiple request-response cycles on a single stable connection as the exercised client model for valid requests, reconnecting after 431/413 | Not Started | Historical evidence under the invalidated approval — reverify after reapproval. Body and incremental decoder implemented in `f558e62`; see Request-Body And Incremental Decoder Evidence. Task 1 scaffold landed 2026-10-14 in this change: `service/CMakeLists.txt`, `service/tests/http_message_tests.cpp` (in-tree `CheckReporter` harness), and root `.clang-format`; harness wiring observed deliberately failing then passing through the verification trio (Apple clang 21.0.0, CMake 4.2.3); task 2 scaffold 2026-10-14: the `orange_http` library (namespace `orange::http`, `parse_request` declaration with a placeholder body) is linked into the suite wiring-only, asserting no behavior; first behavioral contract 2026-10-14: empty-input rejection landed failing-first in commit 12b0e43 (`parse_request("")` must yield `std::nullopt`, observed red at `http_message_tests.cpp:65` with ctest exit 8) and turned green with the minimal rejection implementation in the same change as this evidence update — suite 1/1 passing (ctest exit 0), clang-format clean, no build warnings; request-line contract 2026-10-14: commit 3faf3d9 asserts `method SP request-target SP HTTP-version CRLF` with three non-empty tokens parsed into owned `Request::method/target/version` fields plus seven malformed-line rejections (observed red: 10 of 11 new checks failing, ctest exit 8), turned green by the request-line parser in the same change as this evidence update (suite 1/1 passing, clang-format clean, no build warnings; bytes after the request line remain unconsumed pending the header task); terminator-search swap 2026-10-14: characterization checks landed first in commit 211c62b pinning that tokens cannot cross the first CRLF and that a bare '\r' stays inside its token, then the line-end location became `find('\n')` plus one-byte look-back in the following commit — behavior-equivalent by a 13-input cross-check (0 mismatches, this session) and the 15-check suite green throughout; rationale: a single-char search has no restart loop, eliminating the measured ~23 µs memchr-restart degradation on CR-dense input at the 8 KiB cap; error-classifying contract 2026-10-14: commit e739c68 lands the `std::variant<ParsedRequest, ParseError>` suite (incomplete/malformed/to_large plus consumed bytes, RFC 9112 2.2/3 expectations; observed red 23 of 28) and the implementing commit delivers it — every line search bounded by `header_block_cap` so an unfinished line can never force unbounded rescans, leading empty lines ignored, bare CR rejected, method validated as tchar, version as `HTTP/DIGIT.DIGIT` with case-sensitive HTTP-name, target free of SP/CTL bytes; suite 28/28 green, clang-format clean, no build warnings; owner-review RFC 9112 conformance 2026-10-14: commit db2856f lands the red suite (observed red 14 of 48 checks, ctest exit 8) and the implementing commit turns it green — a request line that cannot end inside `header_block_cap` is classified per RFC 9112 3 as `method_too_long` (501) or `target_too_long` (414 MUST), blamed on the longer field, while a grammar-breaking prefix, a bare CR, or empty lines alone exhausting the budget stay `malformed`; the request-line contract no longer emits the 431-mapped `too_large` (431 belongs to header parsing); target octets follow the URI character set with well-formed percent-encoding (RFC 9112 3.2, RFC 3986 2.1-2.3); `parse_request` is `[[nodiscard]]`; test defects fixed: the NUL fixture encoded VT (0x0B) and `parsed_of` dangled into a temporary variant (ASan stack-use-after-scope); suite 48/48 green under the verification trio and under ASan/UBSan, a 200,000-input seeded sanitizer fuzz run clean, clang-format clean, no build warnings; lookup-table refactor 2026-10-14: after benchmarking the external SIMD/LUT contribution (three-way variant cross-check, 16 inputs 0 mismatches), the byte classifiers moved from per-call `string_view::find` set searches to one compile-time 256-entry bitmask table and the percent-encoding check indexes directly instead of slicing a view per escape — measured at -O2 on the host arm64 toolchain: short valid 31.4→23.9 ns, 200-byte punctuation target 419→126 ns, 8 KiB overflow classification 4651→2559 ns, CR-dense unchanged; behavior identical (cross-check plus the 48-check suite green). The contribution's zero-copy `string_view` Request variant measured faster still (14.3 ns short valid) and stays out under the owned-copy contract; its hand-AVX2 branch is inert on the declared arm64 toolchain (no Rosetta on this host), where `std::find`'s NEON-backed memchr remains the search engine; header-block contract 2026-10-14: commit f9e6364 asserts the header-block suite (zero-field heads completing through the terminating empty line, field-line collection with OWS trimming, order/duplicate/case preservation, obs-text values, 431 boundary and grammar-broken overflow tails, obs-fold/colon/CTL rejections, incomplete partials; observed red 36 of 84 checks, ctest exit 8) and the implementing commit turns it green — the block after a parsed request line parses as CRLF-terminated `field-name: OWS field-value OWS` lines ended by one empty line (RFC 9112 5) into owned `Request::headers` entries, names are tokens so RFC 9112 5.1's whitespace-before-colon rule falls out of the token grammar, values are OWS-trimmed field-content with obs-text allowed (RFC 9110 5.5), obs-fold is rejected (RFC 9112 5.2), and the block spends only the head budget remaining after the request line (`header_block_cap - ParsedRequest::consumed`), owning the 431 `headers_too_long` outcome (RFC 6585 5) — discharging the recorded later-task item (1) and confirming the shared request-line/header budget against Scope; suite 84/84 green via the verification trio (Apple clang 21.0.0, CMake 4.2.3), clean under ASan/UBSan, clang-format clean, no build warnings |
| T-2 | Serving features and TLS listener, developed test-first | Route dispatch, `GET /api/health`, `POST /api/echo` with chunked responses, static serving with MIME map and single-range 206/416, 405/404, loopback bind on both listeners, and the OpenSSL-terminated `https://127.0.0.1:8443` listener with first-run self-signed certificate; CTest integration suite including AC-7's two-connection progress case plus `curl`-based end-to-end checks over both plaintext and TLS | Not Started | Pending |
| T-3 | TypeScript status page, one documented run path, and verification evidence | `web/` npm project with pinned `typescript`, `src/status.ts` and its `node --test` suite, `tsc` build to `web/dist/`, `scripts/dev.sh` with first-run certificate generation, root `README.md` run section, repository-root verification runs of the acceptance checks, including AC-8's real-browser health success and connection-error feedback within two seconds of request failure | Not Started | Pending |

**Affected paths**: `service/**`, `web/**`, `vcpkg.json`, `scripts/dev.sh`,
`README.md` (certificate material is generated into a gitignored location and
never committed)

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| `service/src/http_server.cpp` | `serve_loopback(port, handler)` | N/A — stable symbol or anchor is sufficient | Owns the single-threaded `poll()` event loop and the per-connection readiness-driven read/write state machines with the 5-second idle timeout | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `service/src/http_messages.cpp` | `orange::http::parse_request()` | N/A — stable symbol or anchor is sufficient | Head-only owned parsing with the 8 KiB cap; shared grammar also validates incremental head lines | `f558e62` — head parser implemented; full socket integration pending |
| `service/src/http_request_decoder.cpp` | `orange::http::RequestDecoder::feed()`, `orange::http::RequestDecoder::reset()`, `orange::http::parse_message()` | N/A — stable symbols are sufficient | Own fragment progress, body decoding, pipeline boundaries and both CC-8 body budgets; terminal errors await connection-level rejection | HEAD `1f87646` plus owner-authorized 2026-10-16 workspace repair; source content IDs in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json; network error-response closure pending |
| `service/src/http_request_decoder.hpp`, `service/src/http_messages.hpp` | `orange::http::chunk_metadata_cap`, `orange::http::ParseError::chunk_metadata_too_long` | N/A — stable symbols are sufficient | Public 8192-byte aggregate budget and distinct HTTP-413-mapped error; separate from decoded-content capacity | HEAD `1f87646` plus owner-authorized 2026-10-16 workspace repair; content IDs in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |
| `service/src/http_body_framing.cpp`, `service/src/http_chunk_line.cpp` | `orange::http::detail::body_framing()`, `orange::http::detail::ChunkLine::feed()` | N/A — stable symbols are sufficient | Resolve CL/TE once and validate chunk metadata with finite storage | `f558e62` |
| `service/src/http_response.cpp` | `orange::http::write_response()` | N/A — stable symbol is sufficient | Planned Content-Length/chunked response framing, including `Connection: close` rejection responses | Planned revision 2026-10-15 — replace with the implementing commit before completion |
| `service/src/tls.cpp` | `serve_tls()` | N/A — stable symbol or anchor is sufficient | Terminates TLS through the pinned OpenSSL library on the https listener using the generated self-signed certificate; crypto primitives are never hand-rolled | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `service/src/routes.cpp` | `handle_request()` | N/A — stable symbol or anchor is sufficient | Dispatches health, echo, static-with-single-range (206/416), and 405/404 outcomes | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `web/src/status.ts` | `renderServiceStatus()` | N/A — stable symbol or anchor is sufficient | Fetches `/api/health` and renders the observed status with a fail-fast connection-error state | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `scripts/dev.sh` | the script's documented command contract | N/A — stable symbol or anchor is sufficient | The single documented path: generates the certificate if absent, builds service and web, starts both listeners, prints the stable origin | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |

**Migration and rollback strategy [Conditionally Required — this replaces or
changes existing behavior]**: N/A — greenfield slice; nothing existing is
replaced. Rollback is deleting the added paths.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — the preexisting head-test-main exception was removed on 2026-10-15
under Accepted `service/docs/adr/ADR-0001-http-message-test-decomposition.md`.
The original `main` measured 307 code / 329 physical lines; its replacement
dispatches the same cases in 24 code / physical lines. The 14 extracted
functions are at most 39 code lines and the file is 718 physical lines.
Source content ID: `4f576fd1bbb42b3d2130ab971d7ceda71720977f` (baseline
`1f876464862c69c5a54424d91fbfd6937983e23c`). Both existing CTest targets
passed before and after the refactor, without compiler warnings. The service
maintenance record contains the case inventory and verification commands.
This is exception-removal evidence only: this runtime record remains
Proposed / Not Started with reapproval pending and its ADD link unchanged;
no socket or browser acceptance is claimed.

### Request-Head Optimization Evidence [Optional]

The 2026-10-14 T-1 optimization preserves the `parse_request()` signature,
owned result fields, received header order/case/duplicates, grammar outcomes,
consumed-byte count, and shared 8 KiB head budget. It defers owned copies until
validation completes, records compact call-local field offsets to avoid a
second delimiter/OWS scan, reserves the validated field count once, and checks
the version and method before scanning a potentially long target.

- Baseline source: commit `e8f75a7cce3576c6c9a51ee9b5093b4e2d339bf3`,
  `service/src/http_messages.cpp` blob
  `28d8bcfc6fb3fe8c5b0d822837b45f3c7677a259`.
- Red evidence: commit `ba7da00` adds allocation checks first; the configured
  CMake/CTest trio reports five expected failures (CTest exit 8) for incomplete,
  malformed, and oversized heads allocating copies, and short-field vector
  growth. Existing grammar checks remain green.
- Green source: `service/src/http_messages.cpp` blob
  `99df77b67b72b9929361d095b1c816791278ccd1`, stable symbols
  `orange::http::parse_request`, `validate_header_block`, and `copy_header_block`;
  `service/tests/http_message_tests.cpp` blob
  `a81cdb74e18f4e36b1bc86850240a4a3c6ffaf87`, stable checks
  `check_parse_allocations` and `check_deferred_field_copies`. The latter also
  exercises input-buffer reuse and 2,043 shortest field lines ending exactly
  at the shared cap. No public borrowed view is introduced.
- Host/toolchain: Apple clang 21.0.0 (`clang-2100.3.34.2`),
  `arm64-apple-darwin27.0.0`, CMake 4.2.3. Timing uses separately compiled
  baseline/current parsers with `-std=c++20 -O2 -Wall -Wextra`, alternating
  before/after order over seven rounds and reporting the median. Input creation
  and diagnostics are outside timing; result destruction is included. Ordinary
  heap allocations are counted in a separate instrumented executable.

| Synthetic fixture / delivery | Before | After | Heap allocations before → after |
| --- | --- | --- | --- |
| 68-byte POST echo head with Host and Content-Length | 105.1 ns | 79.1 ns | 2 → 1 |
| 152-byte head, 16 fields `X-N: a` in numeric order | 316.5 ns | 254.7 ns | 5 → 1 |
| 1,188-byte head, 128 fields `X-N: a` in numeric order | 1,999.0 ns | 1,671.0 ns | 8 → 1 |
| Same 128-field head, parse each accumulated 64-byte prefix (19 calls) | 21,766.9 ns | 11,227.5 ns | 136 → 1 |
| Same 128-field head, parse every accumulated byte prefix (1,188 calls) | 1,292,945.4 ns | 621,840.0 ns | 8,219 → 1 |
| 16 fields with 128-byte values, terminating empty line missing | 1,395.0 ns | 937.0 ns | 21 → 0 |
| One padded field making the complete head exactly 8 KiB | 2,437.3 ns | 2,426.7 ns | 2 → 2 |
| Invalid method with an 8,001-byte target | 2,541.9 ns | 460.4 ns | 0 → 0 |

These are local synthetic parser measurements, not network throughput or p99
latency. Incomplete calls still rescan the accumulated input; cross-call scan
progress belongs to the later connection state machine. On this host the
scratch array is 16 KiB of call-local stack storage, bounded by the existing
head budget and the four-byte shortest field-line grammar; no scratch heap
allocation or persistent per-connection buffer is added.

Verification evidence (all final commands exit 0):

- `cmake -S service -B service/build`, `cmake --build service/build`, and
  `ctest --test-dir service/build --output-on-failure`: 1/1 suite passes, no
  compiler warnings.
- Direct suite compile/run with `-std=c++20 -O1 -g -Wall -Wextra
  -fsanitize=address,undefined -fno-omit-frame-pointer`: no sanitizer findings.
- A disposable differential harness compiled under the same sanitizer flags
  compares every parsed field, consumed count, and error against the baseline:
  seed `0x4f52414e`, 106,898 cases, zero mismatches. Cases include generated
  valid heads, truncated prefixes, single-byte mutations, trailing bodies,
  arbitrary bytes, every prefix of dense and padded 8 KiB heads, and all 256
  possible bytes at their last position.
- `clang-format --dry-run --Werror service/src/http_messages.cpp
  service/src/http_messages.hpp service/tests/http_message_tests.cpp` and
  `git diff --check`: clean.
- `npm test --prefix tools/governance-validator` and
  `npm run validate --prefix tools/governance-validator`: pass.

Structured review confirms bounded offsets and initialized-entry reads,
unchanged grammar/error precedence and ownership, no new dependency or public
API, and no new executable unit above the 60-line review threshold. The
pre-existing monolithic test `main()` is retained with two helper calls;
decomposing its existing assertions is outside this parser optimization. The
scratch array's additional stack footprint is the measured implementation
trade-off. Disposable compiler/test outputs are cleaned before task completion;
the pre-existing build-directory metadata is preserved. This evidence does not
complete T-1 or AC-1: bodies, response framing, and the event loop remain
unfinished.

### Grouped Field-Value Validation Evidence [Optional]

The second 2026-10-14 T-1 optimization is a behavior-preserving refactor of
the existing red-green header-field contract. It checks complete 16-octet
groups using independent unsigned-byte comparisons, rejects between groups,
and checks short values and the remainder through the existing classification
table. The overflow-tail classifier reuses this same octet validator after
allowing its existing trailing-CR exception. Input bounds, owned copies,
allocation costs, parser signature, and error precedence remain unchanged.

- Baseline: commit `164ae61852f56ef23a6d2d672409b0041125ac19`, parser blob
  `99df77b67b72b9929361d095b1c816791278ccd1`.
- Supplemental characterization tests landed first in commit `39c5bc9`:
  `check_field_value_octets` covers all 256 byte values at eight interior
  positions; `check_field_value_boundaries` covers lengths 0 through 129 at
  selected boundaries, 16 leading-OWS widths, and DEL at every position of
  those nonempty values. They pass against the unchanged baseline before the
  refactor. A disposable mutation skipping the grouped validator's scalar
  remainder triggers 49 failures in the latter check (process exit 1).
- Final parser blob: `8112d3a0cf3a0f71ddc76a2a520808514bca5a79`, stable
  symbols `is_valid_field_value_groups`, `is_valid_field_value`, and
  `classify_unterminated_header`; final test blob:
  `6fb4de0e2b1afdc3d1c33d71eaa347685c9128e5`.
- Apple clang 21.0.0 (`clang-2100.3.34.2`), arm64-apple-darwin27.0.0,
  `-std=c++20 -O2 -Wall -Wextra`, separate compilation, no LTO. Assembly
  inspection confirms bounded 16-byte loads, vector comparisons, and an
  `umaxv` reduction. No architecture intrinsics or new dependency are added.
- A disposable A/B benchmark alternates execution order across seven rounds
  and reports medians, excluding input construction and including result
  destruction. Its predeclared experimental long-value budget (candidate
  median at most 70% of baseline) fails before the change (2,443.8 versus
  2,468.4 ns, exit 1) and passes for the final source (486.8 versus 2,157.8 ns,
  exit 0). This experiment is not a timing assertion in CTest or a new public
  latency contract.

| Synthetic fixture / delivery | Baseline | Final | Heap allocations baseline → final |
| --- | --- | --- | --- |
| 18-byte head without fields | 25.5 ns | 29.7 ns | 0 → 0 |
| 68-byte POST echo head | 72.2 ns | 73.1 ns | 1 → 1 |
| 1,188-byte head with 128 short fields | 1,544.7 ns | 1,596.6 ns | 1 → 1 |
| Same short-field head, accumulated 64-byte prefixes | 10,033.6 ns | 10,596.8 ns | 1 → 1 |
| 16 fields with 128-byte values | 1,170.2 ns | 539.0 ns | 17 → 17 |
| Same long fields without the terminating empty line | 828.5 ns | 219.1 ns | 0 → 0 |
| One padded field making the complete head exactly 8 KiB | 2,157.8 ns | 486.8 ns | 2 → 2 |
| Same padded field, DEL at the end of its value | 2,025.3 ns | 349.2 ns | 0 → 0 |
| Grammar-valid unterminated field tail reaching the 8 KiB cap | 1,988.2 ns | 342.6 ns | 0 → 0 |

These remain local synthetic parser measurements. Short-field scenarios carry
a small additional dispatch cost, and the no-field fixture adds 4.2 ns in this
run; the retained change targets long values and oversized tails. Trials of
direct vector-element filling and a no-field shortcut were discarded after
comparison because they increased costs in other short-field scenarios.
The stateless parser still repeats scans across fragmented deliveries.

Final verification: the configured CMake/CTest trio and direct `-O2` suite
pass; the direct ASan/UBSan suite reports no findings. Both an optimized and a
sanitizer differential harness compare
every returned field, error, and consumed count with the baseline for 151,913
cases (seed `0x4F52414E`, every prefix of the benchmark fixtures, 50,000 seeded
mutations/truncations/body suffixes, and 20,000 arbitrary byte strings), with
zero mismatches. A separate allocation-count executable confirms unchanged
counts for all 17 fixtures/delivery patterns. clang-format check mode and
`git diff --check` pass. Both configured governance-validator commands pass.

Structured review confirms that the unsigned range predicate admits exactly
HTAB, SP/VCHAR, and obs-text; every vectorized load stays inside the supplied
view, and the scalar remainder remains mandatory. New executable units remain
below the 60-line review threshold, the production file remains below 600
lines, and the existing test-main decomposition debt remains outside this
refactor. No acceptance check, risk-matrix requirement, approved scope, or
status is changed; the full transport task remains unfinished. Disposable
compiler/test outputs are cleaned, preserving existing build metadata.

### Grouped OWS Trimming Evidence [Optional]

The third 2026-10-14 T-1 optimization preserves the existing OWS-trimming
contract. `orange::http`'s private `trim_ows_groups` skips a complete
16-octet group only when every byte is SP or HTAB, then locates each edge
with scalar checks. `trim_ows` selects this path only for views of at least
32 bytes with at least two adjacent OWS bytes at an edge; short values and
single-space edges retain the scalar path. Interior whitespace, owned
copies, error classification, consumed bytes, and the shared cap are
unchanged. This is a refactor within the existing T-1 implementation, not
a new parser contract or acceptance check.

- Baseline: commit `467257bb948d863bd4c21ad2fe63789b3e4ed3bf`, parser blob
  `8112d3a0cf3a0f71ddc76a2a520808514bca5a79`.
- Characterization tests landed first in commit `a8a9502`, test blob
  `a2f2c65d3e6c191b3a5c356224da3fe191713bb8`: `check_ows_trimming` covers
  SP, HTAB, and alternating padding around an owned value with interior
  whitespace and obs-text, empty values, body boundaries, allocations,
  and an exactly 8 KiB head; `check_ows_edge_octets` covers all 256 octets
  at nine trimming widths. Both pass against the unchanged baseline.
- Final parser blob: `1d037e05790ee0f56024466b4032bb5ed0d12465`; stable
  implementation symbols: private `is_ows`, `trim_ows_groups`, and `trim_ows`.
- Apple clang 21.0.0 (`clang-2100.3.34.2`), arm64-apple-darwin27.0.0,
  `-std=c++20 -O2 -Wall -Wextra`, separate compilation without LTO.
  Assembly inspection confirms full 16-byte loads, vector equality checks,
  and reductions guarded by the remaining view length at both edges.
- A disposable A/B benchmark alternates baseline/candidate order across
  seven rounds and reports medians. Fixtures and diagnostics are outside
  the timing; returned owned values are destroyed inside it. Its
  predeclared experimental budget requires each of five 8 KiB OWS cases
  to take at most 50% of baseline time, and each of the other 13 cases to
  take at most 110%. Comparing the unchanged implementation with itself
  first fails exactly the five OWS budgets (ratios 0.994 through 1.006,
  process exit 1). The final source passes all 18 budgets (exit 0).
  These budgets are local experimental criteria, not CI timing assertions
  or public latency guarantees.

| Synthetic fixture / delivery | Baseline | Final |
| --- | --- | --- |
| 18-byte head without fields | 22.0 ns | 22.3 ns |
| 63-byte POST echo head | 72.2 ns | 73.0 ns |
| One ordinary value making the head exactly 8 KiB | 513.0 ns | 526.1 ns |
| Same size, leading OWS around a one-byte value | 2,198.5 ns | 372.4 ns |
| Same size, trailing OWS around a one-byte value | 2,165.8 ns | 374.2 ns |
| Same size, all SP | 2,143.8 ns | 386.5 ns |
| Same size, all HTAB | 2,162.6 ns | 372.4 ns |
| Same size, alternating SP and HTAB | 2,159.3 ns | 379.2 ns |
| 16 short fields | 239.9 ns | 242.7 ns |
| 128 short fields | 1,634.1 ns | 1,609.4 ns |
| Same short fields, accumulated 64-byte prefixes | 10,864.4 ns | 10,975.0 ns |
| 16 fields with 128-byte values | 558.9 ns | 558.3 ns |
| Same long fields without the terminating empty line | 230.3 ns | 236.2 ns |
| 2,043 empty fields fitting 8 KiB | 20,525.9 ns | 20,847.4 ns |
| Same empty fields, accumulated 64-byte prefixes | 836,133.3 ns | 868,134.4 ns |
| One short OWS-padded value | 46.7 ns | 46.9 ns |
| One value with 64 SP before and 64 HTAB after | 92.4 ns | 51.9 ns |
| 8 KiB head with DEL after long OWS | 2,142.3 ns | 346.3 ns |

Long OWS cases decrease by 82% through 83%; other scenarios remain within
the experimental 10% budget. These are local synthetic parser results,
not end-to-end service measurements. The stateless parser still repeats
scans across fragmented deliveries.

Verification: the configured CMake/CTest commands, direct `-O2` suite, and
direct ASan/UBSan suite pass without warnings or sanitizer findings.
Optimized and sanitizer differential harnesses independently compare all
returned fields, error values, and consumed counts for 150,861 cases
(seed `0x4F575333`, every prefix of the 18 fixtures, 50,000 seeded
mutations/truncations/body suffixes, and 20,000 arbitrary byte strings),
with zero mismatches. A disposable mutation that removes one extra byte
after a full trailing-OWS group triggers 512 behavioral failures (exit 1).
The allocation regression checks, clang-format check mode, and
`git diff --check` pass; both configured governance-validator commands pass.

Structured review confirms that no group reads beyond its view, all-OWS
values retain a valid empty slice, and scalar checks preserve each first
non-OWS octet. New executable units remain below the 60-line review
threshold; production and test files remain below their review thresholds.
The existing test-main decomposition debt remains outside this refactor.
No dependency, public API, accepted scope, contract traceability, risk-matrix
requirement, acceptance check, or status changes. The full transport task
remains unfinished. Disposable verification outputs are deleted, preserving
existing build metadata; no reusable artifact is retained or promoted.

### Request-Body And Incremental Decoder Evidence [Optional]

Historical implementation evidence from 2026-10-15, completed under the prior
Accepted decision before its connection-rejection policy was revised. This
section does not satisfy revised acceptance checks until they are rerun after
reapproval. T-1 remains unfinished: response encoding and real socket/event-loop
integration are absent, as are T-2 and T-3. Earlier optimization sections remain
historical experiment records; consolidate their summary when T-1 is complete.

- Red `2f11b19`: the three fixed-length body checks fail as expected (CTest
  exit 8); minimal implementation `f4f11b9` makes both suites pass.
- Red `8cd704a`: framing, chunked and fragment tests produce 668 expected
  failures against the initial implementation (CTest exit 8).
- Red `6006103`: additional trailing-BWS and allocation-failure regressions
  are observed failing before their fixes (five failures, CTest exit 8).
- Green implementation: `f558e62ae4fcf8c48afe20c2995f17a74e1fa6fd`;
  `service/src/http_request_decoder.cpp` blob
  `8b4a182fbfe47fa8aaabc55342019094292193be`,
  `service/tests/http_request_decoder_tests.cpp` blob
  `a8346ec88cf888e78021319b007f20ec9b16cc4f`, and shared head implementation
  `service/src/http_messages.cpp` blob
  `12587f8ab9da9a74d3d314d116490c3f3ea29dfb`.

`parse_request` retains its head-only interface and owned fields. The new
`parse_message` completes an owned head/body message; `RequestDecoder::feed`
accepts only new fragments and reports the portion consumed from that fragment.
`ParsedMessage::consumed` includes chunk framing and discarded trailers and
excludes subsequent pipelined input. A decoder retains terminal errors until
explicit reset, and completed decoders consume no further input until reset.

Framing grammar references: [RFC 9112 section 6](https://www.rfc-editor.org/rfc/rfc9112.html#section-6)
and [section 7.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1).

The implementation resolves framing once: case-insensitive CL/TE field names,
identical normalized decimal CL values across duplicate/list fields, no CL/TE
ambiguity, one final chunked coding, and 501-mapped unsupported preceding
codings. It accepts binary body octets, validates chunk extensions including
quoted-pairs and BWS, discards validated trailers without merging them into the
head, and checks decimal/hexadecimal sizes without integer overflow. The
1 MiB cap counts decoded content across all chunks. At this historical
`f558e62` revision, no additional metadata length policy was introduced
(superseded by CC-8 and the 2026-10-16 section below): extensions and trailers retain finite grammar
state rather than their text. A future event loop still owns deadlines and
connection closure. Allocation failure returns a sticky 500-mapped
`resource_exhausted` value, with no reusable input boundary.

Fragmented heads retain at most the 8 KiB head payload and saved line progress;
complete lines are validated once as they arrive. The final bounded head parse
constructs owned fields once. Complete heads in the first feed use the original
parser directly. Shared request-line/field-line grammar and octet predicates
prevent separate incremental grammar definitions. Body content is appended in
bulk and chunk/trailer metadata is processed once per octet. Thus decoding work
is linear in wire bytes; string capacity overhead is implementation-dependent
and the decoded-content cap is not a claim of a 1 MiB total memory footprint.

Configured verification from the repository root:

- `cmake -S service -B service/build` — exit 0.
- `cmake --build service/build` — exit 0, zero warnings.
- `ctest --test-dir service/build --output-on-failure` — exit 0, 2/2 suites.
- Direct `-O2 -Wall -Wextra` and ASan/UBSan runs of the decoder suite — exit 0,
  5,105 checks, zero failures and no sanitizer findings; the existing head
  suite also passes under ASan/UBSan.
- Head comparison against `e0ebbb1`, checking all owned fields, errors and
  consumed counts: 67,430 cases, seed `0x48454144`, zero mismatches.
- Sanitized one-shot/fragment comparison: 108,416 cases, seed `0x424F4459`,
  all prefixes of six fixtures plus 100,000 mutations/arbitrary byte strings,
  zero mismatches and no sanitizer findings. This checks transport-split
  equivalence rather than supplying an independent framing oracle; the suite
  separately uses 400 seeded fixed/chunked fixtures with known binary content.

Local arm64 Apple clang 21 `-O2` measurements below compare two calling models
using the same final source: repeatedly parse the cumulative prefix, or feed
only new bytes to one decoder. Seven alternating samples, median microseconds
per complete message, `steady_clock`, no LTO, loop counts respectively
100/100/100/30/100/1000/15/15. Long-head fixtures contain one field; dense-head
fixtures contain 64 fields with 100-byte values. Bodies contain 256 KiB of `x`.
The head baseline calls `parse_request`; body baselines call `parse_message`.
These are synthetic decoder measurements, not service throughput or network
results, and not a commit-before/after body benchmark.

| Fixture | Wire bytes | Fragment bytes | Cumulative parsing (µs) | Incremental feed (µs) | Ratio |
| --- | --- | --- | --- | --- | --- |
| Long head | 1,025 | 1 | 43.860 | 15.525 | 2.83× |
| Long head | 4,097 | 1 | 242.149 | 41.040 | 5.90× |
| Long head | 8,001 | 1 | 751.969 | 77.352 | 9.72× |
| Dense head | 6,738 | 1 | 2,796.729 | 69.471 | 40.26× |
| Dense head | 6,738 | 16 | 175.864 | 9.109 | 19.31× |
| Dense head | 6,738 | 1,024 | 4.591 | 3.744 | 1.23× |
| Fixed body | 262,187 | 1,024 | 474.803 | 13.772 | 34.48× |
| Chunked body | 262,205 | 1,024 | 464.550 | 13.478 | 34.47× |

A separate head-only before/after comparison against `e0ebbb1` (seven
alternating samples, 100,000 calls per sample) records: empty-field head
21.99→23.09 ns (+5.0%), typical two-field POST 78.17→77.57 ns (−0.8%),
8,000-byte plain value 537.35→529.56 ns (−1.4%), 8,000-byte OWS value
393.97→387.78 ns (−1.6%), and 128 short fields 1,713.62→1,637.47 ns (−4.4%).
The smallest path has a measured approximately 1 ns cost; no claim that every
path became faster is made.

Structured review: production files remain below 600 lines; the new test file
has 399 lines. New or changed executable units remain below 60 code lines and
nesting below the review threshold. Cyclomatic complexity: N/A — no configured
complexity tool; executable spans and nesting were reviewed instead. The wire
layer owns all state, uses no shared mutable decoder state, and adds no runtime
dependency, thread, listener, reusable build artifact or remote operation.
Disposable compiler/test outputs are deleted before task completion; command
results are retained here. Formatting, diff, and governance checks are recorded
in the final revision's Change Log.

## Request Resource Budgets [Required]

On 2026-10-16 the owner explicitly waived ADR preapproval and serialization
for confirmed review defects. That authorization permits these repairs; it is
not an `Approve` for this revised ADR. The record remains Proposed / Not Started
under the lifecycle contract, and socket/TLS/browser acceptance is unexecuted.
The historical no-metadata-limit description above does not describe the
current workspace. The following contract supersedes that resource policy:

**CC-8 — Independent request budgets.** `request_body_cap` limits decoded
content to 1,048,576 bytes. `chunk_metadata_cap` independently limits the total
chunk size lines (including extensions and CRLF), data-ending CRLF, last-chunk
line, and trailer section (including its final CRLF) to 8,192 wire bytes per
request. Content bytes and request-head bytes do not spend this metadata
budget. The 8,193rd metadata byte produces sticky
`ParseError::chunk_metadata_too_long`; the eventual server must return HTTP 413
and close as CC-5 requires. `reset()` clears the counter only for an independent
input stream; an error does not establish a reusable next-request boundary.
A complete chunked message must satisfy both budgets. There is no general
minimum chunk size or promise to accept every encoding of a 1 MiB body.

With no extensions or trailers, 1 MiB encoded in 1,024-byte chunks spends
1,024 × 7 + 5 = 7,173 metadata bytes and succeeds; 512-byte chunks would spend
2,048 × 7 + 5 = 14,341 bytes and are rejected before completion. Smaller
messages may use smaller chunks within the same aggregate budget. Splitting
identical wire bytes across socket reads or decoder `feed()` calls changes
neither budget nor outcome. Raising the budget merely to accept 512-byte
chunks is outside this repair; it would change the resource policy.

Deterministic parser checks are
`check_metadata_limits`, `check_aggregate_metadata`, and
`check_joint_chunk_budgets` in
`service/tests/http_request_decoder_tests.cpp`: exact 8,192/8,193 boundary,
aggregate chunk/trailer accounting, sticky error and reset, 1 MiB content
with 512/1,024-byte HTTP chunks, and 17/8,192/whole-wire transport feeds.
Execution evidence and source content IDs are retained in
`docs/reviews/2026-10-16-peer-assessment-followup.md` and its adjacent evidence
file. These parser results do not claim the absent server has sent a 413.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| CC-1 | docs/architecture/ADD-0001-orange-v1-baseline.md › Functional Capability Design › CAP-06 | One developer can build and start the backend service and web client locally through one documented path, with no external services required | AC-3, AC-4 | AC-3 runs the one documented path on a clean checkout; AC-4 restarts it at the same origin |
| CC-2 | docs/architecture/ADD-0001-orange-v1-baseline.md › Cross-Cutting Design › CON-02 | The compute service is loopback-only, has no outbound runtime calls, and exposes no LAN or cloud deployment mode | AC-1, AC-5 | AC-1 asserts the loopback bind in the service test suite; AC-5 shows no non-loopback listener and a failing LAN connection while the service runs |
| CC-3 | docs/architecture/ADD-0001-orange-v1-baseline.md › ADR Task Candidates › CAND-0001 | Clean local checkout starts both sides, the client reaches the health endpoint, and ordinary restart preserves the origin; no LAN listener | AC-2, AC-3, AC-4, AC-5 | The checks execute exactly these observable outcomes on the delivered slice |
| CC-4 | AGENTS.md › Verification And Completion Evidence | Source-code features use Red-Green-Refactor: the smallest failing test lands before the production change | AC-1, AC-2 | Each subtask's suite is committed failing first for its missing behavior, then implemented to green, with the sequence recorded in the pull request |
| CC-5 | docs/adr/ADR-0003-local-runtime-path.md › Scope | An over-limit head receives 431; an over-limit decoded body or aggregate chunk metadata receives 413; each response declares `Connection: close`, completes before connection closure, and no following request is handled on that connection; subsequent requests use a new connection | AC-1 | Real-socket cap cases assert status, framing, `Connection: close`, EOF, no pipelined follow-up response on the rejected connection, and a successful valid request after reconnecting |
| CC-6 | docs/adr/ADR-0003-local-runtime-path.md › Scope | An unfinished request on one connection does not block a complete request on another connection | AC-7 | Pause an echo body on connection A; assert B's complete health response before sending A's remaining bytes, then assert A's exact echoed body |
| CC-7 | docs/adr/ADR-0003-local-runtime-path.md › Scope › web/; Implementation Plan › T-3 | The served status page executes its health request and displays the returned status; a failed request exits loading and displays a connection-error state within AC-8's proposed two-second failure-feedback budget | AC-8 | A real browser loads the served page and records the health request and visible ok status; a second load with only the health request aborted records visible failure, no success state and no loading indicator within two seconds |
| CC-8 | docs/adr/ADR-0003-local-runtime-path.md › Request Resource Budgets | Decoded content and aggregate chunk metadata independently satisfy 1,048,576-byte and 8,192-byte limits; byte 8,193 yields sticky chunk_metadata_too_long, reset clears independent-stream state, and feed fragmentation changes neither budget | AC-1; check_metadata_limits; check_aggregate_metadata; check_joint_chunk_budgets | Exact boundary/aggregate/reset cases and 1 MiB encoded with 512/1,024-byte chunks across 17/8,192/whole-wire feeds; future socket cases exercise the CC-5 rejection lifecycle |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: one keep-alive connection preserves response order; an echo request paused midway on connection A must not prevent connection B from completing a health request | Runtime assembly (service) | AC-1 drives 3 keep-alive cycles; AC-7 pauses A's body while B uses a one-second receive deadline and A retains the default five-second idle timeout | All 3 sequential responses remain ordered; B receives its complete 200 health response before A resumes; A subsequently receives the exact hello echo | AC-1, AC-7 | Not Started | Not run — network implementation is absent |
| timeout and deadline | Applicable: idle keep-alive connections close at their configured timeout; a failed browser health request must not leave the status view loading indefinitely | Runtime assembly (service and status page) | AC-1 configures the idle timeout to 1 second; AC-8 aborts only the browser health request while serving the page and assets normally | The idle connection closes after its configured timeout; the browser shows a nonempty connection-error message with no success state or loading indicator within two seconds of the failed request | AC-1, AC-8 | Not Started | Not run — network and browser implementation are absent |
| cancellation and interruption | N/A — the endpoints are stateless with no session or persisted state; the interruption outcome is the documented restart path | Runtime assembly | AC-5 restart procedure | The same origins serve the page and health after restart | AC-5 | N/A — stateless endpoints; restart is the recovery path and is covered by AC-5 | N/A — not run |
| resource bounds and backpressure | Applicable: parsing enforces the 8 KiB head, 1 MiB decoded-content and independent 8 KiB aggregate chunk-metadata caps (CC-8); after rejection the next message boundary is not assumed safe, and idle timeout bounds descriptor accumulation (no storm-tested connection guarantee) | Runtime assembly (service) | CTest real-socket cases send an over-8-KiB head and over-1-MiB fixed/chunked content and metadata byte 8193, with a following valid pipelined request; parser tests cover exact boundaries, sticky errors/reset and 512/1024-byte chunk encodings across feed sizes; after EOF, reconnect and issue a valid request | Each cap response has respectively 431/413, valid framing and `Connection: close`; rejection response completes, then EOF; no follow-up response is sent on the rejected connection; the reconnected valid request receives its expected successful response; no crash | AC-1 | Not Started | Not run — network closure behavior awaits reapproval and implementation |
| framework or trust-boundary rejection | Applicable: both listeners must bind loopback only, and the TLS listener must present the generated self-signed certificate | Runtime assembly (service) | CTest bind assertions for both listeners plus a live `lsof` inspection, a LAN-address connection attempt on both ports, and a `curl -k` TLS handshake check | Listener list shows only `127.0.0.1:8080` and `127.0.0.1:8443`; LAN connections fail; the TLS handshake succeeds and serves the health body | AC-1, AC-2, AC-6 | Not Started | Not run — implementation not started |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | All declared transport cases pass, including rejection closure and reconnection | Service built after toolchain bootstrap; unit fixtures and one client driving real sockets, three valid keep-alive cycles on one connection, idle timeout configured to 1 second, oversized head/fixed/chunked body and aggregate metadata byte 8193 with a pipelined follow-up; parser cases at metadata 8192/8193 and 1 MiB content in 512/1024-byte HTTP chunks using 17/8192/whole-wire feeds | `ctest --test-dir service/build --output-on-failure` | Exit 0: request-line/header/body parsing, binary Content-Length/chunked decoding, exact independent caps (CC-8) and response chunk encoding pass; 1 MiB in 1024-byte chunks succeeds and in 512-byte chunks fails with chunk_metadata_too_long at every feed size; sticky metadata errors require reset for an independent stream; three valid cycles stay ordered on the same connection; idle connection closes at the configured timeout; oversized head yields 431, oversized fixed/chunked content or aggregate chunk metadata yields 413, each with a complete framed `Connection: close` response followed by EOF and no pipelined follow-up response; a valid request on a new connection succeeds | Named passing unit and real-socket tests, response/EOF/reconnection assertions, command exit code | Not Started | Not run after reapproval — historical decoder results do not establish socket behavior |
| AC-2 | T-2 | The serving and TLS integration checks pass over both listeners | Service built and running from the test harness | The T-2 CTest integration suite plus `curl` checks against `http://127.0.0.1:8080` and `curl -k` against `https://127.0.0.1:8443` | `GET /api/health` returns HTTP 200 with exactly `{"status":"ok"}` on both listeners; `POST /api/echo` returns the sent body with chunked framing; a single-range static request returns 206 with the correct slice and an unsatisfiable range returns 416; 405 and 404 behave as declared | Test and curl transcripts | Not Started | Pending |
| AC-3 | T-3 | The web suite and build pass and the page wires the health status | `web/` npm project installed | `npm test --prefix web` then `npm run build --prefix web` | Both exit 0; the compiled page references the status script that fetches `/api/health` | Command outputs and the served HTML/JS reference | Not Started | Pending |
| AC-4 | T-3 | The one documented path starts the assembled runtime on a clean checkout | Clean checkout after documented toolchain bootstrap and first-run certificate generation | `scripts/dev.sh`, then `curl -kfsS https://127.0.0.1:8443/api/health` | The script prints the stable origin; curl exits 0 and prints exactly `{"status":"ok"}` | Script and curl transcript with exit codes | Not Started | Pending |
| AC-5 | T-3 | An ordinary stop and restart preserves the browser origins | Running service from AC-4 | Stop the service, start it again through the same path, then `curl -kfsS` the same page and health URLs on both listeners | All requests exit 0 at the identical `https://127.0.0.1:8443` and `http://127.0.0.1:8080` origins with the same certificate | Restart transcript with curl exit codes | Not Started | Pending |
| AC-6 | T-3 | No non-loopback listener exposes the service | Running service from AC-4 | `lsof -nP -iTCP:8080 -iTCP:8443 -sTCP:LISTEN`, then connect to the machine's LAN IPv4 address on both ports | The listener list shows only `127.0.0.1:8080` and `127.0.0.1:8443`; both LAN connections fail | Command outputs | Not Started | Pending |
| AC-7 | T-2 | A paused request does not block another connection | Real serving harness with the default five-second idle timeout; connection A sends the headers for POST /api/echo with Content-Length 5 and only he as content; connection B has a one-second receive deadline | The T-2 CTest real-socket case sends GET /api/health on B without sending A's remaining bytes; only after B finishes does it send llo on A; exercise both plaintext and TLS listeners | On each listener B receives HTTP 200 with exactly {"status":"ok"} before its receive deadline and before A resumes; A then receives a complete response whose decoded body is exactly hello; neither connection is closed for an idle timeout during the fixture | Named test and timestamped send/receive assertions showing B completes before A resumes, exact decoded bodies and exit code | Not Started | Not run — real socket and serving implementation are absent |
| AC-8 | T-3 | The served page displays real health success and bounded connection failure in a browser | Runtime from AC-4 in one supported desktop browser with version and OS recorded; accept the self-signed certificate in that profile; browser network instrumentation can abort only /api/health | Load the stable HTTPS origin and observe the page, health network request and visible status; reload with only /api/health aborted while HTML and script requests still reach the real service; record elapsed time from the failed health request | The first load executes GET /api/health, receives HTTP 200 with exactly {"status":"ok"}, and visibly displays ok; the failure load shows a nonempty connection-error message with no success state or loading indicator within two seconds; neither load produces an uncaught script error | Browser version and OS, certificate-trust setup, network transcript, elapsed-time observation, visible-state screenshots and console result | Not Started | Not run — web client and runtime are absent |

Allowed final check statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
`Fail` blocks completion. `N/A` is valid only when the check's stated trigger or
precondition demonstrably does not apply.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Not Started | Pending |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Not Started | Pending |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | Not Started | Pending |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Not Started | Pending |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Not Started | Pending |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | Not Started | The standards exist under ADR-0004. The proposed head-test main exception is recorded in Engineering Exceptions and awaits review; its removal is required before T-1 completion |
| A-7 | Contract and baseline risks covered, when applicable | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | Not Started | Pending |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Not Started | Pending |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Archive this record in the same change that retires it when either Decision
Status is `Rejected` with Implementation Status `Not Applicable`, or Decision
Status is `Deprecated` or `Superseded` with Implementation Status `Verified`,
`Complete`, or `Not Applicable`. Before that trigger, retain this section as
inactive future-lifecycle guidance; its checklist does not affect acceptance or
implementation completion. When triggered:

- [ ] Move this file to `archive/ADR-0003-local-runtime-path.md` under this same ADR
      root (project `docs/adr/archive/` or `<service>/docs/adr/archive/`).
- [ ] Update every code marker that cites this file's pre-archive path to the new
      archive path, or remove the marker if the governed code was deleted.
- [ ] If Decision Status is `Superseded`, set the replacement record's
      `Supersedes` field and this record's `Superseded By` field to each other's
      final repository-relative path.
- [ ] If no record supersedes this one, retain `Superseded By: None — not superseded`.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with the archived
      path, scope, and final status.
- [ ] Confirm no ADR or OCR outside an `archive/` directory, and no code marker, still cites the pre-archive path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-13 | Initial Proposed draft selecting CAND-0001 from the Current ADD-0001; records the cpp-httplib-with-vcpkg and fixed-origin decisions and the full test-first implementation plan | @zcode |
| 2026-10-13 | Revise per repository owner direction: the transport becomes a hand-written POSIX socket HTTP server within a declared minimal HTTP/1.1 subset (learning purpose, D-03), with cpp-httplib kept as the exactly pinned fallback under a binary trigger (owner direction, or the T-1 suite needing protocol machinery beyond the declared subset); scope, touchpoints, AC-1, and the resource-bounds risk row updated accordingly | @zcode |
| 2026-10-13 | Record acceptance per repository owner @hailingu's exact Approve in the session context identifying this record (after five owner-directed revisions); Decision Status Proposed→Accepted with approval metadata and an informational context revision; enter In Progress and begin T-1 | @zcode |
| 2026-10-14 | Enter Blocked from In Progress per owner direction: the engineering standards must precede implementation, the started T-1 transport code was reverted (never committed), and standards authoring needs a Full ADR that the serialization gate blocks. Record blocker fields citing docs/adr/ADR-0004-engineering-standards.md under the AGENTS.md blocker-resolution exception; update the index row in the same change | @zcode |
| 2026-10-14 | Resume from Blocked per the recorded exit criterion — ADR-0004 completed with both standards authored and cataloged (its AC-4 records the satisfied inputs) — returning Implementation Status to `In Progress`; T-1 restarts under docs/development/software-engineering-standard.md and docs/development/cpp-development-standard.md; update the index row in the same change | @zcode |
| 2026-10-13 | Fifth owner-directed revision: the connection model becomes a single-threaded `poll()` event loop multiplexing all sockets through readiness-driven read/write state machines, so multiple connections (multiple browser tabs, anticipated by ADD-0001 INV-16 and GO-02) are served concurrently without threads or head-of-line blocking; the one-client one-stable-connection standard remains the exercised and tested model, and the 5-second idle timeout now bounds descriptor accumulation. Consequences trade-off updated from bounded waiting to state-machine complexity | @zcode |
| 2026-10-13 | Fourth owner-directed revision, applied wholesale to the one-client one-stable-connection standard: the connection model becomes a single-threaded iterative loop serving one keep-alive connection at a time with backlog queueing and a 5-second idle timeout; the thread-per-connection model, the max-16 bound, and every multi-client test (including the two-connection head-of-line check) are removed; consequences, risk rows, and AC-1 now describe exactly one persistent client as the exercised model | @zcode |
| 2026-10-13 | Third owner-directed revision: the concurrency scenario shrinks to one persistent client performing multiple request-response cycles on a single connection (single-user origin, D-06), and the connection-bound check becomes a two-connection no-head-of-line-blocking case (one held idle, one active); the max-16 connection bound stays a declared construction guard rather than a storm-tested guarantee | @zcode |
| 2026-10-13 | Second owner-directed revision: expand the declared feature set to keep-alive, chunked transfer in both directions, single-range requests, request-body parsing via a `POST /api/echo` diagnostic route, and a TLS listener through pinned OpenSSL with a first-run self-signed certificate; the stable origin becomes `https://127.0.0.1:8443` with `http://127.0.0.1:8080` kept as the comparison endpoint; the connection model becomes bounded thread-per-connection (max 16) with a declared idle timeout; subtasks, touchpoints, risk matrix (timeout now applicable), and acceptance checks restructured accordingly | @zcode |
| 2026-10-14 | Begin T-1 implementation under the engineering standards: land the task-1 build-and-test scaffold (`service/CMakeLists.txt`, `service/tests/http_message_tests.cpp` with the in-tree `CheckReporter`, root `.clang-format`) with zero production code; harness wiring observed deliberately failing then passing through the verification trio; subtask T-1 moves to `In Progress` | @zcode |
| 2026-10-14 | Land the first T-1 behavioral contract red-green: commit 12b0e43 asserts `parse_request` rejects empty input with `std::nullopt` and is observed failing (ctest exit 8), then the minimal rejection implementation turns the suite green in the same change as the T-1 evidence update (evidence row updated; suite 1/1 passing) | @zcode |
| 2026-10-14 | Land the second T-1 behavioral contract red-green: commit 3faf3d9 asserts the request-line structure (`method SP request-target SP HTTP-version CRLF`, three non-empty tokens, owned field copies) and seven malformed-line rejections, observed red (10 of 11 new checks failing), then the request-line parser turns the suite green in the same change as the T-1 evidence update; the now-redundant wiring-only test call is removed | @zcode |
| 2026-10-14 | Adopt the contributed reversed terminator search as a behavior-preserving refactor: commit 211c62b first pins the line boundary (tokens cannot cross the first CRLF) and the bare-CR token semantics, then the line-end location swaps from `find("\r\n")` to `find('\n')` plus one-byte look-back — equivalence established by a 13-input variant cross-check and the green 15-check suite; the swap removes the memchr-restart worst case measured at ~23 µs on CR-dense input while keeping the owned-copy Request contract | @zcode |
| 2026-10-14 | Apply the owner review to the request-line parser, red-green through commits e739c68 and the implementing change: the contract becomes `std::variant<ParsedRequest, ParseError>` distinguishing incomplete/malformed/to_large with consumed bytes (the cpp standard's sanctioned variant channel); RFC 9112 2.2 conformance lands (leading empty lines ignored; bare CR rejected, flipping the earlier accept-pin deliberately); fields gain grammar validation (tchar method, `HTTP/DIGIT.DIGIT` version with case-sensitive HTTP-name, target free of SP/CTL bytes); every line search is bounded by the declared 8 KiB `header_block_cap`, removing the unbounded-rescan hazard; the redundant empty-input guard and the version-space check subsumed by version grammar are removed | @zcode |
| 2026-10-14 | Apply the second owner review red-green through commit db2856f and the implementing change: request-line overflow follows RFC 9112 3 (`method_too_long` → 501, `target_too_long` → 414) instead of the header-field 431, prefix-invalid overflows and empty-line exhaustion stay `malformed`, target octets follow the RFC 3986 URI character set, and `parse_request` becomes `[[nodiscard]]`; two test defects are fixed (the NUL fixture that encoded VT, and the dangling `parsed_of` pointer). Recorded for later tasks, deliberately not pre-implemented: (1) header parsing (T-1) must spend only the remaining head budget `header_block_cap - ParsedRequest::consumed`, owns the 431 outcome, and confirms the shared request-line/header budget against Scope's "8 KiB header-block cap"; (2) the event loop (T-1) maps each `ParseError` to its status and closes the connection after a 400 (RFC 9112 2.2); (3) dispatch (T-2) validates the request-target form per method (origin-form for routes, authority-form only for CONNECT, asterisk-form only for OPTIONS, RFC 9112 3.2) and decides the policy for grammar-valid non-1.x versions (505); (4) T-2 reconciles Scope's "HTTP 405 for unsupported methods" with RFC 9110 9.1, which reserves 405 for recognized methods not allowed on the target and recommends 501 for unrecognized ones — an owner determination, possibly an ADR revision | @copilot |
| 2026-10-14 | Adopt the benchmark-validated parts of the external SIMD/LUT contribution as a behavior-preserving refactor: the byte classifiers become one compile-time 256-entry bitmask table (replacing per-call `string_view::find` set searches) and the percent-encoding check indexes directly (no per-escape view slicing). A three-way variant cross-check (HEAD, LUT-owned, reviewer-zero-copy) over 16 inputs shows 0 mismatches; measured at -O2 on the host arm64 toolchain the refactor cuts the hot path 31.4→23.9 ns, the punctuation-target regression 419→126 ns, and the overflow classification 4651→2559 ns with CR-dense unchanged. Deliberately not adopted: the zero-copy `string_view` Request (14.3 ns short valid) breaks the owned-copy contract on documented dangling grounds, and the hand-AVX2 scanner is inert on arm64 — this host has no Rosetta, and `std::find` already runs NEON memchr | @zcode |
| 2026-10-14 | Per owner direction, record the borrowed-view lifetime trap inside the Request contract comment itself: why the fields stay owned copies (silent undefined behavior on temporary or reused buffers, no diagnostic), the measured price of ownership (~10-25 ns per request), and the single revisit condition — an event loop that guarantees a stable read buffer for the whole parse-to-response window by construction; comment-only change, suite re-verified green | @zcode |
| 2026-10-14 | Land the header-block T-1 contract red-green through commit f9e6364 and the implementing change: `parse_request` now completes a whole head — request line, field lines, terminating empty line — with field lines following `field-name: OWS field-value OWS` (RFC 9112 5) into owned `Request::headers` entries that preserve order, duplicates, and case; values are OWS-trimmed field-content with obs-text allowed (RFC 9110 5.5) and obs-fold is rejected (RFC 9112 5.2); the block spends only the head budget remaining after the request line and owns the 431 `headers_too_long` classification (RFC 6585 5), discharging the recorded later-task item (1); body parsing (Content-Length and chunked) is the next T-1 rung | @zcode |
| 2026-10-14 | Optimize the existing T-1 request-head parser without changing its interface or protocol outcomes: red allocation tests committed first in `ba7da00`, then defer owned construction, reserve the validated field count once, record bounded field offsets, and short-circuit invalid method/version checks before target validation. Request-Head Optimization Evidence records immutable source/test blobs, A/B timings and allocation counts, configured verification, sanitizer and differential results, and the bounded stack trade-off; acceptance checks, scope, approval metadata, and lifecycle status are unchanged | @codex |
| 2026-10-14 | Refactor existing T-1 field-value and overflowing-tail validation into bounded groups with a short-value/scalar-remainder path after characterization tests in `39c5bc9`. Grouped Field-Value Validation Evidence records immutable blobs, measured long-value improvements and short-input costs, mutation coverage, sanitizer/differential results, and unchanged allocations. Approval metadata, accepted scope, acceptance checks, and lifecycle status remain unchanged | @codex |
| 2026-10-14 | Refactor existing T-1 OWS trimming into bounded all-whitespace groups after characterization tests in `a8a9502`. Grouped OWS Trimming Evidence records immutable blobs, predeclared experimental performance budgets observed failing then passing, short-input costs, mutation coverage, and sanitizer/differential results. Approval metadata, accepted scope, contract traceability, risk-matrix requirements, acceptance checks, and lifecycle status remain unchanged | @codex |
| 2026-10-15 | Complete the authorized body/incremental-decoder portion of T-1 before the rejection-close policy revision: red commits `2f11b19`, `8cd704a`, `6006103`, green implementation `f558e62`; Request-Body And Incremental Decoder Evidence records 5,105 checks, sanitizer/differential validation, synthetic calling-model timings and the measured short-head cost. Response encoding, sockets, T-2 and T-3 remain unfinished | @codex |
| 2026-10-15 | Invalidate prior approval at 2026-10-14T16:20:49Z because owner @hailingu chose responses followed by connection closure for 413/431, replacing the resource-risk requirement to continue on the same connection. Prior Approver @hailingu; Approval Time 2026-10-13T17:02:57Z; Approval Evidence: Approve; informational Approval Context Revision 82a585e556c648ceacfbd86a73e107dbb422427e. Decision Status Accepted→Proposed and Implementation Status In Progress→Not Started; T-1 resets to Not Started. Earlier implementation/evidence is retained as historical and must be reverified after reapproval. Scope, Q-4, T-1, touchpoints, CC-5, risk row and AC-1 now specify close-after-rejection and success on a new connection; index synchronized | @codex |
| 2026-10-15 | Proposed-record review also removes the obsolete duplicate out-of-scope list contradicting the previously owner-directed echo/body subset and replaces the stale no-engineering-standard claim with a measured temporary exception for the unchanged head-test main, to be decomposed before T-1 completion. No ADD outcome, primary boundary, dependency or product capability changes | @codex |
| 2026-10-15 | Proposed-revision validation: `npm test --prefix tools/governance-validator` passes 208/208; `npm run validate --prefix tools/governance-validator` passes; `git diff --check` and clang-format check mode pass. Structured review confirms requirement-level metadata, prior-approval history, exact cap-response/EOF/reconnection assertions, risk-to-AC-1 and CC-5 traceability, one Runtime assembly boundary and three subtasks, truthful historical evidence, ADR index synchronization and unchanged reciprocal ADD CAND-0001 Selected linkage (its existing reason already names Proposed). The reapproval-stage reset is truthful; no socket-closure acceptance is claimed | @codex |
| 2026-10-15 | Address quality-review findings 3 and 6 in the existing Proposed record: add real two-connection progress check AC-7 and real-browser success/failure check AC-8, map them through scope, T-2/T-3, CC-6/CC-7 and the risk matrix, and correct A-6's obsolete no-standard claim. AC-8 proposes a two-second health-failure feedback budget; it does not derive that requirement from the ADD's compute-request deadline. Decision Status remains Proposed, Implementation Status remains Not Started and approval metadata remains pending; no runtime implementation or passing network/browser result is claimed. Existing validator tests pass 208/208; governance validation and git diff --check pass | @codex |
| 2026-10-15 | Remove the obsolete head-test-main size exception with measured before/after evidence from Accepted service/docs/adr/ADR-0001-http-message-test-decomposition.md. The extraction retains every request-head assertion and both CTest targets pass; main is 24 code lines and the file 718 physical lines. Runtime decision, implementation and approval metadata and reciprocal ADD candidate linkage remain unchanged; this maintenance evidence grants no runtime implementation approval | @codex |

| 2026-10-16 | Under the owner's explicit repair waiver, synchronized Scope, T-1, stable symbols, CC-5/CC-8, resource risk and AC-1 with the implemented independent chunk-metadata budget. Added joint-budget characterization; retained historical evidence and truthful Proposed / Not Started status. Formal approval and runtime socket acceptance are not inferred from the waiver. | @codex |

| 2026-10-19 | Reapproval: repository owner @hailingu self-declared the identity and responded with the exact word Approve in the sole context of this record's repository-relative path. Decision Status Proposed→Accepted; Implementation Status remains Not Started; Approver, Approval Time and Approval Evidence recorded; Approval Context Revision recorded informationally at commit 50a0075, which exactly represents the approved content. Approval-preserving metadata change; no implementation evidence is claimed | @zcode |
