# ADR-0004: Own request text with offset descriptors

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Complete
- **Date**: 2026-10-16
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-16T09:39:23Z
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
- **Related [Optional]**: docs/reviews/2026-10-16-owned-request-storage.md; docs/adr/ADR-0008-http-parser-benchmarks.md; service/docs/adr/ADR-0003-fragmented-head-offsets.md; docs/adr/ADR-0003-local-runtime-path.md
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

Long field names/values required separate owned strings. The retained experiment replaces those allocations with one owned character buffer and compact offset descriptors. This Full ADR records the source API, value lifetime, allocation/failure and representation decision; all consumers are inside service, so the service ADR root applies. Public HTTP grammar is unchanged.

### Retrospective Scope And Authorization [Required]

On 2026-10-16, owner @hailingu explicitly answered the scoped acceptance and
serialization question with “本次也豁免，直接实施并记录证据”. The exact
scope and earlier execution evidence are retained in `docs/reviews/2026-10-16-owned-request-storage.md`.
The subsequent request “补充文件缺失的 ADR” authorizes this backfill of that
same work. The temporary exception permits these records to coexist with
non-terminal ADRs; it does not change repository-wide serialization policy.

This record was drafted **after** the owner-waived implementation. Its checks
make the proposal reviewable now; they are not represented as pre-implementation
ADR acceptance. Original experimental retention criteria are distinguished in
the cited report. Subtasks marked Complete describe actual exception execution.
At drafting, Decision Status / Implementation Status were Proposed / Not Started.
On 2026-10-16, @hailingu supplied exact `Approve` after the chat identified only
this record; approval was registered at 2026-10-16T09:39:23Z. This record, including
its narrow ordinary value-copy exception, stayed Accepted / In Progress while the
declared immutable commit anchor remained outstanding; on 2026-10-21 that anchor was
bound to `34d598738631ece0ef6023c154fbf3ae52520296` — the commit that carries the
implemented parser and storage sources, their record markers, and this record — and
Implementation Status became Complete. Approval of another record or permission
to implement directly was not counted as approval of this
one. The broader runtime and fixture-lifecycle records retain their existing statuses.

## Scope [Required]

In scope:

- Request owned storage, HeaderField views, method/target/version/headers accessors and coupled copy/move semantics.
- HeadFields materialization, body-framing accessor migration, affected tests and storage-specific allocation/equivalence diagnostics.

Out of scope:

- Caller-owned zero-copy views, arenas, pools, new dependencies or intrinsics, HTTP grammar changes, server integration and CI changes.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Fewer allocations versus owned lifetimes | Borrowed input views dangle when input/decoder changes | Own characters and derive accessor views from current value offsets |
| TN-2 | Bulk copy versus OWS retention | Copying padded wire heads regressed tiny logical fields | Pack validated slices when logical text fits inline capacity or is <=one quarter of wire bytes |
| TN-3 | Value-copy allocation failure versus coded parser errors | Partial copy can pair unrelated bytes and offsets | Strong copy-and-swap for values; coded allocation errors at parse APIs |

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
| Per-field owned strings | Retain existing Request members | Simple independent values | Long components allocate separately |
| Owned buffer plus offsets | Own text once and index it with descriptors | No caller-input lifetime dependency; at most two complete-head allocations in the declared fixtures | Source API migration and view lifetime contract |
| Borrow caller input | Return string_view members directly | Potentially no owned text allocation | Violates independent ownership; previously rejected |
| Arena/pool | Reuse connection-local allocation capacity | Possible steady-state savings | New lifecycle/memory policy beyond this request |

## Decision [Required]

**Selected option**: Owned buffer plus offsets

**Rationale**: This removes measured long-field allocations while preserving owned Request values. It deliberately differs from the rejected caller-input string_view variant: exposed views borrow the Request, whose private buffer owns all characters.

- **OS-1**: Request privately owns text and field offsets. method(), target() and version() return read-only string_views; headers() returns a sized/indexable ordered range of HeaderField views. Case, duplicates, order, trimmed values and tokens are preserved. Accessor views expire on owner destruction, assignment or moving from it; iterators additionally require their returned range object to remain alive. Retaining a field past its owner requires an explicit owned copy.
- **OS-2**: Copies own independent buffers/descriptors; copy assignment commits the complete coupled representation only after allocation succeeds. Moves are noexcept and leave the source empty. Self-copy/self-move preserve the value. Decoder reset/reuse and caller input destruction do not invalidate previously returned Request owners.
- **OS-3**: Complete-head materialization of the declared long-name/value/method/target fixture uses at most two ordinary allocations; minimal zero-field heads use zero on the declared host library; incomplete/rejected heads allocate no result storage. A tiny logical field with 7,800 discarded OWS octets uses at most one allocation. Dense input may be copied in bulk; sparse validated text is packed under the documented inline-capacity/one-quarter rule. No maximum-sized array is added per Request.
- **OS-4**: parse_request and decoder entry points translate owned-storage bad_alloc into ParseError::resource_exhausted; decoder failures stay sticky until reset. Ordinary value-copy allocation failure may throw bad_alloc with the strong preservation guarantee, under the explicit narrow engineering exception below.
- **OS-5**: Retain this representation only if the same-driver five-pair experiment has long-values-16 median <=85% of baseline, every other one of the sixteen fixture medians <=110%, and complete semantic output is byte-identical across the fixed 95,479-case corpus. Measurements are local evidence, not portable timing or allocation ABI promises.

