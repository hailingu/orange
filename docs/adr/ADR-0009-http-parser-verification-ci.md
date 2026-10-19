# ADR-0009: Disposable HTTP parser verification in CI

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: In Progress
- **Date**: 2026-10-16
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-16T09:36:20Z
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
- **Related [Optional]**: docs/reviews/2026-10-16-http-performance.md; docs/adr/ADR-0008-http-parser-benchmarks.md
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

A committed benchmark alone does not schedule correctness and magnitude checks. This Full ADR owns one repository verification pipeline that invokes the existing service test/measurement interfaces on the declared macOS toolchain. It changes CI policy and its AGENTS scope-routing entry, so project routing is required. Local command equivalence and YAML syntax are evidence; remote Actions execution has not occurred.

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
this record; approval was registered at 2026-10-16T09:36:20Z. This record is now
Accepted / In Progress. Hosted AC-3, its applicable risk observations and the
declared immutable commit anchor remain outstanding. Approval of another record
or permission to implement directly was not counted as approval of this one.
The broader runtime and fixture-lifecycle records retain their existing statuses.

## Scope [Required]

In scope:

- .github/workflows/http-performance.yml triggers, permissions, job order, timeout, cleanup and summary reporting.
- AGENTS.md scope-routing row for this maintained workflow.

Out of scope:

- Benchmark implementations, parser behavior, exact cross-machine latency promises, release/deployment/publishing and remote dispatch or push.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Continuous evidence versus runner noise | Tight local latency thresholds are not portable | Use loose explicit smoke limits and preserve runner/toolchain metadata |
| TN-2 | Automation versus retained artifacts/credentials | Verification must not become an undocumented release | Read-only checkout without persisted credentials; disposable compiler output |

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
| Local-only checks | Ask each contributor to run tools | No CI configuration | No scheduled pull-request verification |
| Disposable macOS verification job | Run existing correctness, smoke and sanitizer interfaces | Same toolchain family; no release output | Runner variance and unverified remote orchestration |
| Precise cross-platform performance gate | Require matching nanosecond budgets on several platforms | Broader coverage | Unsupported toolchains and misleading thresholds |

## Decision [Required]

**Selected option**: Disposable macOS verification job

**Rationale**: It reuses existing targets and explicit magnitude limits, keeping CI a verification boundary rather than introducing packaging or deployment.

- **CI-1**: The workflow targets service changes and its own file on push and pull requests targeting dev, plus manual dispatch. It uses macos-15, Apple clang and Release -O2 -DNDEBUG for correctness and benchmark smoke; correctness completes before timing. The opt-in benchmark contract is owned by ADR-0008.
- **CI-2**: Repository permissions are contents: read, checkout is pinned to 3d3c42e5aac5ba805825da76410c181273ba90b1 with persist-credentials: false, the job timeout is ten minutes, and same workflow/ref runs cancel superseded runs.
- **CI-3**: The sanitizer phase uses AddressSanitizer and UndefinedBehaviorSanitizer with detect_leaks=0 and halt_on_error=1. Compiler output is isolated under runner.temp and removed in an always step; JSON is retained only in logs/job summary, including an always summary step when the timing file exists. Bash pipeline failure must propagate benchmark failure through tee.
- **CI-4**: AGENTS scope routing declares the workflow, required reading and local YAML plus equivalent optimized/sanitized verification commands. Local syntax/command checks do not imply a successful remote Actions run. A remote-run check remains outstanding until its actual URL and outcome exist.

### Consequences [Required]

Positive: Repository events invoke the actual service targets and record measurement context alongside correctness.

Negative: Remote runner behavior and cancellation/cleanup remain unverified before a real run, and smoke limits are deliberately insensitive to small regressions.

Mitigations: Keep remote checks Not Started; record their exact required evidence and use local command/YAML verification without claiming hosted success.

## Implementation Plan [Required]

**Complete task outcome**: A disposable macOS CI workflow invokes correctness, magnitude checks and sanitizers with bounded permissions/runtime and explicit evidence limits.

**Primary implementation boundary**: Repository HTTP parser CI verification orchestration.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Define and locally verify the parser CI job | Workflow configuration and AGENTS routing | Complete | docs/reviews/2026-10-16-http-performance.md YAML and equivalent optimized/sanitized checks; no remote run claimed |

