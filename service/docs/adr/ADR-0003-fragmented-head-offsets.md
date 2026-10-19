# ADR-0003: Retain validated offsets for fragmented request heads

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: In Progress
- **Date**: 2026-10-16
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-16T09:30:41Z
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
- **Related [Optional]**: docs/reviews/2026-10-16-http-performance.md; docs/adr/ADR-0003-local-runtime-path.md; docs/adr/ADR-0008-http-parser-benchmarks.md; service/docs/adr/ADR-0004-owned-request-storage.md
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

Fragmented heads validated each completed line and then reparsed the accumulated head at completion. This Full ADR owns only the validated-offset collector and its decoder integration. It records the internal parse-state and bounded-memory decision independently of chunk metadata and the later Request representation.

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
this record; approval was registered at 2026-10-16T09:30:41Z. This record is now
Accepted / In Progress while its declared immutable commit anchor remains
outstanding. Approval of another record or permission to implement directly
was not counted as approval of this one. The broader runtime and fixture-lifecycle
records retain their existing statuses.

## Scope [Required]

In scope:

- HeadFields validated offset collection; shared request-line/field validators; decoder head completion and complete-first-feed path.
- Focused relocation, invalid-line and many-field tests and supporting test registration.

Out of scope:

- Owned Request API redesign, chunk metadata, new HTTP grammar or budget, server and CI pipeline.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Avoiding repeated grammar scans versus per-connection memory | Offsets consume bounded memory | Allocate descriptors only for received fields; store offsets rather than borrowed views |

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
| Reparse the complete buffer | Keep line validation and final whole-head parsing | No retained descriptors | Measured duplicate work on fragmented input |
| Retain validated offsets | Collect offsets with shared validators and materialize after completion | Preserves validation owner; removes final ordinary grammar scan | A bounded descriptor vector per fragmented decoder |
| Keep borrowed views into growing head | Save views directly | Small descriptors | Buffer relocation invalidates pointers |

## Decision [Required]

**Selected option**: Retain validated offsets

**Rationale**: The collector reuses the established grammar and withstands head-buffer relocation; its bounded descriptor memory is an explicit tradeoff for avoiding the ordinary completion reparse.

- **HF-1**: HeadFields::append_line accepts capped CRLF-stripped lines at absolute offsets and delegates grammar to the shared request-line and field validators. Leading empty lines remain pending; only an empty line after a request line completes the head. Invalid syntax or offsets outside the head budget are rejected.
- **HF-2**: HeadFields stores offsets rather than input views. After completion, finish constructs independently owned results preserving tokens, received case, field order, duplicates, trimmed OWS and consumed bytes without repeating the completed head grammar scan.
- **HF-3**: A complete first fragment uses parse_request directly; fragmented input scans newly received bytes. The existing bounded speculative parse and cap-classification parse remain permitted. Descriptors grow only for received fields, under the 8 KiB head cap, with no maximum-sized per-connection inline array. Allocation failure follows the decoder resource_exhausted path.
- **HF-4**: Retain the collector only when the recorded five-pair same-driver experiment lowers the 17-byte-feed median beyond observed run variation and protected minimal/realistic complete heads and 1 MiB-content medians remain <=115% of baseline.

### Consequences [Required]

Positive: Ordinary fragmented-head completion materializes owned fields from already validated offsets.

Negative: The collector adds a vector object and received-field descriptor allocations to fragmented decoders.

Mitigations: Cap input before collection, reserve only for received fields and verify relocation, allocation errors and exact cap classifications. Report speculative/exceptional reparses honestly.

## Implementation Plan [Required]

**Complete task outcome**: Fragmented request heads finish from validated offsets with unchanged wire semantics and improved measured fragmented-feed cost.

**Primary implementation boundary**: HTTP decoder request-head validation and completion.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Implement and verify the validated-offset completion path | Shared validators, HeadFields, decoder head phase and focused tests | Complete | docs/reviews/2026-10-16-http-performance.md test-first APIs, offset-cap regression, relocation tests and five-pair timing evidence |

**Affected paths**: `service/src/http_head_fields.hpp`; `service/src/http_messages.cpp`; `service/src/http_syntax.hpp`; `service/src/http_request_decoder.cpp`; `service/tests/http_head_fields_tests.cpp`; `service/tests/http_request_decoder_tests.cpp`; `service/CMakeLists.txt`.

