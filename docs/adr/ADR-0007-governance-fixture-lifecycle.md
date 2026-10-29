# Lightweight ADR-0007: Clean up governance test fixtures through their owning tests

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Complete
- **Date**: 2026-10-15
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-19T01:35:00Z
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Approve
- **Approval Context Revision [Optional — informational and non-binding]**: Commit f4f4d25 (exactly represents the approved document content; not approval evidence)
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
- **Related [Optional]**: docs/adr/ADR-0002-governance-validator-adoption.md; docs/adr/ADR-0003-local-runtime-path.md
- **Architecture Source [Conditionally Required — product demand]**: N/A — test-resource lifecycle correction in governance tooling, not product-demand implementation
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

## Context [Required]

Quality-review finding 5 is observed at source revision
`1f876464862c69c5a54424d91fbfd6937983e23c`.
`test/fixtures.mjs::validRepository` creates an
`orange-governance-*` directory with `mkdtempSync` and returns its path,
without registering teardown. Most callers provide no cleanup. One
208-test verification run left 192 new fixture directories; that review removed
only its own newly created output. The rejection-reason suite already uses a
local `repository(t)` wrapper with `t.after`, demonstrating the supported
ownership mechanism.

There is also an outside-repository symlink test with one fixed temporary
filename and cleanup after the validator call; an earlier throw can bypass
that cleanup. It belongs to the same test-resource owner.

This is one lifecycle correction delivered in one pull request. Project
routing is retained for repository governance tooling; this proposal creates
no new service ADR root or repository governance policy.

**Task-scoped drafting authorization**: In this chat on 2026-10-15, the
repository owner explicitly permitted temporarily waiving ADR serialization
to prepare separate repair records for quality-review findings 1, 2, 4 and 5,
while retaining individual approval before implementation. This direct user
instruction permits these proposals to coexist with the unfinished runtime
record; it is not the Accepted-and-Blocked exception and does not change the
general repository policy. Each repair still requires its own exact
`Approve` under `AGENTS.md` Approval and Status.

**Owner-authorized direct repair, 2026-10-16**: The owner subsequently
stated “本次明确豁免 ADR 前置审批与串行限制，直接修复已证实问题”
and requested fixes for the follow-up assessment. The fixture implementation
and its checks have now run under that explicit exception. This is not an
`Approve` of this record. Proposed / Not Started is retained because the
normal state machine does not permit a Proposed record to progress; the
subtask and check evidence below records actual exception execution, without
claiming Accepted / Complete / Verified lifecycle status. Formal acceptance
is still unrecorded. See
`docs/reviews/2026-10-16-peer-assessment-followup.md` and its source/evidence
inventory for the exact workspace represented by these results.

## Decision [Required]

Use an explicit test context for temporary-resource ownership.

- **FX-1**: A test-only `temporaryDirectory(t, prefix)` helper creates a unique directory and immediately registers `t.after` to recursively remove exactly that returned path with `force: true`. Registration precedes fixture population. Deletion errors other than absence propagate through the test hook; there is no broad catch, prefix glob deletion or process-global cleanup registry.
- **FX-2**: `validRepository(t)` obtains its root through that helper. Every caller passes its owning test context, including callers creating several repositories. Fixed Markdown fixture data may become immutable module constants so the resource-owning factory stays within the function cap.
- **FX-3**: Normal completion and a thrown test-body assertion both run teardown. Removing an already-removed owned root is harmless. Two simultaneously running test processes own different roots and cannot delete one another's resources.
- **FX-4**: The outside-path symlink case uses a uniquely allocated, context-owned sibling directory and a file within it. No helper deletes siblings, unrelated sentinels, prior-run fixtures or a supplied parent temporary directory.
- **FX-5**: Cleanup changes resource lifetime only: existing validator assertions, command statuses and dependency versions are preserved.

This proposal covers ordinary runner completion and test failure. A process
terminated before the runner can execute hooks is outside this teardown
contract; it adds no signal manager or operating-system janitor.

## Scope [Required]

In scope:

- `tools/governance-validator/test/fixtures.mjs`: explicit ownership helper and context-aware fixture factory.
- Existing `test/*.test.mjs` call sites: pass the test context and remove duplicate local teardown registration.
- The outside-path fixture in `test/lifecycle-edge-contracts.test.mjs`.
- `test/fixture-lifecycle.test.mjs` and a narrowly scoped `test/fixtures/fixture-lifecycle-child.mjs` driver for subprocess lifecycle checks.

Out of scope:

- Production validator behavior, source/configuration outside the test tree, package scripts and dependencies.
- Removing historical files in the machine's shared temporary directory or handling forced process termination.

## Lightweight Eligibility Check [Required]

