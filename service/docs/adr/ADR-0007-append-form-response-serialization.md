# ADR-0007: Serialize responses straight into the outgoing backlog

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Complete
- **Date**: 2026-10-22
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-22T03:01:59Z
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Approve
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
- **Related [Optional]**: docs/adr/ADR-0003-local-runtime-path.md; docs/adr/ADR-0008-http-parser-benchmarks.md; service/docs/adr/ADR-0006-static-rejection-wires.md; docs/reviews/2026-10-22-http-performance.md
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

The event loop's `queue_response` serialized every response into a fresh temporary string and then copied that string wholesale into the connection's outgoing backlog: one extra allocation plus one full-wire copy per response on the hot path. The serializer already precomputes the exact wire size, so appending straight into the caller's buffer is byte-identical without the intermediate. This Full ADR records the append-form serialization decision; the wire contract is unchanged.

### Retrospective Scope And Authorization [Required]

On 2026-10-22, owner @hailingu instructed in the task chat: “挑选一个执行优化，然后和现有的bench mark对比。在优化前，先将未提交的内容提交”. That instruction selected and authorized this optimization round — the response-serialization copy elimination from the agreed analysis ordering — under the same scoped exception pattern recorded in `docs/adr/INDEX.md` for 2026-10-15/2026-10-16 and extended on 2026-10-22, and authorizes this backfill. The temporary exception permits this record to coexist with non-terminal ADRs; it does not change repository-wide serialization policy.

This record was drafted **after** the owner-waived implementation. Its checks make the proposal reviewable now; they are not represented as pre-implementation ADR acceptance. Subtasks marked Complete describe actual exception execution. At drafting, Decision Status / Implementation Status were Proposed / Not Started. The red/green evidence lives in commits `2be8eaa` (missing-API build failure) and `5b404c3` (implementation, fixture, and CLI contract). The broader runtime and CI records retain their existing statuses.

On 2026-10-22, @hailingu supplied the exact `Approve` in the task-chat exchange whose sole approval context was this record; approval was registered at 2026-10-22T03:01:59Z. Approval of the sibling records or permission to implement directly was not counted as approval of this one. The implementation commits cited in the touchpoints were already immutable at approval, and fresh verification at the approval HEAD (`d7be459`) passed: `ctest --test-dir service/build --output-on-failure` 8/8 suites and `http_response_tests` 72 checks / 0 failures. No source revision intervened between measurement and approval; the only post-drafting edit added the consolidated review to Related (approval-preserving metadata). Implementation Status became Complete in the same change.

## Scope [Required]

In scope:

- The `write_response_into` append-form serializer, the `write_response` wrapper refactor, and the response-suite pins for byte equality, prefix preservation, unchanged-on-failure, and fault injection.
- Switching the event loop's `queue_response` to the append form, and the `respond_echo_chunked_1024` benchmark fixture with its CLI contract and README updates.

Out of scope:

- Parser changes, the recv/read-buffer and incoming-cursor paths, keep-alive capacity retention across requests, and CI workflow changes.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Two public serializer entry points versus one serialization implementation | Divergent framing behavior between the owning and append forms | `write_response` delegates to `write_response_into`; both share the single `validate`/`wire_size`/`serialize` pass |

### Constraints [Required]

- The append form emits byte-identical wire output to the owning form, preserves existing destination content, and leaves the destination unchanged on validation or allocation failure; nothing throws across the boundary.
- The event loop's success, fallback, and close-after-flush behavior is unchanged at the socket boundary.
- No new dependency, intrinsics, or build-flag change; benchmark comparisons use one byte-identical driver against both library revisions, with revision adaptation through dependent lookup only.
- Preserve unrelated worktree changes and existing ADR/ADD statuses.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

None — no material questions.

## Decision Drivers [Required]

1. **Observable correctness**: wire bytes, typed rejections, and connection lifecycle are indistinguishable from the owning-form baseline.
2. **Reproducibility**: the new fixture measures the exact per-response pattern the server performs, with the documented same-driver method.
3. **Bounded ownership**: one fewer allocation per response, with the destination's invariants explicit (unchanged on failure).

