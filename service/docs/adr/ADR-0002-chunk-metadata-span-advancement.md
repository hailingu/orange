# ADR-0002: Advance chunk metadata by bounded spans

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Complete
- **Date**: 2026-10-16
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-16T09:20:11Z
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
- **Related [Optional]**: docs/reviews/2026-10-16-http-performance.md; docs/adr/ADR-0003-local-runtime-path.md; docs/adr/ADR-0008-http-parser-benchmarks.md
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

Long chunk extensions and trailer values were repeatedly dispatched through octet-level transitions. Measurements isolated 24.888 microseconds for a 7,000-byte extension and 17.562 microseconds for a long trailer. This Full ADR records the internal state-machine and resource-boundary decision for advancing runs without changing the wire contract.

### Retrospective Scope And Authorization [Required]

On 2026-10-16, owner @hailingu explicitly answered the scoped acceptance and
serialization question with “本次四项也豁免，直接实施并记录证据”. The exact
scope and earlier execution evidence are retained in `docs/reviews/2026-10-16-http-performance.md`.
The subsequent request “补充文件缺失的 ADR” authorizes this backfill of that
same work. The temporary exception permits these records to coexist with
non-terminal ADRs; it does not change repository-wide serialization policy.

This record was drafted **after** the owner-waived implementation. Its checks
make the proposal reviewable now; they are not represented as pre-implementation
ADR acceptance. Original experimental retention criteria are distinguished in
the cited report. Subtasks marked Complete describe actual exception execution.
At drafting, Decision Status / Implementation Status were Proposed / Not Started.
On 2026-10-16, @hailingu supplied exact `Approve` after the chat identified only
this record; approval was registered at 2026-10-16T09:20:11Z. This record stayed
Accepted / In Progress while its declared immutable commit anchor remained
outstanding; on 2026-10-21 that anchor was bound to
`34d598738631ece0ef6023c154fbf3ae52520296` — the commit that carries the
implemented parser sources, their record markers, and this record — and
Implementation Status became Complete. Approval of another record or permission
to implement directly was not counted as approval of this one. The broader
runtime and fixture-lifecycle records retain their existing statuses.

## Scope [Required]

In scope:

- ChunkLine span advancement, shared bounded hexadecimal accumulation, decoder trailer runs and paired data CRLF advancement.
- Focused grammar/cursor regression tests and existing decoder-budget checks; CMake test registration only as support.

Out of scope:

- Head collection, request storage layout, budgets or error-policy changes, sockets and CI tooling.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Batching versus exact failure cursor | Skipping delimiters or quota boundaries changes classification | Batch only runs within one state; use the established octet transitions at delimiters |

### Constraints [Required]

- Preserve the existing wire grammar, error precedence and independent 8 KiB head / 1 MiB decoded-content / 8 KiB aggregate metadata limits.
- Use the current host Apple clang C++20 toolchain; no production dependency, assembly or architecture intrinsics are introduced.
- Keep historical measurements, their content IDs and their owner-waiver provenance; do not substitute bare HEAD for the measured dirty workspace.
- Preserve unrelated changes and existing ADR/ADD statuses. No commit, push, deployment or server assembly is included.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

None — no material questions remain; unavailable remote execution is explicitly an outstanding check, not assumed evidence.

## Decision Drivers [Required]

1. **Observable correctness**: Preserve parsed values, errors and stopping cursors at the production parser boundary.
2. **Reproducibility**: Separate local measurements from portable guarantees and retain the inputs, commands and source identities.
3. **Bounded ownership**: Keep allocations and lifetime rules explicit without broad refactoring.

## Options Considered [Required]

| Option | Description | Pros | Cons |
| --- | --- | --- | --- |
| Retain byte dispatch | Call the state machine for each metadata byte | Existing grammar implementation | Measured dispatch overhead for long runs |
| Bounded span advancement | Skip valid runs within the current state | Large measured improvement; finite state retained | Requires explicit cursor and cap equivalence checks |
| Unbounded delimiter search | Scan ahead across fragments and budgets | Potentially fewer branches | Can inspect bytes beyond the metadata budget |

