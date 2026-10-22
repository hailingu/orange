# ADR-0005: Reserve the declared fixed body length once

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Complete
- **Date**: 2026-10-22
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-22T02:15:57Z
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
- **Related [Optional]**: docs/adr/ADR-0008-http-parser-benchmarks.md; service/docs/adr/ADR-0004-owned-request-storage.md; docs/reviews/2026-10-16-http-performance.md
- **Architecture Source [Conditionally Required — product demand]**: N/A — corrective performance maintenance of an existing parser and its verification, not a new product-demand capability
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

A fragmented fixed-length request body regrew its owned buffer geometrically: each `read_content` append reallocated and recopied, so a 1 MiB body arriving in transport slices cost eleven ordinary allocations and 4,173,088 requested bytes for 1,048,576 bytes of content. The framing layer already knows and has already bounded the exact length, so the decoder can reserve it once. This Full ADR records the internal storage-reservation decision; the wire contract is unchanged.

### Retrospective Scope And Authorization [Required]

On 2026-10-22, owner @hailingu instructed in the task chat: “先挑选一个执行优化，然后和现有的bench mark对比。在优化前，先将未提交的内容提交”, and then “补充这个ADR后，进行计划内的下一轮优化”. The first instruction selected and authorized this optimization under the same scoped exception pattern recorded in `docs/adr/INDEX.md` for 2026-10-15/2026-10-16; the second authorizes this backfill. The temporary exception permits this record to coexist with non-terminal ADRs; it does not change repository-wide serialization policy.

This record was drafted **after** the owner-waived implementation. Its checks make the proposal reviewable now; they are not represented as pre-implementation ADR acceptance. Subtasks marked Complete describe actual exception execution. At drafting, Decision Status / Implementation Status were Proposed / Not Started. The red/green evidence lives in commits `ae95ecc` (failing allocation test) and `b419a80` (implementation, fixture, and CLI contract), with `9e96ef6` carrying a machine-formatting-only follow-up. The broader runtime and CI records retain their existing statuses.

On 2026-10-22, @hailingu supplied the exact `Approve` in this task chat and, in the same exchange's required one-record clarification, designated this record as the approval target; approval was registered at 2026-10-22T02:15:57Z. Approval of the sibling record or permission to implement directly was not counted as approval of this one. The implementation commits cited in the touchpoints were already immutable at approval, and fresh verification at the approval HEAD (`e93f0cd`) passed: `ctest --test-dir service/build --output-on-failure` 8/8 suites, `http_request_storage_tests` printing 4 ordinary allocations, `http_response_tests` 60 checks / 0 failures. The only change to the measured parser or benchmark sources since the paired comparison is the clang-format rewrap in `9e96ef6`; no semantic revision intervened. Implementation Status became Complete in the same change.

## Scope [Required]

In scope:

- Reserving the declared Content-Length once when fixed framing resolves, plus the storage-suite allocation and failure-injection checks that pin it.
- The `body_1m_fixed_feed16384` benchmark fixture, the configurable fragmented feed size, the CLI contract fixture count, and the benchmark README prose.

Out of scope:

- Chunked body growth policy (totals are unknown; per-chunk reserves would regress), response serialization, the socket event loop, build flags or LTO, and CI workflow changes.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Exact reservation versus lazy geometric growth | A sender that declares a body but trickles it commits the declared memory earlier than growth-on-arrival would | Accept: the declared length is already bounded by `request_body_cap` (1 MiB) before reservation, connections are few under the loopback model, and the win removes a whole regrowth chain |

### Constraints [Required]

- Preserve the wire grammar, decoded bytes, error precedence, `consumed` accounting, sticky failure, and reset behavior exactly; output equivalence must be byte-identical.
- Keep the existing bad_alloc-to-`resource_exhausted` channel: the new allocation site must fail through it, never by throwing across `feed`.
- No new dependency, no target CPU intrinsics, no build-flag or LTO change; benchmark comparisons use one byte-identical driver against both library revisions.
- Preserve unrelated worktree changes and existing ADR/ADD statuses. No commit, push, deployment, or server assembly is included in this record's scope beyond the cited local commits.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