## Options Considered [Required]

### Option: Retain the temporary string and copy

Keep `queue_response` appending the owned result of `write_response`.

Pros:

- No API addition; one serializer entry point.

Cons:

- Every response pays one allocation plus a full-wire copy that the precomputed wire size already renders unnecessary.

### Option: Append-form serialization into the caller's buffer

Add `write_response_into(response, out)`; `write_response` delegates to it.

Pros:

- Removes the temporary and its copy on every response; failure leaves the destination untouched; one shared implementation.

Cons:

- A second public entry point exists, pinned against the first by tests.

### Option: Move-swap when the backlog is empty

Swap the temporary into `outgoing` when it is empty, append otherwise.

Pros:

- No API change.

Cons:

- Still allocates the temporary, only helps the non-pipelined case, and encodes queue policy inside the server instead of the serializer boundary. Rejected as partial.

## Decision [Required]

**Selected option**: Append-form serialization into the caller's buffer

**Rationale**: the serializer owns the exact wire size, so destination-supplied serialization removes the whole intermediate rather than dodging it conditionally, while delegation keeps one framing implementation behind both entry points.

- **MS-1**: `write_response_into` appends exactly the bytes `write_response` would return, after any existing destination content; validation or allocation failure returns the `ResponseError` and leaves the destination unchanged.
- **MS-2**: `write_response` remains byte- and behavior-identical, implemented as a delegating wrapper.
- **MS-3**: `queue_response` keeps its success, 500-fallback, and `closing_after_flush` behavior, observable unchanged at the socket boundary.
- **MS-4**: The change is retained only with a same-driver paired measurement showing the respond fixture improving beyond run-to-run noise and every pre-existing fixture within 15% of baseline.

### Consequences [Required]

Positive:

- The per-response temporary allocation and full-wire copy are eliminated; the measured respond fixture drops 42.1% median latency and halves its allocations.

Negative:

- The response surface carries two entry points whose outputs must agree.

Mitigations:

- The suite pins append-form equality against the owning form across framing shapes, prefixes, rejections, and allocation failure.

## Implementation Plan [Required]

**Complete task outcome**: responses serialize straight into the caller's outgoing buffer — byte-identical, allocation-free of intermediates, failure-transparent — with the improvement measured against a same-driver baseline.

**Primary implementation boundary**: HTTP response wire-format delivery and its consumption by the event loop's outgoing queue.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Provide the append-form serializer with equality and failure pins | `service/src/http_response.hpp`; `service/src/http_response.cpp`; `service/tests/http_response_tests.cpp` | Complete | Red `2be8eaa` (missing-API build failure observed), green `5b404c3` (72 checks, 0 failures) |
| T-2 | Switch the event loop queue and measure the pattern | `service/src/http_server.cpp`; `service/bench/http_parser_workloads.hpp`; `service/bench/verify.cmake`; `service/bench/README.md` | Complete | `5b404c3`; paired five-run comparison and allocation evidence recorded in Acceptance Checks |