## Decision [Required]

**Selected option**: Bounded span advancement

**Rationale**: Long runs are the measured cost; preserving transition ownership and clipping spans before inspection keeps the existing grammar and budget precedence.

- **MS-1**: ChunkLine::feed(string_view) stops at the first non-pending result and returns the same consumed count, decoded size and result as repeated feed(char), including the delimiter or offending octet. Both paths use the same bounded hexadecimal/quota accumulation.
- **MS-2**: Decoder metadata scans are clipped to the remaining 8,192-byte aggregate budget before grammar inspection. Metadata byte 8,193 produces sticky chunk_metadata_too_long; decoded content remains independently capped at 1,048,576 bytes. Fragmentation does not change either result.
- **MS-3**: Trailer token/value runs and a data CRLF pair preserve the existing grammar, decoded body, errors, consumed boundary, pipeline suffix and reset behavior. No extension/trailer text is retained.
- **MS-4**: Retain the optimization only with five-pair same-driver measurements demonstrating faster long extension/trailer medians and no protected minimal/realistic head or 1 MiB-content median above 115% of baseline. Compare raw ranges as well as medians; latency is local evidence.

### Consequences [Required]

Positive: Long metadata runs avoid repeated dispatch while retaining exact byte transition semantics.

Negative: There are byte and span entry points whose cursor semantics must agree.

Mitigations: Share accumulation/transitions and exhaust octets, split sizes, literal quota boundaries and production decoder cap cases.

## Implementation Plan [Required]

**Complete task outcome**: Chunk extensions and trailers advance by bounded runs with unchanged grammar, quota results and stopping cursors, supported by measured improvement.

**Primary implementation boundary**: HTTP decoder chunk-metadata grammar advancement.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Implement and verify bounded metadata advancement | ChunkLine and decoder metadata/trailer phases, focused tests | Complete | docs/reviews/2026-10-16-http-performance.md reports 96,889 span assertions, budget regressions and paired measurements |

**Affected paths**: `service/src/http_chunk_line.hpp`; `service/src/http_chunk_line.cpp`; `service/src/http_request_decoder.cpp`; `service/tests/http_chunk_line_tests.cpp`; `service/tests/http_request_decoder_tests.cpp`; `service/CMakeLists.txt`.