### Consequences [Required]

Positive: Long-value allocations fall 17→2; the retained experiment reduced long-value latency 47%, realistic-head latency 22.5% and thousand-field latency 33%. Request size was 64 versus 96 bytes on the measured host.

Negative: Callers migrate from mutable string members to read-only accessors and borrowed field views. Some shapes gain one allocation; eight OWS-padded fields measured +7.4% latency despite lower requested bytes.

Mitigations: Document owner/range lifetimes, adapt all service callers together, exercise SSO/heap copy/move/reset and allocation failure, preserve the predeclared <=10% per-case regression budget and retain every raw run.

## Implementation Plan [Required]

**Complete task outcome**: An independently owned Request materializes long components with at most two ordinary allocations while preserving value lifetimes and parser semantics within the recorded retention thresholds.

**Primary implementation boundary**: HTTP parser owned Request value and its required consumers.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Introduce and migrate owned Request values | Request representation, HeadFields construction, body-framing and existing test consumers | Complete | docs/reviews/2026-10-16-owned-request-storage.md four production files and accessor migration; lifetime/failure tests |
| T-2 | Verify allocation bounds, semantic equality and performance retention | Storage tests and supporting shared benchmark/allocation/equivalence drivers | Complete | docs/reviews/2026-10-16-owned-request-storage.md TDD 35→2 allocations, 95,479 equal cases, five paired runs and sanitized suites |

**Affected paths**: `service/src/http_messages.hpp`; `service/src/http_messages.cpp`; `service/src/http_head_fields.hpp`; `service/src/http_body_framing.cpp`; `service/tests/http_request_storage_tests.cpp`; `service/tests/http_message_tests.cpp`; `service/tests/http_request_decoder_tests.cpp`; `service/tests/http_head_fields_tests.cpp`; `service/CMakeLists.txt`; `service/bench/http_parser_workloads.hpp`; `service/bench/http_parser_bench.cpp`; `service/bench/http_parser_allocations.cpp`; `service/bench/http_parser_equivalence.cpp`; `service/bench/verify.cmake`; `service/bench/README.md`.

**Affected evidence files**: `docs/reviews/2026-10-16-owned-request-storage.md`; `docs/reviews/2026-10-16-owned-request-storage-evidence.json`; `docs/reviews/2026-10-16-owned-request-storage-source.patch`; `docs/reviews/2026-10-16-adr-backfill.md`; `docs/reviews/2026-10-16-adr-backfill-evidence.json`; `docs/reviews/2026-10-16-adr-provenance.patch`. These reports and comment-free data are listed here rather than modified to fabricate source-comment provenance. Historical raw data/patches remain unchanged.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/src/http_messages.hpp | orange::http::Request; orange::http::HeaderField; orange::http::detail::HeaderFieldOffsets; orange::http::detail::RequestLineOffsets | N/A — stable symbol or configuration key is sufficient | Own characters; expose lifetime-scoped read-only views | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/src/http_messages.cpp | orange::http::Request::Request; orange::http::Request::operator=; orange::http::Request::append_slice; orange::http::Request::swap; orange::http::parse_request | N/A — stable symbol or configuration key is sufficient | Bulk/packed storage, coupled copy/move and coded parse allocation failure | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/src/http_head_fields.hpp | orange::http::detail::HeadFields::finish | N/A — stable symbol or configuration key is sufficient | Construct current owned Request from validated offsets | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/src/http_body_framing.cpp | orange::http::detail::body_framing | N/A — stable symbol or configuration key is sufficient | Use headers accessor without changing framing policy | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/tests/http_request_storage_tests.cpp | (anonymous namespace)::check_allocations; (anonymous namespace)::check_copies; (anonymous namespace)::check_moves; (anonymous namespace)::check_reset; (anonymous namespace)::check_padding_storage; (anonymous namespace)::check_failures; (anonymous namespace)::check_copy_failure | N/A — stable symbol or configuration key is sufficient | Allocation, lifetime, padding and strong failure guarantees | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/bench/http_parser_allocations.cpp | ::main; ::operator new | N/A — stable symbol or configuration key is sufficient | Separate ordinary allocation counts/bytes | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/bench/http_parser_equivalence.cpp | ::main; (anonymous namespace)::corpus | N/A — stable symbol or configuration key is sufficient | Full semantic stream comparison | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Migrate Request and all accessor consumers back together; do not leave old mutable-member and new view-based callers mixed. The stored source patch reconstructs the prior four parser files for disposable A/B measurement; diagnostic adapters support both APIs. For the current provenance-only changes, reverse the dedicated ADR provenance patch first, as documented in the backfill report. Reject a future implementation revision if correctness or the original numeric retention budgets fail.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