None — no material questions.

## Decision Drivers [Required]

1. **Observable correctness**: decoded output, errors, and lifecycle must be indistinguishable from the geometric-growth baseline.
2. **Reproducibility**: fixture, flags, and pairing method follow the documented benchmark contract so the comparison is repeatable.
3. **Bounded ownership**: allocation count and requested bytes are explicit, pinned by a test bound rather than inferred.

## Options Considered [Required]

### Option: Retain geometric growth

Keep append-driven reallocation for fragmented fixed bodies.

Pros:

- No code change; memory tracks arrived bytes lazily.

Cons:

- Eleven allocations and roughly 4x requested bytes per 1 MiB fragmented body; the cost is fully avoidable because the length is known.

### Option: Reserve the declared fixed length once

Call `message.body.reserve(remaining)` when fixed framing resolves in `start_body`.

Pros:

- One body allocation; decoded bytes unchanged; failure path reuses the existing coded channel.

Cons:

- Declared length is committed before the bytes arrive (bounded by the existing 1 MiB body cap).

### Option: Reserve per chunk for chunked framing

Reserve `body.size() + chunk` at every chunk boundary.

Pros:

- Bounds chunked growth more tightly.

Cons:

- Chunked totals are unknowable in advance; per-chunk exact reserves turn amortized doubling into a reallocation per chunk — strictly worse. Rejected; chunked keeps geometric growth.

## Decision [Required]

**Selected option**: Reserve the declared fixed length once

**Rationale**: the framing layer has already validated the exact extent against the body cap, so the reservation is bounded, single, and final; the alternatives either keep a measured regrowth chain or regress the chunked path.

- **MS-1**: When `body_framing` resolves to fixed framing, `start_body` reserves exactly `policy.length` for `message.body` before any content byte is appended; `body_framing` has already rejected any length above `request_body_cap`, so the reservation never exceeds 1 MiB.
- **MS-2**: The chunked path is byte-for-byte unchanged: no reservation, per-chunk geometric append, identical budget enforcement.
- **MS-3**: An allocation failure inside the reserve surfaces as `ParseError::resource_exhausted` through the existing sticky `feed` channel; no exception crosses the public boundary.
- **MS-4**: Decoded body bytes, `consumed` accounting, error precedence, pipeline suffix, and reset behavior are unchanged; the equivalence corpus output is byte-identical between revisions.

### Consequences [Required]

Positive:

- Fragmented fixed bodies drop from eleven ordinary allocations / 4,173,088 requested bytes to four / 1,048,872, and the new fixture's median latency falls 61.9% locally.

Negative:

- A declared length is committed up front; a trickling sender holds the reservation (bounded at 1 MiB) sooner than lazy growth would.

Mitigations:

- The cap check precedes the reservation; the storage suite pins the allocation bound and the failure channel; the memory-profile change is disclosed in this record and the implementation commit.

## Implementation Plan [Required]

**Complete task outcome**: fixed-framing bodies reserve their declared, cap-bounded length once at framing resolution — no geometric regrowth — with decoded bytes, errors, and lifecycle unchanged and the improvement measured against a same-driver baseline.

**Primary implementation boundary**: HTTP transport decode — the decoder's owned body buffer.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Reserve the declared fixed length and pin the allocation bound and failure channel | `service/src/http_request_decoder.cpp`; `service/tests/http_request_storage_tests.cpp` | Complete | Red `ae95ecc` (11 allocations observed failing the six-allocation bound, ctest exit 1), green `b419a80` (4 allocations, 8/8 suites pass) |
| T-2 | Make the improvement measurable and check the comparison | `service/bench/http_parser_workloads.hpp`; `service/bench/verify.cmake`; `service/bench/README.md` | Complete | `b419a80` plus formatting `9e96ef6`; paired five-run comparison and equivalence evidence recorded in Acceptance Checks |