**Affected paths**: `.github/workflows/http-performance.yml`; `AGENTS.md`.

**Affected evidence files**: `docs/reviews/2026-10-16-http-performance.md`; `docs/reviews/2026-10-16-http-performance-evidence.json`; `docs/reviews/2026-10-16-http-performance-source.patch`; `docs/reviews/2026-10-16-adr-backfill.md`; `docs/reviews/2026-10-16-adr-backfill-evidence.json`; `docs/reviews/2026-10-16-adr-provenance.patch`. These reports and comment-free data are listed here rather than modified to fabricate source-comment provenance. Historical raw data/patches remain unchanged.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| .github/workflows/http-performance.yml | on; permissions; concurrency; jobs.verify; steps | N/A — stable symbol or configuration key is sufficient | Events, least privilege, order, timeout, summary and cleanup | Pre-commit HEAD 1f876464862c69c5a54424d91fbfd6937983e23c plus exception workspace; per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json; add an immutable commit anchor before formal completion |
| AGENTS.md | Scope Routing › .github/workflows/http-performance.yml | N/A — stable symbol or configuration key is sufficient | Required reading and non-interactive verification route | Pre-commit HEAD 1f876464862c69c5a54424d91fbfd6937983e23c plus exception workspace; per-file before/after SHA-256 in docs/reviews/2026-10-16-adr-backfill-evidence.json; add an immutable commit anchor before formal completion |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: Remove or disable only this workflow and its routing row; keep benchmarks and parser changes intact. Do not push or dispatch as part of this documentation task. Any future remote verification must retain its run URL and inspect cleanup/permission outcomes before marking the remote check Pass.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice exceeds no measured size or nesting cap, introduces no dependency or CPU intrinsics, and waives no engineering rule. Cyclomatic complexity: N/A — no configured complexity tool; executable-unit spans and nesting were reviewed in the cited experiment report. The temporary ADR approval/serialization exception is documented separately above.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| CI-1 | docs/adr/ADR-0009-http-parser-verification-ci.md › Decision | Events, host/toolchain and correctness-before-smoke | AC-1; AC-2 | Parse job configuration and run the equivalent local commands |
| CI-2 | docs/adr/ADR-0009-http-parser-verification-ci.md › Decision | Read-only immutable checkout, ten-minute bound and cancellation | AC-1; AC-3 | Inspect configuration; observe superseded hosted runs before recording behavioral Pass |
| CI-3 | docs/adr/ADR-0009-http-parser-verification-ci.md › Decision | Sanitizers, isolated disposable output, summary and failure propagation | AC-1; AC-2; AC-3 | Local sanitizer/cleanup execution plus hosted success/failure/cancellation evidence |
| CI-4 | docs/adr/ADR-0009-http-parser-verification-ci.md › Decision | Verification routing and honest remote status | AC-1; AC-3 | Inspect scope row and require real hosted run URLs |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: correctness precedes timing; superseded same-ref runs cancel | Actions job | AC-1 configuration inspection; AC-3 two hosted same-ref runs | Correctness step precedes smoke; superseded run cancelled | AC-1; AC-3 | Not Started | Order configured; hosted cancellation unexecuted |
| timeout and deadline | Applicable: a hung check is bounded by the configured job timeout | Actions job | AC-1 timeout key; AC-3 hosted job metadata | timeout-minutes is 10 and hosted job honors configuration | AC-1; AC-3 | Not Started | Timeout key present; no hosted evidence |
| cancellation and interruption | Applicable: failure/cancellation must still enter cleanup | Actions runner | AC-3 hosted failure/cancel run logs | Always cleanup step executes and removes the owned verification directory when runner remains available | AC-3 | Not Started | Remote cleanup not observed; forced runner loss cannot guarantee a final step |
| resource bounds and backpressure | Applicable: credentials and retained compiler artifacts must be bounded | Actions runner | AC-1 isolation/permissions inspection; AC-2 local cleanup; AC-3 hosted logs | Read-only token, no persisted checkout credentials, owned compiler directory deleted | AC-1; AC-2; AC-3 | Not Started | Local configuration/commands pass; hosted inspection outstanding |
| framework or trust-boundary rejection | Applicable: real CLI/test failure must fail the hosted job | Actions shell/CTest | AC-2 real CLI failure status; AC-3 hosted failed-step logs | CTest/smoke error produces failing step even when stdout is piped to tee | AC-2; AC-3 | Not Started | Local CLI contract passes; hosted shell propagation outstanding |

