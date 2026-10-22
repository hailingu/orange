# ADR-0006: Answer rejections with precomputed static wires

## Metadata [Required]

- **Decision Status**: Proposed
- **Implementation Status**: Not Started
- **Date**: 2026-10-22
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: N/A — no approval received
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: N/A — no approval received
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: N/A — no approval received
- **Rejector [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Time [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Evidence [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Reason [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Retired By [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Time [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Evidence [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Reason [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Blocked From [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker And Evidence [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Owner [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Exit Or Recheck Criterion [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Related [Optional]**: docs/adr/ADR-0003-local-runtime-path.md; service/docs/adr/ADR-0005-fixed-body-reserve.md
- **Architecture Source [Conditionally Required — product demand]**: N/A — corrective performance maintenance of an existing serializer and its verification, not a new product-demand capability
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

Every terminal parse error rebuilt its rejection response through `error_response` plus `write_response` — validate, assemble, allocate — although the nine `ParseError` values produce only six byte-distinct wires, all fully determined at compile time. The event loop additionally kept a file-local duplicate of the 500 wire for its serializer-failure fallback. This Full ADR records the decision to serve rejections from precomputed static wires pinned byte-equal to the serializer, so the rejection path never allocates.

### Retrospective Scope And Authorization [Required]

On 2026-10-22, owner @hailingu instructed in the task chat: “先挑选一个执行优化，然后和现有的bench mark对比。在优化前，先将未提交的内容提交”, and then “补充这个ADR后，进行计划内的下一轮优化”. The second instruction selected and authorized this optimization round — the static rejection wires from the agreed analysis ordering — under the same scoped exception pattern recorded in `docs/adr/INDEX.md` for 2026-10-15/2026-10-16, and authorizes this backfill. The temporary exception permits this record to coexist with non-terminal ADRs; it does not change repository-wide serialization policy.

This record was drafted **after** the owner-waived implementation. Its checks make the proposal reviewable now; they are not represented as pre-implementation ADR acceptance. Subtasks marked Complete describe actual exception execution. At drafting, Decision Status / Implementation Status are Proposed / Not Started awaiting individual approval. The red/green evidence lives in commits `2f53c85` (missing-API build failure) and `1b638ba` (implementation). The broader runtime and CI records retain their existing statuses.

## Scope [Required]

In scope:

- The `error_wire` accessor and its six static wires, pinned byte-equal to `write_response(error_response(...))` for every `ParseError` value.
- Switching the event loop's terminal-error branch, handler-failure path, and serializer-failure fallback to the static wires.

Out of scope:

- Serialization of handler-produced responses, parser changes, benchmark fixtures (the rejection path is cold and not benchmark-covered), and CI changes.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Duplicated wire constants versus a single serializer source | A static wire could silently drift from the serializer's output | The response suite pins byte equality for every `ParseError` value, so drift fails the build deterministically |

### Constraints [Required]

- The static wires are byte-identical to the serializer's output; the serializer remains the reference definition and `error_response` keeps its public contract.
- The rejection path performs no allocation beyond the connection's outgoing buffer growth; the `resource_exhausted` rejection must itself be allocation-free.
- Wire bytes, status mapping, and the close-after-flush lifecycle are unchanged; no new dependency, intrinsics, or build-flag change.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

None — no material questions.

## Decision Drivers [Required]

1. **Observable correctness**: the bytes on the wire and the connection lifecycle must be indistinguishable from the serialized path.
2. **Infallible rejection**: a rejection — above all `resource_exhausted` — must not itself depend on allocation.
3. **Single source of truth**: one pinned relationship between the serializer and the static wires, not a parallel specification.

## Options Considered [Required]

### Option: Retain per-rejection serialization

Keep building every rejection through `error_response` and `write_response`.

Pros:

- One serialization path; nothing to pin.

Cons:

- Every rejection validates and allocates for a fully determined output; the 500 fallback duplicates one wire text file-locally.

### Option: Precomputed static wires with a byte-equality pin

Return the closing wire for each `ParseError` from static storage via `error_wire`, tested byte-equal to the serializer for every value.

Pros:

- Zero allocation and zero validation on a terminal path; the fallback duplication collapses into the shared constant; drift is caught by the suite.

Cons:

- A second representation of six wire texts exists, guarded only by the test pin.

### Option: Lazily cached serialized wires

Serialize once per status and cache the string.

Pros:

- No duplicated text.

Cons:

- First-use allocation remains, the cache is mutable state in a single-threaded loop, and the `resource_exhausted` wire still needs a no-allocation guarantee. Rejected.

## Decision [Required]

**Selected option**: Precomputed static wires with a byte-equality pin

**Rationale**: the rejection outputs are compile-time constants in practice, so serving them from static storage removes work without changing bytes; the equality pin against the serializer closes the duplication risk that the lazy-cache option avoids only by keeping allocation and mutable state.

- **MS-1**: `error_wire(error)` returns, for every `ParseError` value, the exact bytes `write_response(error_response(error))` serializes — status line, `Connection: close`, terminating empty line — from static storage with no allocation.
- **MS-2**: The event loop's terminal-parse-error branch, its handler-failure path, and `queue_response`'s serializer-failure fallback append the static wire and set `closing_after_flush`; lifecycle and observable bytes are unchanged.
- **MS-3**: `error_response` and `write_response` are unchanged and remain the reference definition the pin tests against.

### Consequences [Required]

Positive:

- Rejections — including the 500 fallback — never allocate or validate; one duplicated wire text is eliminated.

Negative:

- Six wire texts live in two representations (serializer grammar and static constants).

Mitigations:

- `check_error_wires` pins byte equality for every `ParseError` value; any grammar change to the serializer fails the suite deterministically.

## Implementation Plan [Required]

**Complete task outcome**: terminal rejections are answered from precomputed static wires that are byte-identical to the serializer's output, with the rejection path allocation-free and the connection lifecycle unchanged.

**Primary implementation boundary**: HTTP response wire-format delivery and its consumption by the event loop's rejection path.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Provide `error_wire` with the byte-equality pin | `service/src/http_response.hpp`; `service/src/http_response.cpp`; `service/tests/http_response_tests.cpp` | Complete | Red `2f53c85` (missing-API build failure observed), green `1b638ba` (60 checks, 0 failures) |
| T-2 | Switch the event loop's rejection paths to the static wires | `service/src/http_server.cpp` | Complete | `1b638ba`; full Debug CTest trio 8/8 with zero warnings |

**Affected paths**: `service/src/http_response.hpp`; `service/src/http_response.cpp`; `service/src/http_server.cpp`; `service/tests/http_response_tests.cpp`.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/src/http_response.hpp | orange::http::error_wire | N/A — stable symbol or anchor is sufficient | Allocation-free static rejection wire contract | Commit `1b638ba` |
| service/src/http_response.cpp | orange::http::error_wire; the six `*_wire` constants | N/A — stable symbol or anchor is sufficient | Static wires and their per-error mapping | Commit `1b638ba` |
| service/src/http_server.cpp | (anonymous namespace)::queue_rejection; (anonymous namespace)::append_response; (anonymous namespace)::queue_response | N/A — stable symbol or anchor is sufficient | Rejection, handler-failure, and fallback paths append the static wire with the same lifecycle | Commit `1b638ba` |
| service/tests/http_response_tests.cpp | (anonymous namespace)::check_error_wires | N/A — stable symbol or anchor is sufficient | Byte-equality pin for every ParseError value | Commit `2f53c85`; machine-formatted in `1b638ba` |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Revert commits `1b638ba` and `2f53c85` together; the static wires, the pin tests, and the server switch arrive and leave as one unit. No persistent state, deployment, or external consumer exists to migrate.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice exceeds no measured size or nesting cap, adds no dependency, and waives no engineering rule. Cyclomatic complexity: N/A — no configured complexity tool; `error_wire` is a single switch and `queue_rejection` is two statements, both far below the review thresholds. The temporary ADR approval/serialization exception is documented separately above.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| MS-1 | service/docs/adr/ADR-0006-static-rejection-wires.md › Decision | Static wire equals the serialized rejection byte for byte, for every ParseError | AC-1 | Nine-value byte-equality loop against the serializer |
| MS-2 | service/docs/adr/ADR-0006-static-rejection-wires.md › Decision | Rejection, handler-failure, and fallback paths keep bytes and lifecycle | AC-2 | Real-socket server suite plus full routed trio |
| MS-3 | service/docs/adr/ADR-0006-static-rejection-wires.md › Decision | Serializer and error_response contracts unchanged | AC-1, AC-3 | Pin tests reference the live serializer; existing wire-shape checks unchanged |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable in form: the static views are shared reads in the single-threaded loop | Response wire format | Inspect storage duration; AC-2 suite | Immutable `constexpr` views; no mutable shared state added | AC-2 | Pass | `constexpr std::string_view` constants; 8/8 suites green at `1b638ba` |
| timeout and deadline | N/A — no I/O, clock, or deadline behavior changes | Rejection path | Inspect changed symbols | No temporal API introduced | AC-2 | N/A — no temporal boundary | Diff touches wire bytes selection only |
| cancellation and interruption | Applicable: a rejected connection must still close after flushing | Event loop connection lifecycle | AC-2 server tests over real sockets | `closing_after_flush` set on every rejection path exactly as before | AC-2 | Pass | http_server_tests green at `1b638ba` |
| resource bounds and backpressure | Applicable: the resource_exhausted rejection must not itself allocate | Response wire format | AC-1 pin plus signature inspection | `error_wire` returns a view into static storage; only outgoing growth may allocate | AC-1 | Pass | 60 response checks pass; the accessor cannot allocate by construction |
| framework or trust-boundary rejection | Applicable: every parser rejection must still map to its status and bytes | Wire-format boundary | AC-1 byte equality per ParseError; AC-2 socket-level rejections | Same status mapping and exact closing wires as the serialized path | AC-1; AC-2 | Pass | Nine pins pass; server suite green at `1b638ba` |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Every ParseError's static wire is byte-identical to the serialized rejection | All nine ParseError enumerators | `ctest -R ^http_response_tests$ --output-on-failure` | Exit 0; 60 checks, 0 failures; each `error_wire` equals its `write_response(error_response(...))` | CTest output and suite count | Pass | Historical exception execution: red at `2f53c85` (missing-API build failure), green at `1b638ba` (60 checks, 0 failures) |
| AC-2 | T-2 | Socket-level rejection behavior is unchanged | Repository root, Debug configuration | `cmake -S service -B service/build && cmake --build service/build && ctest --test-dir service/build --output-on-failure` | 8/8 suites pass, including the real-socket server suite; zero warnings | CTest summary | Pass | Historical exception execution: 8/8 pass at `1b638ba`, zero warnings, clang-format clean |
| AC-3 | T-1 | The serializer reference contract is untouched | Existing response-suite wire-shape and error-mapping checks | Same command as AC-2 | The pre-existing exact-wire and status-mapping checks pass unmodified | CTest output | Pass | Historical exception execution: the 400/413/431/500 exact-wire checks pass unchanged at `1b638ba` |

Historical Pass rows cite exception execution, not ADR approval. Fresh execution
is required if an implementation or acceptance condition changes.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Not Started | Pending — awaiting the required approver's response |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | Not Started | Historical exception execution recorded above; formal completion follows approval |
| A-3 | Reciprocal ADD link synchronized, when applicable | N/A — maintenance creates no product-demand candidate | Architecture Source assessment | N/A — no ADD handoff | No ADD candidate selected or changed |
| A-4 | Requirement levels satisfied | Every required section is complete and every conditional trigger assessed | Structured document review at drafting | Complete | Pass — structured review at drafting on 2026-10-22 |
| A-5 | Acceptance checks are decidable | Each row names one subtask, exact input, method, binary result and evidence | Structured check review at drafting | Complete | Pass — structured review at drafting on 2026-10-22 |
| A-6 | Engineering exceptions governed, when applicable | All triggered exceptions have rule, owner, controls, lifecycle and evidence; otherwise reasoned N/A | Engineering Exceptions subsection | Complete | N/A assessed — no engineering exception applies |
| A-7 | Contract and baseline risks covered, when applicable | All clause IDs map to explicit checks and all five baseline risks are assessed | Traceability and Risk Coverage Matrix above | Complete | All three clauses mapped; five risk rows assessed with historical Pass/N/A evidence |
| A-8 | Governance validation passed | Repository validator exits 0 with no record, index, lifecycle or cross-reference diagnostic | npm run validate --prefix tools/governance-validator | Complete | Drafting-stage validation exits 0 on 2026-10-22 |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Inactive future-lifecycle guidance — this Proposed record is not retired.
When the archival trigger applies, move this record to `archive/`
in its own ADR root, retain its filename, update its index row and all file
markers, and synchronize reciprocal supersession references in the same change.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-22 | Backfill this one owner-waived implementation decision at the owner's request; preserve historical red/green evidence and hold Proposed / Not Started until individual approval | @zcode |