**Affected evidence files**: `docs/reviews/2026-10-16-http-performance.md`; `docs/reviews/2026-10-16-http-performance-evidence.json`; `docs/reviews/2026-10-16-http-performance-source.patch`; `docs/reviews/2026-10-16-adr-backfill.md`; `docs/reviews/2026-10-16-adr-backfill-evidence.json`; `docs/reviews/2026-10-16-adr-provenance.patch`. These reports and comment-free data are listed here rather than modified to fabricate source-comment provenance. Historical raw data/patches remain unchanged.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/src/http_head_fields.hpp | orange::http::detail::HeadFields; orange::http::detail::HeadLineResult | N/A — stable symbol or configuration key is sufficient | Retain validated offset state rather than input views | Pre-commit HEAD 1f876464862c69c5a54424d91fbfd6937983e23c plus exception workspace; per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json; add an immutable commit anchor before formal completion |
| service/src/http_messages.cpp | orange::http::detail::HeadFields::append_line; orange::http::detail::HeadFields::finish; orange::http::(anonymous namespace)::validate_request_line; orange::http::(anonymous namespace)::validate_field_line | N/A — stable symbol or configuration key is sufficient | One grammar owner and owned materialization | Pre-commit HEAD 1f876464862c69c5a54424d91fbfd6937983e23c plus exception workspace; per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json; add an immutable commit anchor before formal completion |
| service/src/http_syntax.hpp | orange::http::detail::is_token; orange::http::detail::is_tchar; orange::http::detail::is_field_octet | N/A — stable symbol or configuration key is sufficient | Shared octet classifiers used by the head validators | Pre-commit HEAD 1f876464862c69c5a54424d91fbfd6937983e23c plus exception workspace; per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json; add an immutable commit anchor before formal completion |
| service/src/http_request_decoder.cpp | orange::http::RequestDecoder::Impl::read_head; orange::http::RequestDecoder::Impl::head_line | N/A — stable symbol or configuration key is sufficient | Direct first-fragment path and offset-based completion | Pre-commit HEAD 1f876464862c69c5a54424d91fbfd6937983e23c plus exception workspace; per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json; add an immutable commit anchor before formal completion |
| service/tests/http_head_fields_tests.cpp | (anonymous namespace)::check_owned_fields; (anonymous namespace)::check_rejections; (anonymous namespace)::check_many_fields | N/A — stable symbol or configuration key is sufficient | Relocation, grammar, budget and ordered descriptor growth | Pre-commit HEAD 1f876464862c69c5a54424d91fbfd6937983e23c plus exception workspace; per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json; add an immutable commit anchor before formal completion |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Revert the collector and decoder head integration together to the prior bounded reparse path, retaining independent chunk/resource repairs. If rolling back after the Request representation change, use a compatible materializer rather than restoring obsolete field members. Historical source patches are applied only in disposable copies.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice exceeds no measured size or nesting cap, introduces no dependency or CPU intrinsics, and waives no engineering rule. Cyclomatic complexity: N/A — no configured complexity tool; executable-unit spans and nesting were reviewed in the cited experiment report. The temporary ADR approval/serialization exception is documented separately above.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| HF-1 | service/docs/adr/ADR-0003-fragmented-head-offsets.md › Decision | Completion, grammar and capped offsets | AC-1 | Leading/trailing empties, invalid request/field and out-of-budget offset fixtures |
| HF-2 | service/docs/adr/ADR-0003-fragmented-head-offsets.md › Decision | Independent ownership, field semantics and no completion grammar rescan | AC-1; AC-2 | Relocate/destroy input, inspect materializer and exercise fragmented decoder |
| HF-3 | service/docs/adr/ADR-0003-fragmented-head-offsets.md › Decision | Fast path, bounded growth, cap classification and coded allocation failure | AC-2 | Inspect cap-before-append and direct path; execute boundary/allocation/lifecycle regressions |
| HF-4 | service/docs/adr/ADR-0003-fragmented-head-offsets.md › Decision | Local fragmented-head retention rule | AC-3 | Compare paired driver runs and protected fixtures |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: interleaved decoders and fragmented field order | Connection-local head state | AC-1; AC-2 relocation and interleaved-feed fixtures | No cross-instance data and identical ordered fields/cursors | AC-1; AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md head and lifecycle checks |
| timeout and deadline | N/A — no clock, wait or transport deadline is changed | Head parser | Inspect scope | No temporal API introduced | AC-2 | N/A — no temporal boundary | Synchronous parse only |
| cancellation and interruption | Applicable: reset or allocation failure interrupts a partial head | RequestDecoder | AC-2 allocation-failure and lifecycle cases | Failure sticky; reset discards partial offsets and accepts an independent stream | AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md allocation/reset checks |
| resource bounds and backpressure | Applicable: field-vector growth and head cap | Head collector | AC-1; AC-2 many fields, invalid offset and exact head boundaries | Offsets remain inside 8192; growth tied to received fields; no unbounded scan | AC-1; AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md memory and offset-budget review |
| framework or trust-boundary rejection | Applicable: bad request lines, field syntax and overflow tails | Production parser APIs | AC-1; AC-2 actual parser rejection cases | Same malformed/method_too_long/target_too_long/headers_too_long outcomes | AC-1; AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md existing/new parser suites |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Collector preserves owned values and rejects invalid descriptors | check_owned_fields, check_rejections and check_many_fields; 409 fields, relocation and offset 8190 | ctest -R ^http_head_fields_tests$ --output-on-failure; inspect finish construction | Exit 0; exact ordered/cased/trimmed fields retained after input destruction; invalid grammar/offset rejected; no finish grammar scan | CTest and stable-symbol inspection | Pass | docs/reviews/2026-10-16-http-performance.md focused head collector suite; docs/reviews/2026-10-16-owned-request-storage.md updated accessor suite |
| AC-2 | T-1 | Decoder integration preserves public behavior and bounds | Head-boundary, fragmented, allocation-failure and lifecycle fixtures; current parser | Run each of http_message_tests and http_request_decoder_tests through CTest --output-on-failure; inspect read_head and vector growth | Exit 0; full first fragment uses direct parser, head cap enforced before append, expected cap errors/cursors and sticky allocation failures/reset pass | CTest and bounded-growth review | Pass | docs/reviews/2026-10-16-http-performance.md optimized/sanitized checks and memory review |
| AC-3 | T-1 | Fragmented-head timing meets the local retention rule | Original eleven workload shapes and before/after parser snapshots; one byte-identical driver per comparison; five alternating pairs | Follow performance report A/B procedure and inspect raw runs/summary | 17-byte-feed median lower and maximum final per-run median below minimum baseline per-run median (individual noisy samples may overlap); protected head/content ratios <=1.15 | Raw paired JSON and source IDs | Pass | docs/reviews/2026-10-16-http-performance.md 936.2→838.8 ns; protected ratios <=1.027 |

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
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Complete | @hailingu responded exactly Approve in the sole context of service/docs/adr/ADR-0003-fragmented-head-offsets.md; registered at 2026-10-16T09:30:41Z |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | In Progress | T-1 and AC-1/AC-2/AC-3 have passing implementation evidence; all 22 source/configuration comment-snapshot fingerprints still match the backfill evidence. The touchpoint commit anchor remains outstanding before formal completion |
| A-3 | Reciprocal ADD link synchronized, when applicable | N/A — maintenance creates no product-demand candidate | Architecture Source assessment | N/A — no ADD handoff | No ADD candidate selected or changed |
| A-4 | Requirement levels satisfied | Every required section is complete and every conditional trigger assessed | Structured review in docs/reviews/2026-10-16-adr-backfill.md | Pass | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-5 | Acceptance checks are decidable | Each row names one subtask, exact input, method, binary result and evidence | Structured check review in docs/reviews/2026-10-16-adr-backfill.md | Pass | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-6 | Engineering exceptions governed, when applicable | All triggered exceptions have rule, owner, controls, lifecycle and evidence; otherwise reasoned N/A | Engineering Exceptions subsection | Pass | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-7 | Contract and baseline risks covered, when applicable | All clause IDs map to explicit checks and all five baseline risks are assessed; applicable rows must pass before implementation review-ready | Traceability, Risk Coverage Matrix and linked evidence | Pass | All clause mappings and five risk assessments reviewed; historical execution evidence is linked |
| A-8 | Governance validation passed | Repository validator exits 0 with no record, index, lifecycle or cross-reference diagnostic | npm run validate --prefix tools/governance-validator | Pass | Accepted-stage validation exits 0 after approval registration; npm test --prefix tools/governance-validator: 303/303 pass; git diff --check exits 0 |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Inactive future-lifecycle guidance — this Accepted record is not retired.
When the archival trigger applies, move this record to `archive/`
in its own ADR root, retain its filename, update its index row and all file
markers, and synchronize reciprocal supersession references in the same change.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-16 | Backfill this one owner-waived implementation decision at the owner's request; preserve historical evidence and Proposed / Not Started until individual approval | @codex |
| 2026-10-16 | Record exact Approve from @hailingu for this path alone at 2026-10-16T09:30:41Z; set Accepted / In Progress, complete approval gate A-1, retain implementation evidence and the outstanding pre-completion commit-anchor condition; no decision scope or check changes | @codex |
