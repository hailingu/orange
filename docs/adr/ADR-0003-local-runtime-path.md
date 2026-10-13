# ADR-0003: One documented local runtime path with a loopback health endpoint

## Metadata [Required]

- **Decision Status**: Proposed
- **Implementation Status**: Not Started
- **Date**: 2026-10-13
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — replace with the concrete `@<actor-id>`
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — replace with an ISO 8601 date-time containing `Z` or an explicit `±HH:MM` offset
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — replace with exactly `Approve`
- **Approval Context Revision [Optional — informational and non-binding]**: remove — no immutable revision contains the approved content yet
- **Rejector [Conditionally Required — Decision Status is `Rejected`]**: Pending — replace with the concrete `@<actor-id>`
- **Rejection Time [Conditionally Required — Decision Status is `Rejected`]**: Pending — replace with an ISO 8601 date-time containing `Z` or an explicit `±HH:MM` offset
- **Rejection Evidence [Conditionally Required — Decision Status is `Rejected`]**: Pending — replace with exactly `Reject`
- **Rejection Reason [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Retired By [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with the concrete `@<actor-id>`
- **Retirement Time [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with an ISO 8601 date-time containing `Z` or an explicit `±HH:MM` offset
- **Retirement Evidence [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with exactly `Deprecate` or `Supersede`
- **Retirement Reason [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Blocked From [Conditionally Required — Implementation Status is `Blocked`]**: N/A — Implementation Status is not Blocked
- **Blocker And Evidence [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Owner [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Exit Or Recheck Criterion [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
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
  127.0.0.1 on a fixed documented port, implements the declared minimal
  HTTP/1.1 server subset by hand over POSIX sockets, serves
  `GET /api/health` returning HTTP 200 with body `{"status":"ok"}`, serves
  the built client files from `web/dist/` at `/` with a small MIME map,
  answers unsupported methods with HTTP 405 and unknown paths with HTTP 404,
  and rejects requests whose header block exceeds 8 KiB with HTTP 431.
- The declared minimal HTTP/1.1 subset: request-line and header parsing for
  `GET` only, per-request `Connection: close`, `Content-Length`-delimited
  responses with `Content-Type`, and an iterative single-threaded accept
  loop. No keep-alive, chunking, ranges, TLS, or body parsing.
- `web/` — a minimal TypeScript client: one page and a `status` module that
  fetches `/api/health` and renders the result, compiled by a pinned `tsc`
  toolchain to `web/dist/`.
- `scripts/dev.sh` — the one documented local path: build both sides and start
  the service, printing the origin URL.
- Root `README.md` — the documented run instructions, including the dependency
  bootstrap (compiler toolchain and npm) and the fixed origin.
- The authorized fallback, `cpp-httplib` exactly pinned through a `vcpkg.json`
  manifest, is added only when the fallback trigger recorded in Decision fires;
  it is not part of the default build.

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
| Q-1 | Which HTTP layer serves the loopback service? | @hailingu | 2026-10-13 | Resolved | Owner direction 2026-10-13: hand-written POSIX socket HTTP server within the declared minimal HTTP/1.1 subset, because learning the platform is a stated product purpose (D-03); cpp-httplib stays the exactly pinned fallback under Decision's recorded trigger |
| Q-2 | Which port is the stable origin? | @hailingu | 2026-10-13 | Resolved | Default 8080 on 127.0.0.1, documented in README.md; a port conflict produces a coded startup error rather than a silent alternative port |

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
layer be hand-written for that purpose. The declared minimal HTTP/1.1 subset
bounds the protocol surface to what this slice needs, the test-first plan
catches protocol defects at the smallest scale, and the exactly pinned
cpp-httplib fallback keeps the slice deliverable even if the hand-written path
hits its recorded trigger.

**Fallback trigger**: adopt the cpp-httplib fallback within this ADR's
accepted scope — recording the evidence in the Change Log and touching no
other decision content — when either (a) the repository owner directs the
fallback, or (b) the hand-written server cannot make its T-1 suite green
without protocol machinery beyond the declared minimal subset. The default
outcome remains the hand-written server.

### Consequences [Required]

Positive:

- The transport layer is fully readable and modifiable by the learner, and
  the HTTP wire protocol is studied directly (the owner's stated purpose).
- A clean checkout reaches a running local origin through one documented path
  with zero runtime library dependencies.

Negative:

- Hand-written HTTP carries protocol-defect risk a vetted library would not,
  and the declared subset must grow deliberately in later slices.

Mitigations:

- The minimal subset is declared in this ADR; anything beyond it requires an
  explicit revision here rather than quiet scope creep.
- The T-1 suite covers health, static files, 405/404, the 8 KiB cap,
  loopback bind, and 8 concurrent requests before the server is trusted.
- The exactly pinned cpp-httplib fallback is pre-authorized with a binary
  trigger, so adopting it does not reopen the decision.

## Implementation Plan [Required]

**Complete task outcome**: One documented local path builds and starts the
TypeScript client and the loopback C++20 health service at the stable origin
`http://127.0.0.1:8080`, where the page renders and `/api/health` answers
`{"status":"ok"}`, restart preserves the origin, and no non-loopback listener
exists.

**Primary implementation boundary**: Runtime assembly.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Loopback C++20 service with a hand-written minimal HTTP/1.1 subset, developed test-first | `service/` CMake project and POSIX socket code, CTest suite covering the health response, static file serving, 405 and 404 behavior, the 8 KiB header cap with HTTP 431, loopback bind assertion, and 8 concurrent health requests | Not Started | Pending |
| T-2 | Minimal TypeScript client page reporting service health, developed test-first | `web/` npm project with pinned `typescript`, `src/status.ts`, a `node --test` suite for the status module, and a `tsc` build to `web/dist/` | Not Started | Pending |
| T-3 | One documented run path and verification evidence | `scripts/dev.sh`, root `README.md` run section, repository-root verification runs of the acceptance checks | Not Started | Pending |

**Affected paths**: `service/**`, `web/**`, `scripts/dev.sh`, `README.md`
(`vcpkg.json` is added only if the fallback trigger fires)

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| `service/src/http_server.cpp` | `serve_loopback(port, handler)` | N/A — stable symbol or anchor is sufficient | Owns the hand-written POSIX transport: 127.0.0.1 bind, iterative accept loop, request-line and header parsing capped at 8 KiB, and the minimal HTTP/1.1 response framing declared in Scope | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `service/src/main.cpp` | `int main` | N/A — stable symbol or anchor is sufficient | Assembles the server, mounts the health route and static directory, and fails with a coded error when the port is occupied | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `service/src/health.cpp` | `health_response()` | N/A — stable symbol or anchor is sufficient | Returns the exact `GET /api/health` response contract: HTTP 200, body `{"status":"ok"}` | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `web/src/status.ts` | `renderServiceStatus()` | N/A — stable symbol or anchor is sufficient | Fetches `/api/health` and renders the observed status with a fail-fast connection-error state | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |
| `scripts/dev.sh` | the script's documented command contract | N/A — stable symbol or anchor is sufficient | The single documented path: builds service and web, starts the loopback service, prints the stable origin | Planned pre-commit state 2026-10-13 — replace with the implementing commit before completion |

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
| concurrency and ordering | Applicable: parallel health requests from the same page load or repeated reloads must not interleave into errors | Runtime assembly (service) | CTest case issuing 8 concurrent `GET /api/health` requests | Every response is HTTP 200 with body `{"status":"ok"}` | AC-1 | Not Started | Not run — implementation not started |
| timeout and deadline | N/A — this slice ships no interactive compute request and no loading-deadline UI; its single health fetch fails fast on connection error | Runtime assembly | N/A | N/A | N/A | N/A — no deadline-bound behavior exists in this slice | N/A — not run |
| cancellation and interruption | N/A — the endpoints are stateless with no session or persisted state; the interruption outcome is the documented restart path | Runtime assembly | AC-4 restart procedure | The same origin serves the page and health after restart | AC-4 | N/A — stateless endpoints; restart is the recovery path and is covered by AC-4 | N/A — not run |
| resource bounds and backpressure | Applicable: a hand-written parser must not accept unbounded request headers | Runtime assembly (service) | CTest case sending a request whose header block exceeds 8 KiB | The server answers HTTP 431 and closes the connection without crashing; normal requests continue to succeed | AC-1 | Not Started | Not run — implementation not started |
| framework or trust-boundary rejection | Applicable: the listener must bind loopback only and no other interface may expose the service | Runtime assembly (service) | CTest bind assertion plus a live `lsof` inspection and a LAN-address connection attempt while the service runs | Listener list contains only `127.0.0.1:8080`; connection to the machine's LAN address on the port fails | AC-1, AC-5 | Not Started | Not run — implementation not started |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | The service test suite passes, including the loopback-bind, header-cap, and concurrency cases | Service built after toolchain bootstrap | `ctest` in the service build directory | Exit code 0 with the health-response, static-file, 405, 404, 8-KiB-cap (HTTP 431), loopback-bind, and 8-concurrent-request tests all passing | Test output with named passing tests and exit code | Not Started | Pending |
| AC-2 | T-2 | The web suite and build pass and the page wires the health status | `web/` npm project installed | `npm test --prefix web` then `npm run build --prefix web` | Both exit 0; the compiled page references the status script that fetches `/api/health` | Command outputs and the served HTML/JS reference | Not Started | Pending |
| AC-3 | T-3 | The one documented path starts the assembled runtime on a clean checkout | Clean checkout after documented dependency bootstrap | `scripts/dev.sh`, then `curl -fsS http://127.0.0.1:8080/api/health` | The script prints the origin; curl exits 0 and prints exactly `{"status":"ok"}` | Script and curl transcript with exit codes | Not Started | Pending |
| AC-4 | T-3 | An ordinary stop and restart preserves the browser origin | Running service from AC-3 | Stop the service, start it again through the same path, then `curl -fsS` the same page and health URLs | Both requests exit 0 at the identical `http://127.0.0.1:8080` origin | Restart transcript with both curl exit codes | Not Started | Pending |
| AC-5 | T-3 | No non-loopback listener exposes the service | Running service from AC-3 | `lsof -nP -iTCP:8080 -sTCP:LISTEN`, then connect to the machine's LAN IPv4 address on port 8080 | The listener list shows only `127.0.0.1:8080`; the LAN connection fails | Command outputs | Not Started | Pending |

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