- [x] Change is limited to the paths in Scope and can be fully reverted without migration or retained partial behavior.
- [x] Change modifies localized test-resource behavior and its tests; no normative governance document or configuration is changed by implementation.
- [x] No public API, schema, protocol, service boundary, auth, security, user-data model, migration, dependency, build, CI, or deployment behavior changes; only test-owned temporary files are removed.
- [x] No new technology, framework, storage, queue, runtime, provider, or design pattern; reuse the suite's existing node:test teardown mechanism.
- [x] No material product or technical question remains unresolved.
- [x] Real temporary directories, child-runner exit results and filesystem inspection provide deterministic verification.

## Implementation Plan [Required]

**Complete task outcome**: Governance tests automatically remove all temporary
resources they own after success or test-body failure, while preserving
unrelated paths and the existing validation assertions.

**Primary implementation boundary**: Governance test-fixture resource ownership.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Introduce and verify explicit fixture ownership across the existing suite | Shared helper, caller migration, outside-path fixture and subprocess lifecycle regressions; observe current fixtures surviving child completion before adding teardown | Complete | Implemented under the owner exception: fixtures.mjs::temporaryDirectory registers exact-path teardown before population; validRepository requires its context; all callers migrated; the outside-path case has a unique owner. Real child regressions and 303/303 full-suite checks pass, with only the unchanged sentinel remaining in the isolated parent. Source IDs and measurements: docs/reviews/2026-10-16-peer-assessment-followup-evidence.json. Post-approval reverification 2026-10-19: focused fixture-lifecycle runner 6/6 pass; isolated-TMPDIR full suite 303/303 pass leaving only its sentinel; npm run validate and git diff --check pass |

### Stable Implementation Touchpoints [Required]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| tools/governance-validator/test/fixtures.mjs | temporaryDirectory; validRepository | N/A — stable symbol is sufficient | Context-owned allocation registers teardown before population; factory has one fixed-data loop | HEAD 1f87646 plus 2026-10-16 exception workspace; source content IDs in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |
| tools/governance-validator/test/rejection-reason.test.mjs | validRepository(t) call sites | N/A — stable symbol is sufficient | Use the shared owner directly; remove the duplicate local ownership wrapper | HEAD 1f87646 plus 2026-10-16 exception workspace; source content IDs in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |
| tools/governance-validator/test/lifecycle-edge-contracts.test.mjs | test named reports an index path that resolves outside the repository root | N/A — stable test name is sufficient | Replace the shared temporary filename with uniquely owned outside-path state | HEAD 1f87646 plus 2026-10-16 exception workspace; source content IDs in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |

**Migration and rollback**: Migrate all callers in the same slice; do not leave
a context-free compatibility path that silently skips cleanup. Revert the
helper and call-site migration together if the lifecycle check fails. Remove
only this task's own verification output; do not scan or remove historical
shared temporary directories.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — no cap waiver is proposed. New helpers/callbacks and the modified
`validRepository` stay within 60 code lines; migrate its fixed fixture strings
to immutable test data outside the function if needed. Existing callbacks
whose only change is context propagation remain within 80 code lines; review
each one exceeding 60 for its single existing assertion scenario.

At the cited revision, `validate-lifecycle.test.mjs` has 1,163 physical lines
and `validate-structure.test.mjs` has 1,211, above the 1,000-line review
threshold and below the 1,800-line test-file cap. The call-site migration keeps
their existing lifecycle and structural-validation cohorts together and
changes no assertion scenarios. Keep each within 1,800 lines; broad suite
reorganization is a separate maintenance outcome.

## Contract-To-Check Traceability [Required]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| FX-1 | docs/development/software-engineering-standard.md › State, Concurrency, And Failure Boundaries; this record › Decision › FX-1 | Each temporary directory has one test owner; register teardown before population and remove only that path; cleanup errors are not swallowed | AC-1, AC-4 | Real child-runner completion checks plus helper registration/deletion-path and error-propagation review |
| FX-2 | This record › Decision › FX-2 | Every repository fixture is context-owned, including multiple allocations in one test | AC-1, AC-3, AC-4 | Single/multiple-root child probes, the complete suite in an isolated temporary parent and call-site review |
| FX-3 | This record › Decision › FX-3 | Success, test-body throw and already-removed-root teardown are deterministic; concurrent owners stay isolated | AC-1, AC-2 | Child probes with expected exit codes and parent filesystem observations |
| FX-4 | This record › Decision › FX-4 | Outside-path fixtures are uniquely owned and unrelated files/parent directories are preserved | AC-2, AC-3 | Run the real symlink test; sentinel and parent survive while owned directories disappear |
| FX-5 | This record › Decision › FX-5 | Existing validator assertions and dependencies are preserved | AC-3, AC-4 | Routed suite and scope/assertion review |