**Affected evidence files**: `docs/reviews/2026-10-16-http-performance.md`; `docs/reviews/2026-10-16-http-performance-evidence.json`; `docs/reviews/2026-10-16-http-performance-source.patch`; `docs/reviews/2026-10-16-adr-backfill.md`; `docs/reviews/2026-10-16-adr-backfill-evidence.json`; `docs/reviews/2026-10-16-adr-provenance.patch`. These reports and comment-free data are listed here rather than modified to fabricate source-comment provenance. Historical raw data/patches remain unchanged.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/src/http_chunk_line.hpp | orange::http::detail::ChunkLineStep; orange::http::detail::ChunkLine::feed(std::string_view) | N/A — stable symbol or configuration key is sufficient | Exact consumed/result span interface | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/src/http_chunk_line.cpp | orange::http::detail::ChunkLine::feed; orange::http::detail::ChunkLine::add_digit | N/A — stable symbol or configuration key is sufficient | Runs reuse bounded accumulation and delimiter transitions | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/src/http_request_decoder.cpp | orange::http::RequestDecoder::Impl::read_metadata; orange::http::RequestDecoder::Impl::read_trailers; orange::http::RequestDecoder::Impl::read_chunk_line | N/A — stable symbol or configuration key is sufficient | Budget clipping, trailer runs and CRLF pairing | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/tests/http_chunk_line_tests.cpp | (anonymous namespace)::compare; (anonymous namespace)::check_lines; (anonymous namespace)::check_octets | N/A — stable symbol or configuration key is sufficient | Compare byte/span grammar and exact stopping position | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Revert span/trailer advancement and its focused target together while preserving the earlier independent metadata budget repair. Reconstruct historical experiments only in disposable copies using the source patches; do not revert unrelated parser or storage work.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice exceeds no measured size or nesting cap, introduces no dependency or CPU intrinsics, and waives no engineering rule. Cyclomatic complexity: N/A — no configured complexity tool; executable-unit spans and nesting were reviewed in the cited experiment report. The temporary ADR approval/serialization exception is documented separately above.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| MS-1 | service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md › Decision | Byte/span result, size and stop-position equality | AC-1 | All octets, splits, malformed syntax and literal quotas |
| MS-2 | service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md › Decision | Independent caps and sticky error before out-of-budget inspection | AC-2 | Exact/over-cap, aggregate, joint-budget and reset fixtures |
| MS-3 | service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md › Decision | Trailer/body/pipeline semantics and finite metadata storage | AC-2 | Existing real decoder APIs, malformed metadata and fragmentation; inspect retained state |
| MS-4 | service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md › Decision | Measured retention rule | AC-3 | Five paired same-driver runs and protected medians |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: state transitions and pipeline cursors must remain ordered across feeds | Connection-local decoder | AC-1; AC-2 fragmented and interleaved instances | Same stop result and suffix for every tested split | AC-1; AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md correctness review |
| timeout and deadline | N/A — synchronous parser has no I/O or clock/deadline ownership | Parser metadata | Inspect changed symbols | No clock, wait or socket API added | AC-2 | N/A — no temporal boundary | Only bounded parser advancement |
| cancellation and interruption | Applicable: a failed or reset decoder must not continue stale metadata | Connection-local decoder | AC-2 sticky failure and reset cases | Failure persists until reset; reset starts an independent stream | AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md lifecycle tests |
| resource bounds and backpressure | Applicable: long runs reach the independent metadata cap | Parser metadata | AC-2 exact 8192/8193 and joint budgets | 8192 permitted when grammar completes; next metadata byte rejected; body budget unchanged | AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md budget regressions |
| framework or trust-boundary rejection | Applicable: invalid controls, quoted escapes and framing bytes | Wire parser APIs | AC-1; AC-2 malformed octet and production decoder cases | Same malformed/too_large classification and consumed cursor as established grammar | AC-1; AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md 96,889 span assertions |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Span grammar agrees with byte transitions | http_chunk_line_tests fixtures over quotas 0/175/1048576; 256 octets and multiple splits | ctest -R ^http_chunk_line_tests$ --output-on-failure | Exit 0, 96,889 assertions and zero failures; literal 0x10 quota/stop assertions pass | CTest output and fixture review | Pass | docs/reviews/2026-10-16-http-performance.md 96,889 checks, zero failures |
| AC-2 | T-1 | Production decoder preserves metadata limits and lifecycle | Existing check_chunked, check_metadata_octets, check_metadata_limits, check_aggregate_metadata, check_joint_chunk_budgets, check_fragments and check_decoder_lifecycle | ctest -R ^http_request_decoder_tests$ --output-on-failure; inspect bounded span and retained state | Exit 0; exact independent limits, errors, consumed boundaries, interleaving and reset assertions pass; no extension/trailer text retained | Named CTest and resource-boundary inspection | Pass | docs/reviews/2026-10-16-http-performance.md optimized/sanitized tests; docs/reviews/2026-10-16-owned-request-storage.md current suites remain green |
| AC-3 | T-1 | Metadata optimization meets the local retention rule | Original eleven workload shapes before/after the first optimization; one byte-identical driver per comparison; Apple clang -O2, no LTO; five alternating pairs | Reproduce paired method in performance report and compare raw_paired_runs/summary in its evidence JSON | Long extension/trailer medians lower than baseline with non-overlapping observed ranges; minimal/realistic head and 1 MiB-content ratios <=1.15 | Raw runs, source IDs and ratio calculation | Pass | docs/reviews/2026-10-16-http-performance.md extension 24.888→1.958 us; trailer 17.562→1.952 us; protected ratios <=1.027 |

