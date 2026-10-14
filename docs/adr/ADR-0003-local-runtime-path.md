# ADR-0003: One documented local runtime path with a loopback health endpoint

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: In Progress
- **Date**: 2026-10-13
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-13T17:02:57Z
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Approve
- **Approval Context Revision [Optional — informational and non-binding]**: 82a585e556c648ceacfbd86a73e107dbb422427e — immutable revision whose record content equals the content present for approval; the acceptance edits were made after approval within the same change
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
  paths; an 8 KiB header-block cap answering 431 and a 1 MiB body cap
  answering 413; and a single-threaded `poll()` event-loop connection model:
  one thread multiplexes every open socket through readiness-driven reads
  and writes, so several connections (for example, multiple browser tabs)
  are served concurrently without threads or head-of-line blocking, while
  the exercised and tested client model remains one persistent client on
  one stable connection. Idle connections are closed after the declared
  5-second timeout to bound descriptor use.
- TLS: an `https://127.0.0.1:8443` listener terminated through OpenSSL
  (exactly pinned; crypto primitives are never hand-rolled — only the
  protocol layer is), with a self-signed certificate generated on first run
  into a gitignored location and reused across restarts.
- `web/` — a minimal TypeScript client: one page and a `status` module that
  fetches `/api/health` and renders the result, compiled by a pinned `tsc`
  toolchain to `web/dist/`.
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

Out of scope:

- Any domain capability: expression evaluation, matrix computation, content,
  exercises, progress, or tutoring.
- Compute endpoints beyond the health route, request-payload limits, or
  overload-rejection behavior (owned by the domain candidates).
- Client tooling beyond `tsc` (bundler, dev server, framework choices belong
  to the presentation candidates).
- CI, packaging, deployment tooling, and any remote or LAN exposure.

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
  blocking, while the single persistent client remains the exercised and
  tested model.
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
| T-1 | Hand-written HTTP/1.1 transport core, developed test-first | `service/` CMake project and POSIX socket code: single-threaded `poll()` event loop with readiness-driven read/write state machines, keep-alive connection lifecycle with the declared 5-second idle timeout, request-line/header/body parsing (Content-Length and chunked decode), response framing (Content-Length and chunked encode), the 8 KiB header cap (431) and 1 MiB body cap (413); CTest unit suite for every behavior, with one persistent client completing multiple request-response cycles on a single stable connection as the exercised client model | In Progress | Task 1 scaffold landed 2026-10-14 in this change: `service/CMakeLists.txt`, `service/tests/http_message_tests.cpp` (in-tree `CheckReporter` harness), and root `.clang-format`; harness wiring observed deliberately failing then passing through the verification trio (Apple clang 21.0.0, CMake 4.2.3); task 2 scaffold 2026-10-14: the `orange_http` library (namespace `orange::http`, `parse_request` declaration with a placeholder body) is linked into the suite wiring-only, asserting no behavior; first behavioral contract 2026-10-14: empty-input rejection landed failing-first in commit 12b0e43 (`parse_request("")` must yield `std::nullopt`, observed red at `http_message_tests.cpp:65` with ctest exit 8) and turned green with the minimal rejection implementation in the same change as this evidence update — suite 1/1 passing (ctest exit 0), clang-format clean, no build warnings |
| T-2 | Serving features and TLS listener, developed test-first | Route dispatch, `GET /api/health`, `POST /api/echo` with chunked responses, static serving with MIME map and single-range 206/416, 405/404, loopback bind on both listeners, and the OpenSSL-terminated `https://127.0.0.1:8443` listener with first-run self-signed certificate; CTest integration suite plus `curl`-based end-to-end checks over both plaintext and TLS | Not Started | Pending |
| T-3 | TypeScript status page, one documented run path, and verification evidence | `web/` npm project with pinned `typescript`, `src/status.ts` and its `node --test` suite, `tsc` build to `web/dist/`, `scripts/dev.sh` with first-run certificate generation, root `README.md` run section, repository-root verification runs of the acceptance checks | Not Started | Pending |