**Affected paths**: `service/src/http_response.hpp`; `service/src/http_response.cpp`; `service/src/http_server.cpp`; `service/tests/http_response_tests.cpp`; `service/bench/http_parser_workloads.hpp`; `service/bench/verify.cmake`; `service/bench/README.md`.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/src/http_response.hpp | orange::http::write_response_into | N/A — stable symbol or anchor is sufficient | Append-form serialization contract with unchanged-on-failure guarantee | Commit `5b404c3` |
| service/src/http_response.cpp | orange::http::write_response_into; orange::http::write_response | N/A — stable symbol or anchor is sufficient | Single validate/wire_size/serialize pass behind both entry points | Commit `5b404c3` |
| service/src/http_server.cpp | (anonymous namespace)::queue_response | N/A — stable symbol or anchor is sufficient | Event-loop queue serializes in place with the unchanged 500 fallback | Commit `5b404c3` |
| service/tests/http_response_tests.cpp | (anonymous namespace)::check_write_into | N/A — stable symbol or anchor is sufficient | Byte equality, prefix preservation, rejection and fault pins | Commit `2be8eaa`; machine-formatted in `5b404c3` |
| service/bench/http_parser_workloads.hpp | orange::bench::Mode::respond; orange::bench::serialize_adaptive; orange::bench::echo_response_wire_size | N/A — stable symbol or anchor is sufficient | Revision-adaptive respond workload and its independent expected size | Commit `5b404c3` |
| service/bench/verify.cmake | fixture-count check in the `http_parser_bench_cli` contract | N/A — stable symbol or anchor is sufficient | Pins the eighteen-fixture CLI contract | Commit `5b404c3` |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Revert commits `5b404c3` and `2be8eaa` together; the append form, its tests, the server switch, and the fixture arrive and leave as one unit. No persistent state, deployment, or external consumer exists to migrate.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice exceeds no measured size or nesting cap, adds no dependency or intrinsics, and waives no engineering rule. Cyclomatic complexity: N/A — no configured complexity tool; `write_response_into` is a short linear unit and the `queue_response` edit reduces branching, both far below the review thresholds. The temporary ADR approval/serialization exception is documented separately above.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| MS-1 | service/docs/adr/ADR-0007-append-form-response-serialization.md › Decision | Append form equals owning form byte for byte, preserves prefixes, and fails transparently | AC-1 | Shape matrix plus prefix, rejection, and fault-injection pins |
| MS-2 | service/docs/adr/ADR-0007-append-form-response-serialization.md › Decision | write_response behavior unchanged | AC-1 | Existing exact-wire checks pass unmodified through the delegating wrapper |
| MS-3 | service/docs/adr/ADR-0007-append-form-response-serialization.md › Decision | Queue success, fallback, and close lifecycle unchanged | AC-2 | Real-socket server suite plus full routed trio |
| MS-4 | service/docs/adr/ADR-0007-append-form-response-serialization.md › Decision | Measured retention rule | AC-3, AC-4 | Five paired same-driver runs and allocation comparison |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: pipelined responses must append in order without interleaving | Connection-local outgoing queue | AC-2 full suite including keep-alive and pipeline cases | Responses append in handler order; no interleave or truncation | AC-2 | Pass | ctest 8/8 green at `5b404c3` |
| timeout and deadline | N/A — no I/O, clock, or deadline behavior changes | Response wire format | Inspect changed symbols | No temporal API introduced | AC-2 | N/A — no temporal boundary | Diff touches serialization destination only |
| cancellation and interruption | Applicable: a failed serialization must not corrupt queued bytes or the close lifecycle | Response wire format and event loop | AC-1 unchanged-on-failure pins; AC-2 server suite | Destination untouched on error; fallback wire and closing_after_flush as before | AC-1; AC-2 | Pass | 72 response checks pass; server suite green at `5b404c3` |
| resource bounds and backpressure | Applicable: one fewer allocation per response; reserve bounded by the precomputed wire size | Response wire format | AC-1 fault injection; AC-4 allocation comparison | resource_exhausted on reserve failure with no mutation; 2 to 1 calls / 2176 to 1088 bytes on the fixture | AC-1; AC-4 | Pass | Fault-injection pin passes; allocation JSON compared |
| framework or trust-boundary rejection | Applicable: invalid caller input must keep its typed rejection without writing bytes | Response wire format | AC-1 rejection pins; AC-2 suite | Identical ResponseError values with an unchanged destination | AC-1; AC-2 | Pass | Rejection matrix passes at `5b404c3` |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Append form equals owning form and fails transparently | Seven framing shapes with a prefixed destination; three invalid inputs; one allocation-failure injection | `ctest -R ^http_response_tests$ --output-on-failure` | Exit 0; 72 checks, 0 failures; destinations preserved or untouched as specified | CTest output and suite count | Pass | Historical exception execution: red at `2be8eaa` (missing-API build failure), green at `5b404c3` (72 checks, 0 failures) |
| AC-2 | T-2 | Socket-level serving behavior is unchanged | Repository root, Debug configuration | `cmake -S service -B service/build && cmake --build service/build && ctest --test-dir service/build --output-on-failure` | 8/8 suites pass, including the real-socket server suite; zero warnings | CTest summary | Pass | Historical exception execution: 8/8 pass at `5b404c3`, zero warnings, clang-format clean |
| AC-3 | T-2 | The respond fixture improves beyond run-to-run noise with no protected regression | Byte-identical driver built against baseline (production patch reversed) and optimized libraries; Apple clang 21.0.0, arm64, -O2 -DNDEBUG, no LTO; five alternating pairs | Paired medians per `service/bench/README.md` comparison guidance | respond_echo_chunked_1024 median lower with non-overlapping ranges; every pre-existing fixture within 15% of baseline | Per-run JSON medians | Pass | Historical exception execution: 91.0 ns → 52.7 ns (-42.1%), ranges disjoint (88.9–98.6 vs 52.4–53.1 ns); all pre-existing fixtures within ±2.2% |
| AC-4 | T-2 | Allocation count drops only on the new fixture and results agree | Same paired revisions as AC-3; Release benchmark build | Compare `http_parser_allocations` reports; run `ctest -R ^http_parser_bench_cli$` in the Release tree | respond fixture 2 → 1 calls and 2176 → 1088 requested bytes; all other counts unchanged; checksums agree on all eighteen fixtures; CLI contract exits 0 | Allocation JSON and CTest output | Pass | Historical exception execution: allocation comparison as specified; Release ctest 9/9 including the eighteen-fixture CLI contract |