Allowed final risk statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
Any applicable row without Pass blocks implementation review-ready and formal completion.

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Workflow and routing encode CI-1 through CI-4 | Saved workflow and AGENTS scope row | ruby -e 'require "yaml"; YAML.load_file(".github/workflows/http-performance.yml")'; inspect named keys and explicit Bash shell semantics | YAML parses; exact trigger/toolchain/permission/timeout/pin/concurrency/order/always/routing values match Decision; no deployment or upload step | YAML exit code and structured configuration inspection | Pass | docs/reviews/2026-10-16-http-performance.md local YAML and configuration review |
| AC-2 | T-1 | Equivalent local optimized, smoke, sanitizer and cleanup commands succeed | Disposable Release benchmarks and Debug sanitizer directories, existing test inputs | Run workflow CMake/CTest and --smoke-scale 1 commands locally; inspect raw statuses; remove owned directories | All CTests and real smoke exit 0; invalid/forced gates retain nonzero CLI statuses; ASan/UBSan zero findings; owned output absent after cleanup | Local logs, smoke JSON and cleanup evidence | Pass | docs/reviews/2026-10-16-http-performance.md first run; docs/reviews/2026-10-16-owned-request-storage.md final Release 6/6, sanitizer 5/5, smoke sixteen cases |
| AC-3 | T-1 | Hosted orchestration demonstrates configured success, rejection and supersession | A future authorized push/PR/manual event on the configured hosted runner; a failing-check run and two overlapping same-ref runs | Inspect real Actions run URLs, step order, logs, summaries, cancellation and cleanup; do not dispatch in this backfill | Success run completes tests/smoke/sanitizers; forced failing check fails the job; newer same-ref run cancels older; available-runner cleanup runs and removes owned directory; job metadata shows ten-minute limit | Actual hosted run URLs and per-step outcomes | Not Started | Not run — no push, dispatch or remote Actions execution is authorized or claimed in this task |

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
| A-1 | ADR approved | Eligible @hailingu supplies exact Approve in the context of this one path; metadata records identity and offset timestamp | Approval metadata | Complete | @hailingu responded exactly Approve in the sole context of docs/adr/ADR-0009-http-parser-verification-ci.md; registered at 2026-10-16T09:36:20Z |
| A-2 | Complete task delivered | All declared subtasks and applicable checks pass; formal lifecycle begins only after approval | Implementation Plan and Acceptance Checks | In Progress | T-1 and AC-1/AC-2 retain passing local implementation evidence; all 22 source/configuration fingerprints still match. Hosted AC-3 and its five applicable risk observations remain Not Started, and the touchpoint commit anchor is outstanding; formal completion is not claimed |
| A-3 | Reciprocal ADD link synchronized, when applicable | N/A — maintenance creates no product-demand candidate | Architecture Source assessment | N/A — no ADD handoff | No ADD candidate selected or changed |
| A-4 | Requirement levels satisfied | Every required section is complete and every conditional trigger assessed | Structured review in docs/reviews/2026-10-16-adr-backfill.md | Pass | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-5 | Acceptance checks are decidable | Each row names one subtask, exact input, method, binary result and evidence | Structured check review in docs/reviews/2026-10-16-adr-backfill.md | Pass | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-6 | Engineering exceptions governed, when applicable | All triggered exceptions have rule, owner, controls, lifecycle and evidence; otherwise reasoned N/A | Engineering Exceptions subsection | Pass | Pass — structured review recorded in docs/reviews/2026-10-16-adr-backfill.md |
| A-7 | Contract and baseline risks covered, when applicable | All clause IDs map to explicit checks and all five baseline risks are assessed; applicable rows must pass before implementation review-ready | Traceability, Risk Coverage Matrix and linked evidence | Not Started | Hosted AC-3 and applicable hosted risks remain unexecuted; configuration/local checks are recorded |
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
| 2026-10-16 | Record exact Approve from @hailingu for this path alone at 2026-10-16T09:36:20Z; set Accepted / In Progress and complete A-1; preserve outstanding hosted AC-3, risk observations and pre-completion commit anchor; no decision scope or check changes and no remote execution | @codex |
