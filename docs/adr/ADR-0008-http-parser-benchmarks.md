# ADR-0008: Reproducible HTTP parser benchmarks

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Complete
- **Date**: 2026-10-16
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-16T09:10:58Z
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
- **Related [Optional]**: docs/reviews/2026-10-16-http-performance.md; docs/reviews/2026-10-16-owned-request-storage.md; docs/adr/ADR-0009-http-parser-verification-ci.md; service/docs/adr/ADR-0004-owned-request-storage.md
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

Historical nanosecond claims had no repository-owned executable. The owner requested reproducible parser workloads and a loose magnitude guard. This Full ADR covers the opt-in measurement target and its build/CLI contract; the independently deployable Actions pipeline is a separate decision. Allocation and semantic-comparison drivers added for the later storage experiment are supporting measurement clients of the same workload owner.

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
At drafting, Decision Status / Implementation Status were Proposed / Not
Started because the earlier implementation waiver did not approve this record.
On 2026-10-16, @hailingu supplied the exact `Approve` after the chat explicitly
identified only `docs/adr/ADR-0008-http-parser-benchmarks.md`. That response
accepts this record alone; approval metadata records the processing time in UTC.
Implementation Status was set to In Progress at approval because the touchpoints
required the already-declared immutable commit anchor before formal completion.
On 2026-10-21 that anchor was bound to `34d598738631ece0ef6023c154fbf3ae52520296`
— the commit that carries the implemented benchmark sources, their record
markers, and this record — and Implementation Status became Complete. The other
four backfilled records and the broader runtime/fixture records retain their
own lifecycle states.

## Scope [Required]

In scope:

- Release-only optional timing, allocation and revision-equivalence executables; deterministic workload definitions and CLI checks.
- The ORANGE_BUILD_BENCHMARKS configuration key, test registration and benchmark reproduction instructions.

Out of scope:

- Production parser optimizations, server throughput/SLOs, external benchmark dependencies and remote CI configuration.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Measurement visibility versus instrumentation overhead | Counting allocations inside a timer biases latency | Use separate counting and timing executables with identical fixtures |
| TN-2 | Regression detection versus shared-runner variance | Small fixed latency budgets produce noise | Default to reporting; enable explicit loose per-case magnitude limits |

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
| Keep ad hoc programs | Retain reports without drivers | No maintained tooling | Measurements cannot be rerun from the repository |
| Repository-owned standard-library drivers | Share deterministic fixtures across separate executables | No new dependency; byte-identical A/B drivers | Requires CLI and fixture maintenance |
| Add a benchmark framework | Introduce a third-party harness | More framework features | Dependency and configuration cost beyond this scope |

## Decision [Required]

**Selected option**: Repository-owned standard-library drivers

**Rationale**: This makes the already-measured parser paths reproducible with the declared toolchain while keeping counting instrumentation outside timing.

- **BM-1**: ORANGE_BUILD_BENCHMARKS defaults OFF; ON requires CMAKE_BUILD_TYPE=Release and builds http_parser_bench, http_parser_allocations and http_parser_equivalence. The timing driver is separately compiled without IPO.
- **BM-2**: Timing and counting use the same sixteen deterministic fixtures. Each validates its observed result against fixture facts before reporting. Timings exclude input generation, include fresh decoder construction where applicable, owned materialization, observation and destruction, and report compiler, flags, architecture, iterations, repetitions, minimum, median, maximum and checksum.
- **BM-3**: Default timing is report-only. --quick selects three samples and 1 ms calibration instead of eleven and 5 ms. --smoke-scale accepts only positive finite values, multiplies each fixture limit and exits 1 on a selected gate or fixture failure; invalid invocation exits 2 and success exits 0.
- **BM-4**: Allocation counts/bytes refer to ordinary allocation calls around the workload, excluding fixture generation and JSON emission. The separate equivalence driver emits all parsed tokens, ordered field bytes, body bytes, errors and cursors for a deterministic seeded corpus; binary stdout comparison is the equality method. Neither metric is a network-throughput claim.

### Consequences [Required]

Positive: The measurement inputs and failure statuses can be reproduced without external dependencies.

Negative: Results remain host/toolchain dependent, and loose gates do not detect small regressions.

Mitigations: Retain raw A/B samples and source fingerprints, alternate execution order and compare run medians; keep allocation and semantic diagnostics separate.

## Implementation Plan [Required]

**Complete task outcome**: A repository-owned Release benchmark and diagnostics report the actual parser workloads and reject malformed invocations or explicitly failed gates.