**Affected paths**: `service/**`, `web/**`, `vcpkg.json`, `scripts/dev.sh`,
`README.md` (certificate material is generated into a gitignored location and
never committed)

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| `service/src/http_server.cpp` | `serve_loopback(port, handler)` | N/A — stable symbol or anchor is sufficient | Owns the single-threaded `poll()` event loop and the per-connection readiness-driven read/write state machines with the 5-second idle timeout | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `service/src/http_messages.cpp` | `parse_request()` and `write_response()` | N/A — stable symbol or anchor is sufficient | Own the wire format: request-line/header/body parsing with Content-Length and chunked decoding, the 8 KiB and 1 MiB caps, and response framing with the chunked encoder | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `service/src/tls.cpp` | `serve_tls()` | N/A — stable symbol or anchor is sufficient | Terminates TLS through the pinned OpenSSL library on the https listener using the generated self-signed certificate; crypto primitives are never hand-rolled | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `service/src/routes.cpp` | `handle_request()` | N/A — stable symbol or anchor is sufficient | Dispatches health, echo, static-with-single-range (206/416), and 405/404 outcomes | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `web/src/status.ts` | `renderServiceStatus()` | N/A — stable symbol or anchor is sufficient | Fetches `/api/health` and renders the observed status with a fail-fast connection-error state | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `scripts/dev.sh` | the script's documented command contract | N/A — stable symbol or anchor is sufficient | The single documented path: generates the certificate if absent, builds service and web, starts both listeners, prints the stable origin | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |

**Migration and rollback strategy [Conditionally Required — this replaces or
changes existing behavior]**: N/A — greenfield slice; nothing existing is
replaced. Rollback is deleting the added paths.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