| Rule and affected unit | Measured value or condition | Rationale | Risks | Compensating controls | Owner | Removal, review, or permanent rationale | Verification evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| docs/development/cpp-development-standard.md › Error Handling And Contracts: no exceptions across module API boundaries; orange::http::Request copy constructor/operator= | Ordinary value copies can throw std::bad_alloc; parser entry points instead return resource_exhausted | Request retains standard owned-container value copying; no result-returning copy API is introduced. This preserves prior ordinary value-copy failure behavior while making assignment atomic | Callers copying Request values must handle allocation failure; implicit copies cannot return a coded ParseError | Copy-and-swap; injected failures at both allocation steps; parser/decoder allocation failures are translated separately | @hailingu | Narrow permanent value-copy exception accepted in this record; review whenever the Request copy API or no-exception policy changes | http_request_storage_tests::check_copy_failure and check_failures; docs/reviews/2026-10-16-owned-request-storage.md mutation regression |

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| OS-1 | service/docs/adr/ADR-0004-owned-request-storage.md › Decision | Owned read-only fields, exact semantics and accessor/range lifetimes | AC-1; AC-4 | Destroy input, inspect declarations, exercise all field bytes via equivalence corpus |
| OS-2 | service/docs/adr/ADR-0004-owned-request-storage.md › Decision | Independent copy, strong assignment and noexcept empty-source moves | AC-1 | SSO/heap copy/move/self/reset cases, injected copy failure and move declaration inspection |
| OS-3 | service/docs/adr/ADR-0004-owned-request-storage.md › Decision | At-most-two/zero/one allocation fixtures and bounded packed storage | AC-2 | Ordinary allocation instrumentation, rejected/incomplete/minimal/OWS fixtures; inspect packing predicate |
| OS-4 | service/docs/adr/ADR-0004-owned-request-storage.md › Decision | Coded parser failure, sticky decoder failure and strong ordinary copy exception | AC-3 | Fail each owned allocation; test sticky decoder behavior and destination preservation |
| OS-5 | service/docs/adr/ADR-0004-owned-request-storage.md › Decision | Predeclared performance budgets and byte-exact corpus equality | AC-4; AC-5 | Compare complete semantic streams and all sixteen paired timing ratios |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: field ordering and independent copied/decoded values | Owned Request | AC-1; AC-4 duplicate/case, copies and fragmented outcomes | Same ordered bytes with no cross-owner dependency | AC-1; AC-4 | Pass | docs/reviews/2026-10-16-owned-request-storage.md lifetime/equivalence results |
| timeout and deadline | N/A — a synchronous owned value introduces no clock, deadline or I/O | Owned Request | Inspect affected symbols | No temporal mechanism added | AC-1 | N/A — no temporal boundary | Source/API scope review |
| cancellation and interruption | Applicable: resetting decoder and failed copy/construction interrupt value ownership | Request/decoder ownership | AC-1; AC-3 reset and injected allocation failure | Outstanding returned owners remain valid; failed copy preserves both values; decoder failure sticky | AC-1; AC-3 | Pass | docs/reviews/2026-10-16-owned-request-storage.md reset, failure and mutation checks |
| resource bounds and backpressure | Applicable: sparse padding and many long fields change retained/allocation storage | Request materializer | AC-2 counts and packed-storage review; AC-5 full workload comparison | Specified <=2/0/1 ordinary allocation fixtures pass; storage bounded by capped head/fields | AC-2; AC-5 | Pass | docs/reviews/2026-10-16-owned-request-storage.md allocation and 16-case measurements |
| framework or trust-boundary rejection | Applicable: grammar rejection and bad_alloc must not produce partial Request | Production parse APIs | AC-3; AC-4 real parsers and arbitrary octets | Coded allocation errors; unchanged rejection/error/cursor stream | AC-3; AC-4 | Pass | docs/reviews/2026-10-16-owned-request-storage.md 95,479 cases; ASan/UBSan clean |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Owned value lifetimes and documented source API hold | Short inline/one-field/long-field values; caller destruction, copies, moves, self-assignment and decoder reset | Run http_request_storage_tests; inspect Request accessor lifetime comments, moved-from clearing, noexcept declarations and headers range capture | All lifetime checks exit 0; independent copies retain exact fields; moves clear source; accessors derive only from owned buffer; documented range/iterator lifetime matches implementation | CTest plus stable-symbol semantic review | Pass | docs/reviews/2026-10-16-owned-request-storage.md lifetime tests and source/API review |
| AC-2 | T-2 | Complete-head ordinary allocation bounds hold | Long method/target with sixteen long field names/values; minimal 18-byte head; incomplete/rejected input; one field with 7,800 OWS bytes | Run storage tests and http_message_tests allocation cases; run http_parser_allocations; inspect packing predicate | Long fixture <=2 calls; minimal 0; incomplete/rejected no result allocation; padded tiny field <=1; no per-Request maximum-sized array | Allocation output and packing inspection | Pass | docs/reviews/2026-10-16-owned-request-storage.md 35→2 focused fixture; long-values benchmark 17→2; padding 1 and minimal 0 |
| AC-3 | T-1 | Allocation failure preserves coded parse and strong copy outcomes | Fail steps 1/2 in direct owned parsing, 1/2/3 in whole-feed decoder, and 1/2 in value copy assignment | Run http_request_storage_tests::check_failures/check_copy_failure via CTest; run decoder reset/allocation suite | Direct/decoder failures return resource_exhausted without escape; decoder error sticky until reset; value-copy bad_alloc leaves both complete values intact | CTest and documented exception evidence | Pass | docs/reviews/2026-10-16-owned-request-storage.md failures pass; sequential-member-assignment mutation exits 1 |
| AC-4 | T-2 | All parser observables match the prior representation | Byte-identical equivalence driver and fixtures; seed 0x53544F52; old parser reconstructed from retained patch | Run equivalence binary per revision and cmp complete stdout; repeat candidate with ASan/UBSan | 95,479 records and 66,143,697 bytes equal; candidate stream SHA-256 88e1a83e8a654a57f0b3657cc9097f60c82a694965ca28a5c2a1551e8d855f63; no sanitizer findings | Source IDs, stream counts/hash, cmp status and sanitizer log | Pass | docs/reviews/2026-10-16-owned-request-storage.md and its equivalence evidence |
| AC-5 | T-2 | All sixteen local performance retention budgets pass | Five alternating old/new pairs, eleven calibrated samples/run, same driver, Apple clang -O2 -DNDEBUG without LTO | Follow storage report A/B procedure; compare median of five run medians for every fixture | head_long_values_16 ratio <=0.85 and every other fixture ratio <=1.10 | All ten raw JSON runs, driver/source hashes and calculated ratios | Pass | docs/reviews/2026-10-16-owned-request-storage.md long-values ratio .530; largest other ratio 1.074; all sixteen pass |

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
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Complete | @hailingu responded exactly Approve in the sole context of service/docs/adr/ADR-0004-owned-request-storage.md; registered at 2026-10-16T09:39:23Z |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | Complete | Complete 2026-10-21: T-1/T-2 and AC-1 through AC-5 retain passing evidence and the pre-declared commit anchor is bound to `34d598738631ece0ef6023c154fbf3ae52520296`. All seven touchpoint files of this record match the anchor verbatim (unchanged since `34d5987`); the only two files changed repository-wide since the anchor (`service/CMakeLists.txt` via `87548eb`/`cf1eed6` and `service/src/http_syntax.hpp` via `cf1eed6`) are additive runtime-record changes under docs/adr/ADR-0003-local-runtime-path.md outside every touchpoint of this record. Fresh HEAD `86161b0` verification: `ctest --test-dir service/build --output-on-failure` exit 0 with 7/7 suites including `http_request_storage_tests` and `http_message_tests` green; a disposable Release build with `ORANGE_BUILD_BENCHMARKS=ON` passes its CTest 8/8 including `http_parser_bench_cli`, which cross-checks the allocation and equivalence drivers' shared fixture identities |
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
| 2026-10-16 | Record exact Approve from @hailingu for this path alone at 2026-10-16T09:39:23Z; set Accepted / In Progress, including the already-proposed narrow ordinary value-copy exception; complete A-1, retain implementation evidence and the outstanding pre-completion commit-anchor condition; no decision scope or check changes | @codex |
| 2026-10-21 | Bind the pre-declared immutable commit anchor to `34d598738631ece0ef6023c154fbf3ae52520296`, record fresh HEAD suite and Release-driver verification, complete A-2 through A-8 lifecycle gates, and set Implementation Status In Progress→Complete with the index row synchronized; no decision scope, contract, or check changes | @zcode |