**Primary implementation boundary**: Parser verification tooling and its opt-in build/CLI interface.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Implement reproducible drivers and build/CLI checks | service/bench/** and ORANGE_BUILD_BENCHMARKS | Complete | docs/reviews/2026-10-16-http-performance.md; docs/reviews/2026-10-16-owned-request-storage.md record test-first CLI failures, successful reports and allocation/equivalence checks |

**Affected paths**: `service/CMakeLists.txt`; `service/bench/README.md`; `service/bench/http_parser_bench.cpp`; `service/bench/http_parser_workloads.hpp`; `service/bench/http_parser_allocations.cpp`; `service/bench/http_parser_equivalence.cpp`; `service/bench/verify.cmake`.

**Affected evidence files**: `docs/reviews/2026-10-16-http-performance.md`; `docs/reviews/2026-10-16-http-performance-evidence.json`; `docs/reviews/2026-10-16-http-performance-source.patch`; `docs/reviews/2026-10-16-adr-backfill.md`; `docs/reviews/2026-10-16-adr-backfill-evidence.json`; `docs/reviews/2026-10-16-adr-provenance.patch`. These reports and comment-free data are listed here rather than modified to fabricate source-comment provenance. Historical raw data/patches remain unchanged.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/CMakeLists.txt | ORANGE_BUILD_BENCHMARKS; http_parser_bench_cli | N/A — stable symbol or configuration key is sufficient | Opt-in Release gate and executable/CTest registration | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); later additive commits `87548eb` and `cf1eed6` under docs/adr/ADR-0003-local-runtime-path.md register response/server sources and test targets and leave `ORANGE_BUILD_BENCHMARKS` and `http_parser_bench_cli` unchanged; historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/bench/http_parser_bench.cpp | (anonymous namespace)::measure; (anonymous namespace)::arguments; (anonymous namespace)::report; ::main | N/A — stable symbol or configuration key is sufficient | Calibrated measurement, JSON observations and failure status | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/bench/http_parser_workloads.hpp | orange::bench::fixtures; orange::bench::run; orange::bench::expected_result | N/A — stable symbol or configuration key is sufficient | Independent fixture facts shared by measurement clients | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/bench/http_parser_allocations.cpp | ::operator new; ::main | N/A — stable symbol or configuration key is sufficient | Count ordinary workload allocations outside timing | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/bench/http_parser_equivalence.cpp | ::main; (anonymous namespace)::corpus | N/A — stable symbol or configuration key is sufficient | Emit reproducible complete semantic records | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |
| service/bench/verify.cmake | execute_process; string(JSON) | N/A — stable symbol or configuration key is sufficient | Exercise real executable statuses and report shape | Immutable anchor commit `34d598738631ece0ef6023c154fbf3ae52520296` (carries this implemented source and its record marker); historical pre-commit HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus exception workspace with per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Disable the opt-in flag to leave normal correctness targets available. Revert only the benchmark block/bench files to remove tooling; parser implementations remain independently reversible. Delete disposable compiler output after verification.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice exceeds no measured size or nesting cap, introduces no dependency or CPU intrinsics, and waives no engineering rule. Cyclomatic complexity: N/A — no configured complexity tool; executable-unit spans and nesting were reviewed in the cited experiment report. The temporary ADR approval/serialization exception is documented separately above.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| BM-1 | docs/adr/ADR-0008-http-parser-benchmarks.md › Decision | Release-only optional targets, no driver IPO | AC-1 | Configure default, Release ON and Debug ON; inspect compile configuration |
| BM-2 | docs/adr/ADR-0008-http-parser-benchmarks.md › Decision | Identical sixteen validated fixtures and complete timing report | AC-1; AC-2 | CTest drives both executables and checks identities and observations; inspect timed scope and report fields |
| BM-3 | docs/adr/ADR-0008-http-parser-benchmarks.md › Decision | Calibration/sample selection, report-only default, positive finite scale and exact exit statuses | AC-2 | Run CLI test plus invalid argument/value cases and inspect calibrated reports |
| BM-4 | docs/adr/ADR-0008-http-parser-benchmarks.md › Decision | Separate allocation accounting and full binary semantic comparison | AC-3 | Inspect counting scope and compare the recorded old/new semantic streams |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: paired executions must not overlap timing or disagree on fixture order | Measurement driver | AC-2; AC-3 serial runs and identity checks | Same ordered fixture names; old/new execution order alternates | AC-2; AC-3 | Pass | docs/reviews/2026-10-16-owned-request-storage.md paired runs |
| timeout and deadline | N/A — no production deadline changes; calibration is iteration-bounded and benchmark runtime is verification-only | Measurement driver | Inspect measure iteration ceiling | Iterations do not exceed 2^26 | AC-2 | N/A — no production deadline | measure bounds calibration |
| cancellation and interruption | N/A — synchronous local executables introduce no cancellation API or running/shared state | Measurement driver | Inspect ownership and scope | Only process-local output and disposable compiler files | AC-1 | N/A — no cancellation mechanism | No runtime mutation |
| resource bounds and backpressure | Applicable: counters and diagnostic setup must not pollute timed allocations | Measurement driver | AC-2; AC-3 inspect scope and run reporters | Fixture construction outside timed/counted scope; sixteen matching reports | AC-2; AC-3 | Pass | docs/reviews/2026-10-16-owned-request-storage.md allocation report |
| framework or trust-boundary rejection | Applicable: malformed CLI and failed requested latency limits | Executable CLI | AC-2 real executable calls | Invalid exit 2; forced gate exit 1; valid report exit 0 | AC-2 | Pass | docs/reviews/2026-10-16-http-performance.md and docs/reviews/2026-10-16-owned-request-storage.md CLI checks |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Build configuration honors the opt-in contract | Default OFF, Release ON and Debug ON in separate disposable directories | Configure/build Release per service/bench/README.md; configure Debug with ORANGE_BUILD_BENCHMARKS=ON; inspect target IPO key | Release creates all three executables; default omits them; Debug ON configure fails with Parser benchmarks require CMAKE_BUILD_TYPE=Release; timing target IPO FALSE | Configure logs and target inspection | Pass | docs/reviews/2026-10-16-http-performance.md and docs/reviews/2026-10-16-owned-request-storage.md optimized builds/configuration checks |
| AC-2 | T-1 | Measurement reports and CLI statuses obey BM-2/BM-3 | Release drivers; valid default, --quick, --smoke-scale 1; --unknown; missing scale; scales 0,-1,nan,inf | ctest -R ^http_parser_bench_cli$ --output-on-failure; execute listed CLI inputs and inspect JSON plus measure scope | Sixteen ordered validated cases; default eleven/quick three samples; positive report fields and checksum; valid exit 0, invalid exit 2, forced tiny gate exit 1 | JSON, command statuses and timed-scope inspection | Pass | docs/reviews/2026-10-16-http-performance.md CLI checks; docs/reviews/2026-10-16-owned-request-storage.md final sixteen-case reports |
| AC-3 | T-1 | Allocation reporting and semantic comparison stay separate and reproducible | Byte-identical shared drivers against pre-storage and final parser; corpus seed 0x53544F52 | Run http_parser_allocations; follow storage report reconstruction and run http_parser_equivalence twice, then cmp binary outputs; inspect count/timing scopes | Same sixteen fixture names; allocation results observed; 95,479 semantic records compare byte-for-byte; no allocation counter in timed driver | Driver hashes, allocation JSON, cmp exit status and scope review | Pass | docs/reviews/2026-10-16-owned-request-storage.md; 66,143,697 equal bytes, SHA-256 88e1a83e8a654a57f0b3657cc9097f60c82a694965ca28a5c2a1551e8d855f63 |

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
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Complete | @hailingu responded exactly Approve in the sole ADR-0008 context; registered at 2026-10-16T09:10:58Z |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | Complete | Complete 2026-10-21: T-1 and AC-1/AC-2/AC-3 retain passing evidence and the pre-declared commit anchor is bound to `34d598738631ece0ef6023c154fbf3ae52520296`. 20 of the 22 backfill fingerprints match the current workspace verbatim; the only two files changed since the anchor (`service/CMakeLists.txt` via `87548eb`/`cf1eed6`, adding response/server sources and test targets, and `service/src/http_syntax.hpp` via `cf1eed6`, adding `iequals`) are additive changes under docs/adr/ADR-0003-local-runtime-path.md that leave every benchmark touchpoint unchanged. Fresh HEAD `86161b0` verification: `ctest --test-dir service/build --output-on-failure` exit 0 with 7/7 suites; a disposable Release build with `ORANGE_BUILD_BENCHMARKS=ON` builds all three executables and its CTest passes 8/8 including `http_parser_bench_cli`; a Debug `ORANGE_BUILD_BENCHMARKS=ON` configure fails with `Parser benchmarks require CMAKE_BUILD_TYPE=Release`; disposable directories removed after the run |
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
| 2026-10-16 | Record exact Approve from @hailingu for this path alone at 2026-10-16T09:10:58Z; set Accepted / In Progress, complete approval gate A-1, retain implementation evidence and the outstanding pre-completion commit-anchor condition; no decision scope or check changes | @codex |
| 2026-10-21 | Bind the pre-declared immutable commit anchor to `34d598738631ece0ef6023c154fbf3ae52520296`, record the fresh HEAD verification of the opt-in build and CLI contract, complete A-2, and set Implementation Status In Progress→Complete with the index row synchronized; no decision scope, contract, or check changes | @zcode |