| Rule and affected unit | Measured value or condition | Rationale | Risks | Compensating controls | Owner | Removal, review, or permanent rationale | Verification evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| N/A — no repository software-engineering standard exists yet; development follows the AGENTS.md test-first rule and exactly pinned dependencies, and no rule is exceeded or waived | N/A — no standard to measure against | N/A | N/A | N/A | N/A | N/A | N/A |

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| CC-1 | docs/architecture/ADD-0001-orange-v1-baseline.md › Functional Capability Design › CAP-06 | One developer can build and start the backend service and web client locally through one documented path, with no external services required | AC-3, AC-4 | AC-3 runs the one documented path on a clean checkout; AC-4 restarts it at the same origin |
| CC-2 | docs/architecture/ADD-0001-orange-v1-baseline.md › Cross-Cutting Design › CON-02 | The compute service is loopback-only, has no outbound runtime calls, and exposes no LAN or cloud deployment mode | AC-1, AC-5 | AC-1 asserts the loopback bind in the service test suite; AC-5 shows no non-loopback listener and a failing LAN connection while the service runs |
| CC-3 | docs/architecture/ADD-0001-orange-v1-baseline.md › ADR Task Candidates › CAND-0001 | Clean local checkout starts both sides, the client reaches the health endpoint, and ordinary restart preserves the origin; no LAN listener | AC-2, AC-3, AC-4, AC-5 | The checks execute exactly these observable outcomes on the delivered slice |
| CC-4 | AGENTS.md › Verification And Completion Evidence | Source-code features use Red-Green-Refactor: the smallest failing test lands before the production change | AC-1, AC-2 | Each subtask's suite is committed failing first for its missing behavior, then implemented to green, with the sequence recorded in the pull request |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: one keep-alive connection reused for multiple sequential requests must return responses in request order with correct framing (the single-user origin's observable requirement) | Runtime assembly (service) | CTest case with one persistent client performing 3 request-response cycles on a single connection | All 3 responses are correct, complete, and answered in request order on the same connection | AC-1 | Not Started | Not run — implementation not started |
| timeout and deadline | Applicable: idle keep-alive connections must not accumulate descriptors forever once their owners stop using them | Runtime assembly (service) | CTest case with the idle timeout configured to 1 second: send a request, then hold the connection idle | The server closes the idle connection after the timeout; other connections continue to be served | AC-1 | Not Started | Not run — implementation not started |
| cancellation and interruption | N/A — the endpoints are stateless with no session or persisted state; the interruption outcome is the documented restart path | Runtime assembly | AC-5 restart procedure | The same origins serve the page and health after restart | AC-5 | N/A — stateless endpoints; restart is the recovery path and is covered by AC-5 | N/A — not run |
| resource bounds and backpressure | Applicable: a hand-written parser must not accept unbounded input, and the idle timeout is the declared bound on descriptor accumulation (no storm-tested connection guarantee) | Runtime assembly (service) | CTest cases sending a header block over 8 KiB (expect 431) and a body over 1 MiB (expect 413) | Each over-limit input receives its coded rejection without crashing, and the single stable connection continues to serve subsequent requests | AC-1 | Not Started | Not run — implementation not started |
| framework or trust-boundary rejection | Applicable: both listeners must bind loopback only, and the TLS listener must present the generated self-signed certificate | Runtime assembly (service) | CTest bind assertions for both listeners plus a live `lsof` inspection, a LAN-address connection attempt on both ports, and a `curl -k` TLS handshake check | Listener list shows only `127.0.0.1:8080` and `127.0.0.1:8443`; LAN connections fail; the TLS handshake succeeds and serves the health body | AC-1, AC-2, AC-6 | Not Started | Not run — implementation not started |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | The transport-core suite passes every declared protocol behavior | Service built after toolchain bootstrap | `ctest` in the service build directory | Exit code 0 with tests covering request-line/header/body parsing (Content-Length and chunked), chunked response encoding, keep-alive cycles on one stable connection, the idle timeout, and the 431 and 413 caps all passing, exercised by a single persistent client | Test output with named passing tests and exit code | Not Started | Pending |
| AC-2 | T-2 | The serving and TLS integration checks pass over both listeners | Service built and running from the test harness | The T-2 CTest integration suite plus `curl` checks against `http://127.0.0.1:8080` and `curl -k` against `https://127.0.0.1:8443` | `GET /api/health` returns HTTP 200 with exactly `{"status":"ok"}` on both listeners; `POST /api/echo` returns the sent body with chunked framing; a single-range static request returns 206 with the correct slice and an unsatisfiable range returns 416; 405 and 404 behave as declared | Test and curl transcripts | Not Started | Pending |
| AC-3 | T-3 | The web suite and build pass and the page wires the health status | `web/` npm project installed | `npm test --prefix web` then `npm run build --prefix web` | Both exit 0; the compiled page references the status script that fetches `/api/health` | Command outputs and the served HTML/JS reference | Not Started | Pending |
| AC-4 | T-3 | The one documented path starts the assembled runtime on a clean checkout | Clean checkout after documented toolchain bootstrap and first-run certificate generation | `scripts/dev.sh`, then `curl -kfsS https://127.0.0.1:8443/api/health` | The script prints the stable origin; curl exits 0 and prints exactly `{"status":"ok"}` | Script and curl transcript with exit codes | Not Started | Pending |
| AC-5 | T-3 | An ordinary stop and restart preserves the browser origins | Running service from AC-4 | Stop the service, start it again through the same path, then `curl -kfsS` the same page and health URLs on both listeners | All requests exit 0 at the identical `https://127.0.0.1:8443` and `http://127.0.0.1:8080` origins with the same certificate | Restart transcript with curl exit codes | Not Started | Pending |
| AC-6 | T-3 | No non-loopback listener exposes the service | Running service from AC-4 | `lsof -nP -iTCP:8080 -iTCP:8443 -sTCP:LISTEN`, then connect to the machine's LAN IPv4 address on both ports | The listener list shows only `127.0.0.1:8080` and `127.0.0.1:8443`; both LAN connections fail | Command outputs | Not Started | Pending |

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
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | N/A — no software-engineering standard exists; the subsection records its N/A reason | N/A |
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