**Affected paths**: `service/src/http_request_decoder.cpp`; `service/tests/http_request_storage_tests.cpp`; `service/bench/http_parser_workloads.hpp`; `service/bench/verify.cmake`; `service/bench/README.md`.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/src/http_request_decoder.cpp | orange::http::RequestDecoder::Impl::start_body | N/A — stable symbol or anchor is sufficient | Reserves `policy.length` once for fixed framing after the cap check | Commits `b419a80` (implementation) and `ae95ecc` (red test); tree state of `9e96ef6` is byte-identical for this file |
| service/src/http_body_framing.cpp | orange::http::detail::body_framing | N/A — stable symbol or anchor is sufficient | Rejects any declared length above `request_body_cap` before the reservation runs | Unchanged at `b419a80`; behavior pinned by existing decoder tests |
| service/tests/http_request_storage_tests.cpp | (anonymous namespace)::check_fixed_body_allocations | N/A — stable symbol or anchor is sufficient | Pins the six-allocation bound and per-site failure injection | Commit `ae95ecc`; unchanged since |
| service/bench/http_parser_workloads.hpp | orange::bench::Case::feed; orange::bench::fixtures; orange::bench::fixed | N/A — stable symbol or anchor is sufficient | Configurable transport feed size and the fixed-body fixture | Commit `b419a80`; formatting-only follow-up `9e96ef6` |
| service/bench/verify.cmake | fixture-count check in the `http_parser_bench_cli` contract | N/A — stable symbol or anchor is sufficient | Pins the seventeen-fixture CLI contract | Commit `b419a80` |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Revert commits `9e96ef6`, `b419a80`, and `ae95ecc` together; the reservation, its tests, and the fixture arrive and leave as one unit. No persistent state, deployment, or external consumer exists to migrate.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice exceeds no measured size or nesting cap, adds no dependency or CPU intrinsics, and waives no engineering rule. Cyclomatic complexity: N/A — no configured complexity tool; the one-line addition sits inside the existing `start_body` unit, which remains far below the 60-code-line review threshold. The temporary ADR approval/serialization exception is documented separately above.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| MS-1 | service/docs/adr/ADR-0005-fixed-body-reserve.md › Decision | Fixed framing reserves exactly the cap-bounded declared length once | AC-1, AC-3 | Allocation bound test plus paired fixture measurement |
| MS-2 | service/docs/adr/ADR-0005-fixed-body-reserve.md › Decision | Chunked path behavior is unchanged | AC-4 | Equivalence corpus plus allocation counts on chunked fixtures |
| MS-3 | service/docs/adr/ADR-0005-fixed-body-reserve.md › Decision | Reserve failure returns resource_exhausted, never an exception | AC-1 | Fail-at-every-site injection loop |
| MS-4 | service/docs/adr/ADR-0005-fixed-body-reserve.md › Decision | Decoded bytes, consumed, errors, pipeline and reset are unchanged | AC-2, AC-4 | Full suite plus byte-identical equivalence streams |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: pipelined messages and fragmented feeds must keep ordering and cursors | Connection-local decoder | AC-2 full suite; AC-4 equivalence corpus | Same stop results, suffixes, and ordering for every tested split | AC-2; AC-4 | Pass | ctest 8/8 green at `b419a80`; 95,480 equivalence cases byte-identical |
| timeout and deadline | N/A — synchronous parser has no I/O or clock/deadline ownership | Parser body storage | Inspect changed symbols | No clock, wait, or socket API added | AC-2 | N/A — no temporal boundary | `start_body` diff touches only reservation |
| cancellation and interruption | Applicable: a failed reservation must stay terminal until reset | Connection-local decoder | AC-1 fail-at injection at every allocation site | Every site returns resource_exhausted; the sticky failure persists until reset | AC-1 | Pass | injection loop over all four allocation sites passes |
| resource bounds and backpressure | Applicable: the reservation commits the declared length up front | Parser body storage | AC-1 bound test; existing over-cap decoder tests | Declared lengths above 1 MiB are rejected before reserving; reservations never exceed the cap | AC-1; AC-2 | Pass | `body_framing` rejects over-cap before `start_body`; 4 allocations / 1,048,872 bytes measured |
| framework or trust-boundary rejection | Applicable: Content-Length grammar and over-cap rejection are attacker-controlled input | Wire parser APIs | AC-4 equivalence corpus; AC-2 decoder tests | Identical malformed/body_too_long classification and consumed cursors as baseline | AC-2; AC-4 | Pass | equivalence streams byte-identical over 95,480 cases |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Fragmented fixed body reserves once and fails coded at every site | 262,145-byte declared body fed in 4 KiB fragments | `ctest -R ^http_request_storage_tests$ --output-on-failure` | Exit 0; at most six ordinary allocations; every fail-at step returns resource_exhausted | CTest output with the printed count | Pass | Historical exception execution: red at `ae95ecc` (11 allocations, exit 1); green at `b419a80` prints 4 ordinary allocations, exit 0 |
| AC-2 | T-1 | The full routed suite stays green with zero warnings | Repository root, Debug configuration | `cmake -S service -B service/build && cmake --build service/build && ctest --test-dir service/build --output-on-failure` | 8/8 suites pass; no compiler warning | CTest summary | Pass | Historical exception execution: 8/8 pass at `b419a80`, zero warnings |
| AC-3 | T-2 | The fixed-body fixture improves beyond run-to-run noise with no protected regression | Byte-identical driver built against baseline (reservation reversed) and optimized libraries; Apple clang 21.0.0, arm64, -O2 -DNDEBUG, no LTO; five alternating pairs | Paired medians per `service/bench/README.md` comparison guidance | body_1m_fixed_feed16384 median lower with non-overlapping ranges; every pre-existing fixture within 15% of baseline | Per-run JSON medians | Pass | Historical exception execution: 43,913 ns → 16,730 ns (-61.9%), ranges disjoint (42,553–47,129 vs 16,257–16,981 ns); all pre-existing fixtures within ±2.4% |
| AC-4 | T-2 | Outputs and non-target allocation counts are unchanged | Same paired revisions as AC-3 | Run both `http_parser_equivalence` binaries and `cmp` the binary stdout; compare `http_parser_allocations` reports | Streams byte-identical; checksums agree on all seventeen fixtures; only the new fixture's count changes | cmp exit 0 and allocation JSON | Pass | Historical exception execution: 95,480 semantic cases identical; calls 11 → 4 and requested bytes 4,173,088 → 1,048,872 only on body_1m_fixed_feed16384 |
| AC-5 | T-2 | The bench CLI contract accepts the seventeen-fixture report | Release benchmarks configured | `ctest -R ^http_parser_bench_cli$ --output-on-failure` in the benchmark build | Exit 0 with seventeen fixtures validated | CTest output | Pass | Historical exception execution: 1/1 pass at `b419a80` |