## Risk Coverage Matrix [Required]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable — simultaneous test processes allocate roots under one temporary parent | Fixture owner | Run two child test processes concurrently; each reports its allocated roots; retain a sibling sentinel | Roots differ, both children exit 0, every reported root disappears, parent and sentinel remain | AC-2 | Pass | Two explicitly coordinated child runners allocate distinct roots; first exit removes only its root while the second remains live; both exit 0 and preserve parent/sentinel. fixture-lifecycle.test.mjs; evidence inventory in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |
| timeout and deadline | Applicable — a broken lifecycle child must not leave the new coordination checks waiting indefinitely | Lifecycle verification harness | Give every child probe a 30-second supervisor deadline; capture readiness, release and exit results | Every successful and deliberate-failure probe reaches its expected exit before 30 seconds; expiry fails the parent check and terminates the child | AC-1, AC-2 | Pass | All six lifecycle tests finish within the 30-second parent deadlines; child test timeout is 25 seconds and parent teardown terminates any unfinished child. See focused runner summary in the follow-up evidence inventory |
| cancellation and interruption | Applicable — a test body throws after fixture creation and skips its remaining statements | node:test teardown | Child probe deliberately throws ORANGE_EXPECTED_FIXTURE_FAILURE after reporting its root | Child exits 1 for the deliberate failure and the root is absent after process completion | AC-1 | Pass | Deliberate body failure exits 1 with ORANGE_EXPECTED_FIXTURE_FAILURE; reported root is absent after close; the parent and sentinel survive. Real runner regression passes |
| resource bounds and backpressure | Applicable — repeated fixture creation accumulates directories or deletes an unrelated path | Fixture owner | Single/multiple-root probes and the complete suite under an isolated temporary parent with a sentinel | All owned roots are removed; sentinel and parent remain; already-removed-root cleanup succeeds | AC-1, AC-2, AC-3 | Pass | Single/multiple/already-removed/outside and concurrent child cases pass; full suite leaves only its unchanged sentinel; real outside-root symlink assertion is retained and passes |
| framework or trust-boundary rejection | Applicable — teardown must operate through the real test runner and outside-repository symlink fixture without following or deleting unrelated state | node:test and filesystem | Real subprocess probes plus the suite's real outside-root symlink rejection case | Expected runner statuses; outside-root rejection assertion retained; only context-owned roots removed | AC-1, AC-2, AC-3 | Pass | Single/multiple/already-removed/outside and concurrent child cases pass; full suite leaves only its unchanged sentinel; real outside-root symlink assertion is retained and passes |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Real runner teardown cleans roots after success and body failure | Parent creates a private TMPDIR and sibling sentinel; child driver imports the real fixture helper, reports allocated paths and exercises success, two roots in one test, an already-removed root, or a body throw with stable fixture marker ORANGE_EXPECTED_FIXTURE_FAILURE | node --test tools/governance-validator/test/fixture-lifecycle.test.mjs forks the child driver using the real node:test runner API with a 30-second parent-test deadline and inspects the parent only after each child completes | Success/multiple/already-removed children exit 0; intentional-failure child exits 1 with the declared marker; each exits before 30 seconds; every reported root is absent; sentinel and parent remain; expiry fails the parent and terminates the child | Failing-first retained-directory result, child statuses/marker, emitted path records, elapsed time and post-exit filesystem results | Pass | Exception execution passed; named test, command summaries and filesystem observations are retained in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |
| AC-2 | T-1 | Concurrent owners and outside-path fixtures stay isolated | Two child probes start concurrently with one private TMPDIR; each reports owned roots; one keeps a root alive while the other finishes; a sibling sentinel exists | Parent coordinates the probes through explicit child readiness/release messages with a 30-second supervisor deadline for each child and observes paths; run the real outside-root symlink case with its uniquely owned directory | Allocated roots differ; completion of one child removes its roots while the other's live root remains; both exit 0 before their deadlines and leave no owned roots; sentinel/parent survive; the real symlink rejection assertion passes; expiry fails the parent and terminates the child | Child readiness/release transcript, path existence observations, elapsed time, statuses and symlink-case result | Pass | Exception execution passed; named test, command summaries and filesystem observations are retained in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |
| AC-3 | T-1 | The complete existing suite leaves no fixture output in an isolated parent | Fresh private TMPDIR containing only a sentinel; run all suites after caller migration and capture logs outside that parent | env TMPDIR=<private-parent> npm test --prefix tools/governance-validator; list that parent after runner completion; npm run validate --prefix tools/governance-validator; git diff --check | All commands exit 0; the temporary parent contains only its unchanged sentinel; all original validator assertions remain passing; no package/dependency diff | Commands, exit codes, runner summary and post-run directory/sentinel observation | Pass | Exception execution passed; named test, command summaries and filesystem observations are retained in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |
| AC-4 | T-1 | Ownership migration and scoped structure satisfy the proposal | Final fixture helper and all migrated callers at a recorded revision | Structured call-site, registration-order, deletion-target, error-propagation, assertion and size review; no source-text regression tests | Every fixture allocation receives its owning context; teardown is registered before writes and targets only returned roots; no broad catch or global registry; new helpers/callbacks and validRepository at most 60 code lines, context-only migrated callbacks at most 80 with reviews above 60, test files at most 1,800 physical lines; original assertions unchanged | Reviewed ownership/size inventory and source revision | Pass | Exception execution passed; named test, command summaries and filesystem observations are retained in docs/reviews/2026-10-16-peer-assessment-followup-evidence.json |

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Complete | @hailingu self-declared the identity and responded with the exact word Approve in the sole context of this record's repository-relative path on 2026-10-19; Approval Time 2026-10-19T01:35:00Z; informational Approval Context Revision recorded at commit f4f4d25 |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Complete | T-1 and AC-1 through AC-4 executed under the owner exception; formal record acceptance recorded 2026-10-19. Post-approval reverification: node --test tools/governance-validator/test/fixture-lifecycle.test.mjs 6/6 pass; isolated-TMPDIR full suite 303/303 pass leaving only its unchanged sentinel; npm run validate --prefix tools/governance-validator and git diff --check pass |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | N/A — maintenance, not product-demand implementation | N/A — Architecture Source is reasoned N/A; no ADD selection or relink |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Complete | Structured follow-up review completed: requirement levels, untriggered archival guidance, one boundary, scoped assertions, source touchpoints and all five risk dimensions checked; exception execution is distinguished from formal approval |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Complete | Structured follow-up review completed: requirement levels, untriggered archival guidance, one boundary, scoped assertions, source touchpoints and all five risk dimensions checked; exception execution is distinguished from formal approval |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | N/A — no engineering cap waiver proposed | Threshold reviews and resulting size requirements are recorded in Implementation Plan |
| A-7 | Contract and baseline risks covered | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | Complete | FX-1 through FX-5 map to executed AC-1 through AC-4; all applicable risk rows pass with exception-execution evidence |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Complete | npm run validate --prefix tools/governance-validator exits 0 after the completion-status fix; final record and repository validation passes |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