CTest methods run in the disposable Release build directory created by the
recipe in `service/bench/README.md`; use `ctest --test-dir "$orange_bench_dir"`
when invoking them from the repository root. Sanitizer comparisons use the
Debug configuration and ASan/UBSan flags recorded in the cited experiment.
Historical source fingerprints describe the measured snapshot; the backfill
report explains reversing the comment-only provenance patch before replay.

Allowed final check statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Historical Pass rows cite exception execution, not ADR approval. Fresh execution
is required if an implementation or acceptance condition changes.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Complete | @hailingu responded exactly Approve in the sole context of service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md; registered at 2026-10-16T09:20:11Z |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | Complete | Complete 2026-10-21: T-1 and AC-1/AC-2/AC-3 retain passing evidence and the pre-declared commit anchor is bound to `34d598738631ece0ef6023c154fbf3ae52520296`. All four touchpoint files of this record match the anchor verbatim (unchanged since `34d5987`); the only two files changed repository-wide since the anchor (`service/CMakeLists.txt` via `87548eb`/`cf1eed6` and `service/src/http_syntax.hpp` via `cf1eed6`) are additive runtime-record changes under docs/adr/ADR-0003-local-runtime-path.md outside every touchpoint of this record. Fresh HEAD `86161b0` verification: `ctest --test-dir service/build --output-on-failure` exit 0 with 7/7 suites, including `http_chunk_line_tests` and `http_request_decoder_tests` green |
| A-3 | Reciprocal ADD link synchronized, when applicable | N/A — maintenance creates no product-demand candidate | Architecture Source assessment | N/A — no ADD handoff | No ADD candidate selected or changed |
| A-4 | Requirement levels satisfied | Every required section is complete and every conditional trigger assessed | Structured review in docs/reviews/2026-10-16-adr-backfill.md | Complete | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-5 | Acceptance checks are decidable | Each row names one subtask, exact input, method, binary result and evidence | Structured check review in docs/reviews/2026-10-16-adr-backfill.md | Complete | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-6 | Engineering exceptions governed, when applicable | All triggered exceptions have rule, owner, controls, lifecycle and evidence; otherwise reasoned N/A | Engineering Exceptions subsection | Complete | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-7 | Contract and baseline risks covered, when applicable | All clause IDs map to explicit checks and all five baseline risks are assessed; applicable rows must pass before implementation review-ready | Traceability, Risk Coverage Matrix and linked evidence | Complete | All clause mappings and five risk assessments reviewed; every applicable risk row is Pass with linked exception-execution evidence |
| A-8 | Governance validation passed | Repository validator exits 0 with no record, index, lifecycle or cross-reference diagnostic | npm run validate --prefix tools/governance-validator | Complete | Accepted-stage validation exits 0 after approval registration; npm test --prefix tools/governance-validator: 303/303 pass; git diff --check exits 0; completion-stage validation exits 0 on 2026-10-21 |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Inactive future-lifecycle guidance — this Accepted record is not retired.
When the archival trigger applies, move this record to `archive/`
in its own ADR root, retain its filename, update its index row and all file
markers, and synchronize reciprocal supersession references in the same change.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-16 | Backfill this one owner-waived implementation decision at the owner's request; preserve historical evidence and Proposed / Not Started until individual approval | @codex |
| 2026-10-16 | Record exact Approve from @hailingu for this path alone at 2026-10-16T09:20:11Z; set Accepted / In Progress, complete approval gate A-1, retain implementation evidence and the outstanding pre-completion commit-anchor condition; no decision scope or check changes | @codex |
| 2026-10-21 | Bind the pre-declared immutable commit anchor to `34d598738631ece0ef6023c154fbf3ae52520296`, record fresh HEAD suite verification, complete A-2 through A-8 lifecycle gates, and set Implementation Status In Progress→Complete with the index row synchronized; no decision scope, contract, or check changes | @zcode |