CTest methods run in the disposable build directories created by the recipe in
`service/bench/README.md`. Historical Pass rows cite exception execution, not
ADR approval. Fresh execution is required if an implementation or acceptance
condition changes.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Complete | @hailingu responded exactly Approve in the task-chat exchange whose sole approval context was this record; registered at 2026-10-22T03:01:59Z |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | Complete | T-1/T-2 and AC-1 through AC-4 retain passing evidence; fresh approval-HEAD (`d7be459`) verification: ctest 8/8, response suite 72 checks / 0 failures |
| A-3 | Reciprocal ADD link synchronized, when applicable | N/A — maintenance creates no product-demand candidate | Architecture Source assessment | N/A — no ADD handoff | No ADD candidate selected or changed |
| A-4 | Requirement levels satisfied | Every required section is complete and every conditional trigger assessed | Structured document review at drafting | Complete | Pass — structured review at drafting on 2026-10-22 |
| A-5 | Acceptance checks are decidable | Each row names one subtask, exact input, method, binary result and evidence | Structured check review at drafting | Complete | Pass — structured review at drafting on 2026-10-22 |
| A-6 | Engineering exceptions governed, when applicable | All triggered exceptions have rule, owner, controls, lifecycle and evidence; otherwise reasoned N/A | Engineering Exceptions subsection | Complete | Pass — no engineering exception applies; assessed at drafting on 2026-10-22 |
| A-7 | Contract and baseline risks covered, when applicable | All clause IDs map to explicit checks and all five baseline risks are assessed | Traceability and Risk Coverage Matrix above | Complete | All four clauses mapped; five risk rows assessed with historical Pass/N/A evidence |
| A-8 | Governance validation passed | Repository validator exits 0 with no record, index, lifecycle or cross-reference diagnostic | npm run validate --prefix tools/governance-validator | Complete | Drafting-stage and completion-stage validations exit 0 on 2026-10-22; validator suite 303/303 |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Inactive future-lifecycle guidance — this Accepted record is not retired.
When the archival trigger applies, move this record to `archive/`
in its own ADR root, retain its filename, update its index row and all file
markers, and synchronize reciprocal supersession references in the same change.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-22 | Backfill this one owner-waived implementation decision at the owner's request; preserve historical red/green and measurement evidence and hold Proposed / Not Started until individual approval | @zcode |
| 2026-10-22 | Link the consolidated docs/reviews/2026-10-22-http-performance.md in Related as evidence metadata before approval; no decision content, scope, or check changes | @zcode |
| 2026-10-22 | Register @hailingu's exact Approve supplied in this record's sole approval context at 2026-10-22T03:01:59Z; set Accepted / Complete with fresh approval-HEAD verification recorded; complete A-1 and A-2; synchronize the index row; no decision scope or check changes | @zcode |