This record is Proposed and has not been retired. Retain the following as
inactive future-lifecycle guidance; it does not affect proposal approval or
implementation completion. When the archival trigger in the heading applies:

- [ ] Move this record to `docs/adr/archive/ADR-0007-governance-fixture-lifecycle.md`.
- [ ] Update every source marker and reference to the final archived path in the same change.
- [ ] Preserve reciprocal replacement paths when superseded; otherwise retain Superseded By: None.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with its final path, scope and statuses.
- [ ] Confirm no active record or code marker still cites the former path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-15 | Draft a separate Proposed repair for quality-review finding 5, with one implementation boundary, deterministic acceptance checks, contract traceability and five-dimension risk assessment. The owner explicitly waived serialization for preparing findings 1, 2, 4 and 5 in this chat; individual approval before implementation remains required. No implementation is authorized or claimed | @codex |
| 2026-10-15 | Proposal verification: npm test --prefix tools/governance-validator passes 208/208; npm run validate --prefix tools/governance-validator and git diff --check pass. Four disposable Accepted-stage preflights pass without changing repository approval state. A fresh C++ verification build with Apple clang 21.0.0 passes 2/2 CTest targets without warnings; clang-format check passes. The build used a task-owned temporary directory to preserve the existing service/build output. Structured planning review covers requirement levels, routing and per-root number uniqueness, one boundary/subtask per record, exact acceptance inputs and outputs, contract/risk traceability and truthful unexecuted implementation checks | @codex |

| 2026-10-16 | Owner waived preapproval and serialization for confirmed defects. Implemented the fixture owner and all caller migrations test-first, retained real child-runner failure/concurrency/ownership regressions, and observed 303/303 full-suite passes with no new fixture residue. Added factual execution evidence; did not invent record approval or a terminal lifecycle transition. | @codex |

| 2026-10-19 | Acceptance and completion: @hailingu self-declared the identity and responded with the exact word Approve in the sole context of this record. Decision Status Proposed→Accepted; Implementation Status Not Started→In Progress (upon acceptance)→Complete in the same change, backed by post-approval reverification: node --test tools/governance-validator/test/fixture-lifecycle.test.mjs 6/6 pass; isolated-TMPDIR full suite 303/303 pass leaving only its unchanged sentinel; npm run validate --prefix tools/governance-validator and git diff --check pass. Approval metadata recorded; informational Approval Context Revision at commit f4f4d25; A-1 and A-2 completed | @zcode |