CTest methods run in the disposable build directories created by the recipe in
`service/bench/README.md`. Historical Pass rows cite exception execution, not
ADR approval. Fresh execution is required if an implementation or acceptance
condition changes.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Complete | @hailingu responded exactly Approve in this task chat and designated this record in the required one-record clarification; registered at 2026-10-22T02:15:57Z |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | Complete | T-1/T-2 and AC-1 through AC-5 retain passing evidence; fresh approval-HEAD (`e93f0cd`) verification: ctest 8/8, storage test prints 4 ordinary allocations, response suite 60 checks / 0 failures |
| A-3 | Reciprocal ADD link synchronized, when applicable | N/A — maintenance creates no product-demand candidate | Architecture Source assessment | N/A — no ADD handoff | No ADD candidate selected or changed |
| A-4 | Requirement levels satisfied | Every required section is complete and every conditional trigger assessed | Structured document review at drafting | Complete | Pass — structured review at drafting on 2026-10-22 |
| A-5 | Acceptance checks are decidable | Each row names one subtask, exact input, method, binary result and evidence | Structured check review at drafting | Complete | Pass — structured review at drafting on 2026-10-22 |
| A-6 | Engineering exceptions governed, when applicable | All triggered exceptions have rule, owner, controls, lifecycle and evidence; otherwise reasoned N/A | Engineering Exceptions subsection | Complete | N/A assessed — no engineering exception applies |
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
| 2026-10-22 | Register @hailingu's exact Approve designated to this record at 2026-10-22T02:15:57Z; set Accepted / Complete with fresh approval-HEAD verification recorded; complete A-1 and A-2; synchronize the index row; no decision scope or check changes | @zcode |
