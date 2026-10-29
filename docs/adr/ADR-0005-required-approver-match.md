# ADR-0005: Match concrete required approvers during approval validation

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Verified
- **Date**: 2026-10-15
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-15T09:37:07Z
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
- **Related [Optional]**: docs/adr/ADR-0002-governance-validator-adoption.md; docs/adr/ADR-0003-local-runtime-path.md
- **Architecture Source [Conditionally Required — product demand]**: N/A — corrective governance-tool maintenance, not product-demand implementation
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

Quality-review finding 1 is reproducible at source revision
`1f876464862c69c5a54424d91fbfd6937983e23c`. The shared
`validateApprovalMetadata` function checks the approving actor's syntax,
timestamp, exact evidence and separation from the author. It never compares
the actor to a concrete `Required Approver`.

In a disposable copy of the repository documentation, changing only the
Accepted governance-adoption record's Approver from `@hailingu` to
`@quality-probe-outsider`, while retaining Required Approver `@hailingu`,
still produced exit 0 and `Governance validation passed.`. No repository
approval metadata was changed by that probe.

Failing-first verification also exposed an incorrect planning assumption:
`validateRequiredMetadata` already requires a concrete actor for Required
Approver. A fixture using `Decision Owner after independent review` exits 1
with `metadata field Required Approver must be a concrete @<actor-id>`.
Although the template permits an actor or rule, the existing validator does
not support descriptive rules. This repair preserves that baseline behavior;
adding rule support or reconciling the wider template contract is outside
this task. The original AC-3 exit-0 expectation was incorrect and has been
replaced with the observed exit-1 baseline, requiring reapproval.

This is one correction to the approval-validation boundary, delivered in one
implementation pull request. It enforces an already declared eligibility
restriction; it neither proves a human's identity nor grants approval.

**Task-scoped drafting authorization**: In this chat on 2026-10-15, the
repository owner explicitly permitted temporarily waiving ADR serialization
to prepare separate repair records for quality-review findings 1, 2, 4 and 5,
while retaining individual approval before implementation. This direct user
instruction permits these proposals to coexist with the unfinished runtime
record; it is not the Accepted-and-Blocked exception and does not change the
general repository policy. Each repair still requires its own exact
`Approve` under `AGENTS.md` Approval and Status.

## Scope [Required]

In scope:

- The active-metadata check in `tools/governance-validator/validate.mjs::validateApprovalMetadata`.
- Focused CLI regression fixtures in `tools/governance-validator/test/approval-authorization.test.mjs`.
- A bounded capability note in the validator README explaining concrete-actor comparison and the existing lack of descriptive-rule support.

Out of scope:

- Rejection and retirement actor authorization, role-rule interpretation, actor aliases, authentication integration and approval-history reconstruction.
- Lifecycle changes, the serialization validator, dependencies, runtime code and product capabilities.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Templates permit an actor or rule, while the current metadata validator only accepts concrete actors | Expanding rule support would change an independent existing gate and exceed the demonstrated actor-mismatch correction | Add concrete actor equality while preserving the existing descriptive-rule rejection; disclose the unsupported template form in the capability note |

### Constraints [Required]

- `AGENTS.md` Approval and Status remains the approval authority: an eligible non-author actor and exact `Approve` are required.
- Reuse the existing active-metadata parser and `isValidActor`; do not infer identities from repository ownership or machine login.
- No package, command, dependency or exit-status convention changes.
- The file is 761 physical lines at the cited revision: above the 600-line decomposition-review threshold and below the 800-line cap. The change stays in the existing approval owner; the CLI's record dispatch and validators remain colocated for this patch, with the resulting file kept within 800 lines. This is a threshold review, not a cap waiver.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

N/A — the supported concrete-actor form and comparison result are explicit. The newly observed descriptive-rule rejection is retained, rather than adding rule support; the revised AC-3 baseline is presented for owner reapproval.

## Decision Drivers [Required]

1. **Eligibility enforcement**: an unrelated, syntactically valid actor must not satisfy an explicit actor restriction.
2. **Compatibility**: preserve the existing metadata, lifecycle and approval gates without inventing a rule language.
3. **Narrow correction**: reuse the shared approval check used by Accepted ADRs/OCRs and Current ADDs.

## Options Considered [Required]

### Option: Compare concrete required actors in the shared check

Use the existing actor predicate on active Required Approver and reject an
otherwise-valid approving actor that differs.

Pros:

- Covers all three document families at their existing approval gate.
- Requires no new rule parser or identity provider.

Cons:

- The template's descriptive-rule form remains unsupported by the existing metadata validator.

### Option: Introduce an approver-rule language

Parse actor lists, roles and delegation into an authorization policy.

Pros:

- Could eventually automate additional eligibility policies.

Cons:

- Adds a new governance language without a captured requirement and exceeds the demonstrated defect.

## Decision [Required]

**Selected option**: Compare concrete required actors in the shared check.

**Rationale**: The recorded failure is a direct actor mismatch, so the existing
actor predicate and active metadata provide sufficient information.

- **RA-1**: Whenever approval validation is triggered and active Required Approver is a valid concrete actor, a valid active Approver must equal that actor exactly. A mismatch produces exit 1 with diagnostic code `APPROVAL_ACTOR_NOT_AUTHORIZED` and the record path.
- **RA-2**: Matching Required Approver never exempts self-approval, invalid actor, invalid timestamp or non-`Approve` evidence from the existing gates.
- **RA-3**: Preserve the existing concrete-actor metadata gate: a descriptive Required Approver still exits 1 under that gate and receives no new rule interpretation. Proposed/Draft documents do not acquire an approval requirement from this correction. Authorized values appearing only in comments, fenced examples or Change Log do not replace active metadata.

### Consequences [Required]

Positive:

- The demonstrated unrelated-actor approval is rejected across the shared approval boundary.

Negative:

- The existing validator continues to reject descriptive Required Approver rules even though the template permits that form; this bounded correction does not add rule support.

Mitigations:

- Document the existing rule-support limitation and cover its rejection baseline alongside matching, mismatching and inactive-metadata fixtures.

## Implementation Plan [Required]

**Complete task outcome**: The CLI rejects a concrete required-actor mismatch
with a stable diagnostic while preserving valid approvals and existing
approval-stage behavior.

**Primary implementation boundary**: Governance validator approval policy.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Enforce and document concrete required-actor eligibility with regression coverage | Shared approval check, focused CLI fixtures and validator capability note; reproduce mismatch failure before implementation, then run the routed suite | Complete | validate.mjs::validateApprovalMetadata enforces RA-1 without bypassing RA-2; approval-authorization.test.mjs covers RA-1–RA-3 through the real CLI; README records concrete-actor behavior and the existing rule limitation. Source content IDs and commands are recorded below |

**Affected paths**: `tools/governance-validator/validate.mjs`,
`tools/governance-validator/test/approval-authorization.test.mjs`,
`tools/governance-validator/README.md`.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| tools/governance-validator/validate.mjs | validateApprovalMetadata; isValidActor | N/A — stable symbols are sufficient | Compare active actor restriction at the existing shared approval boundary | 1f876464862c69c5a54424d91fbfd6937983e23c |
| tools/governance-validator/validate.mjs | validateRequiredMetadata | N/A — stable symbol is sufficient | Preserve the existing concrete-actor gate, including descriptive-rule rejection; no implementation change to this function | 1f876464862c69c5a54424d91fbfd6937983e23c |
| tools/governance-validator/lib/metadata-validation.mjs | createMetadataValidator | N/A — stable symbol is sufficient | Existing parser supplies only active metadata; reuse without changing its contract | 1f876464862c69c5a54424d91fbfd6937983e23c |
| tools/governance-validator/test/fixtures.mjs | acceptedAdr; acceptedOcr; validRepository | N/A — stable symbols are sufficient | Reuse complete fixtures and mutate only eligibility inputs | 1f876464862c69c5a54424d91fbfd6937983e23c |

**Migration and rollback strategy [Conditionally Required — this replaces or
changes existing behavior]**: No stored-data migration. A newly rejected record
must be reviewed against its recorded authorization evidence; never rewrite its
approver merely to make validation pass. Revert only this implementation slice
to restore the previous validator behavior, retaining its failing regression as
evidence until the correction is reapplied.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — no engineering cap is waived. Keep changed executable units within 60
code lines and the CLI within 800 physical lines; the existing file-threshold
review is recorded in Constraints.

**Verified implementation evidence**: Content identities below were computed
with `git hash-object <path>` over the uncommitted source bytes. No commit,
push or reusable build artifact was created. The source baseline remains
`1f876464862c69c5a54424d91fbfd6937983e23c`; the parser, dependency manifests
and existing fixture builders have no diff from that baseline.

| Affected path | Stable implementation anchor | Git blob content ID |
| --- | --- | --- |
| tools/governance-validator/validate.mjs | validateApprovalMetadata | 7cc83e17ffcd65acea55f83eda48bce349221405 |
| tools/governance-validator/test/approval-authorization.test.mjs | RA-1, RA-2 and RA-3 CLI cases; approvalFixture; assertUnauthorized | 92ebd3c374b474e16f817df266796965dfc0b73a |
| tools/governance-validator/README.md | Governance Validator capability note | ff8a68d2225a80fa157dedc921f5ed6eada229d3 |

**Structured engineering review**: The changed approval function is 18 code
lines excluding its internal comment. New helpers have 11, 5, 9, 17 and 7
code lines; new callbacks are shorter, and all changed units stay within the
60-line constraint. The CLI is 768 physical lines and the new test file is
155, within their caps. Executable nesting stays within four levels.
Cyclomatic complexity: N/A — no configured complexity tool; unit spans and
nesting were reviewed instead. The CLI's existing threshold review in
Constraints still applies: this patch adds one bounded policy check inside
the existing approval owner, with no unrelated extraction or dependency.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| RA-1 | AGENTS.md › Approval and Status; docs/adr/ADR-0005-required-approver-match.md › Decision › RA-1 | A concrete required actor restricts approval to that exact actor; mismatches fail with a record-scoped stable diagnostic | AC-1 | Parameterized real CLI fixtures for Full ADR, Lightweight ADR, OCR and ADD differ only in active Approver |
| RA-2 | AGENTS.md › Approval and Status; this record › Decision › RA-2 | Required-actor equality does not authorize self-approval or replace the timestamp and exact evidence gates | AC-2 | Matching actor fixtures separately violate each existing gate and must still fail |
| RA-3 | tools/governance-validator/validate.mjs › validateRequiredMetadata at the cited source revision; this record › Decision › RA-3 | Descriptive rules retain existing exit-1 rejection; inactive metadata never repairs an active mismatch; unapproved stages remain unapproved | AC-3 | CLI fixtures exercise descriptive-rule rejection, Proposed/Draft stages and mismatch plus authorized history/comment/example |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | N/A — the correction compares synchronous immutable metadata; it introduces no shared state or ordering operation | Approval policy | Inspect the scoped change and run the routed suite | No new shared mutable state; existing suite exits 0 | AC-4 | N/A — synchronous comparison | N/A — no concurrent operation |
| timeout and deadline | N/A — no I/O, retry, timer or new input scan is added by actor equality | Approval policy | Inspect the comparison and run the routed suite | Direct existing-parser lookup and bounded string comparison; suite exits 0 | AC-4 | N/A — no deadline-bearing operation | N/A — no timer |
| cancellation and interruption | N/A — the check is synchronous and changes no process lifecycle | Approval policy | Inspect the scoped change | No cancellation handler or retained state is added | AC-4 | N/A — no interruptible operation | N/A — no retained state |
| resource bounds and backpressure | N/A — comparison reuses parsed metadata and allocates no queue, connection or growing collection | Approval policy | Inspect the scoped change and unit/file measurements | Changed units remain within the declared guardrails; no collection is added | AC-4 | N/A — no accumulating resource | N/A — no backpressure boundary |
| framework or trust-boundary rejection | Applicable — a foreign actor is well formed but not the actor explicitly required by the active record | Approval policy | Run mismatch fixtures through the real CLI, including misleading inactive authorized metadata | Exit 1, record path and APPROVAL_ACTOR_NOT_AUTHORIZED; valid matching fixtures exit 0 | AC-1, AC-2, AC-3 | Pass | AC-1, AC-2 and AC-3 all pass after reapproval; focused real-CLI suite 27/27 and routed suite 235/235. Concrete outsiders and misleading inactive metadata are rejected with path plus APPROVAL_ACTOR_NOT_AUTHORIZED |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Concrete required-actor equality is enforced for every approval-bearing document family | Otherwise-valid Accepted Full ADR, Accepted Lightweight ADR, Accepted OCR and Current ADD fixtures; author @codex, Required Approver @linhai, valid timestamp and Approve evidence; compare Approver @linhai versus @quality-probe-outsider | Named parameterized CLI cases in test/approval-authorization.test.mjs via node --test tools/governance-validator/test/approval-authorization.test.mjs | Every matching fixture exits 0; every outsider fixture exits 1 and reports its record path plus APPROVAL_ACTOR_NOT_AUTHORIZED | Failing-first outsider result, passing parameterized report, CLI status and diagnostics, implementation revision | Pass | After reapproval, red run: all four outsider fixtures return 0 instead of 1; green run: all four matching fixtures exit 0 and all four outsider fixtures exit 1 with their path and APPROVAL_ACTOR_NOT_AUTHORIZED. The case-sensitive identity fixture also passes. Focused report: 27/27; source identities below |
| AC-2 | T-1 | An actor match cannot bypass existing approval gates | Otherwise-valid accepted fixture variants: Author and Required Approver and Approver all @codex; Required Approver and Approver both @Human; matching @linhai with timestamp 2026-99-99T99:99:99+99:99; matching @linhai with evidence approve | Focused CLI regressions plus existing self-approval and metadata tests | Each variant exits 1; corresponding valid non-author controls exit 0 | Case inputs and CLI exit results | Pass | All four isolated invalid variants exit 1: self-approval, @Human role label, impossible timestamp and lowercase approve evidence. Matching non-author controls for all four families exit 0. Focused report: 27/27 |
| AC-3 | T-1 | Existing stage, rule and active-metadata boundaries remain intact | Otherwise-valid fixture with Required Approver set to Decision Owner after independent review and Approver @linhai; Proposed ADR and Draft ADD with approval fields N/A — not approved; AC-1 outsider fixtures supplemented with @linhai approval metadata only in a comment, fenced example or Change Log | Focused CLI regressions using the existing parser and complete fixture builders; inspect validateRequiredMetadata remains unchanged | Descriptive-rule fixture retains exit 1 under the existing concrete-actor gate; unapproved-stage controls retain exit 0; every active mismatch exits 1 with APPROVAL_ACTOR_NOT_AUTHORIZED | Named test results, unchanged existing metadata gate and explicit limitation review | Pass | Descriptive Required Approver exits 1; Proposed ADR and Draft ADD with unapproved fields exit 0; all twelve inactive-metadata cases (three locations × four families) exit 1 with their path and APPROVAL_ACTOR_NOT_AUTHORIZED. Diff review confirms validateRequiredMetadata and the metadata parser are unchanged |
| AC-4 | T-1 | Routed validation and size constraints pass after the correction | Accepted implementation slice with no unrelated source changes | npm test --prefix tools/governance-validator; npm run validate --prefix tools/governance-validator; git diff --check; structured scope and size review | All commands exit 0; changed executable units at most 60 code lines; validate.mjs at most 800 physical lines; no dependency or metadata-parser change | Command summaries, revision, unit/file measurements and reviewed diff | Pass | npm test --prefix tools/governance-validator: 235/235 pass, 0 failures, 26.988 seconds. npm run validate --prefix tools/governance-validator and git diff --check exit 0. CLI 768 physical lines; changed approval function 18 code lines; longest new test helper 17 code lines; no changed executable unit exceeds 60. Dependency and metadata-parser diff is empty |

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Complete | @hailingu replied with exact Approve for the revised record in this chat at 2026-10-15T09:37:07Z; identity was explicitly declared in the same approval context |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Complete | T-1 is Complete; AC-1 through AC-4 pass with real CLI results and source content IDs; applicable trust-boundary risk is Pass |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | N/A — maintenance, not product-demand implementation | N/A — Architecture Source is reasoned N/A; no ADD selection or relink |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Complete | Final structured review confirms all required sections and lifecycle fields are complete, every conditional trigger has evidence or a specific N/A reason, and no active completion evidence remains Pending |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Complete | All four accepted checks retain explicit inputs, binary expected results and deterministic methods; the executed CLI cases and scope review cover each requirement without source/prose equality assertions |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | N/A — no engineering cap waiver proposed | Threshold reviews and resulting size requirements are recorded in Implementation Plan |
| A-7 | Contract and baseline risks covered, when applicable | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | Complete | RA-1, RA-2 and RA-3 map to passing AC-1, AC-2 and AC-3; all five baseline risk dimensions were reviewed. Applicable trust-boundary rejection passes with four families plus twelve inactive-metadata cases; the other four dimensions retain their specific N/A assessments |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Complete | npm run validate --prefix tools/governance-validator exits 0 after the implementation; final record/index validation is included in the completion command summary |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

This record is Accepted and has not been retired. Retain the following as
inactive future-lifecycle guidance; it does not affect proposal approval or
implementation completion. When the archival trigger in the heading applies:

- [ ] Move this record to `docs/adr/archive/ADR-0005-required-approver-match.md`.
- [ ] Update every source marker and reference to the final archived path in the same change.
- [ ] Preserve reciprocal replacement paths when superseded; otherwise retain Superseded By: None.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with its final path, scope and statuses.
- [ ] Confirm no active record or code marker still cites the former path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-15 | Draft a separate Proposed repair for quality-review finding 1, with one implementation boundary, deterministic acceptance checks, contract traceability and five-dimension risk assessment. The owner explicitly waived serialization for preparing findings 1, 2, 4 and 5 in this chat; individual approval before implementation remains required. No implementation is authorized or claimed | @codex |
| 2026-10-15 | Proposal verification: npm test --prefix tools/governance-validator passes 208/208; npm run validate --prefix tools/governance-validator and git diff --check pass. Four disposable Accepted-stage preflights pass without changing repository approval state. A fresh C++ verification build with Apple clang 21.0.0 passes 2/2 CTest targets without warnings; clang-format check passes. The build used a task-owned temporary directory to preserve the existing service/build output. Structured planning review covers requirement levels, routing and per-root number uniqueness, one boundary/subtask per record, exact acceptance inputs and outputs, contract/risk traceability and truthful unexecuted implementation checks | @codex |
| 2026-10-15 | @hailingu declared their identity and then replied with exact Approve in this chat's context identifying only this record. Approval recorded at 2026-10-15T09:25:55Z; Decision Status becomes Accepted and T-1 enters In Progress. No other repair ADR is approved by this response | @codex |
| 2026-10-15 | Historical failing-first run at source revision 1f876464862c69c5a54424d91fbfd6937983e23c: 27 focused CLI cases, 9 pass and 18 fail. Seventeen failures expose missing actor equality, including all four document families and inactive-metadata cases; each wrongly returned exit 0. The remaining failure disproves AC-3's planning assumption: a descriptive Required Approver already returns exit 1 from validateRequiredMetadata. No production source was changed; the temporary regression draft was removed after recording this evidence so no known-failing suite remains in the worktree | @codex |
| 2026-10-15 | Approval invalidated at 2026-10-15T09:28:36Z: preserve the existing descriptive-rule rejection in RA-3 and correct AC-3 from exit 0 to exit 1, without expanding rule support. Prior Approver: @hailingu; prior Approval Time: 2026-10-15T09:25:55Z; prior Approval Evidence: Approve; Approval Context Revision: none — the approved document was uncommitted. Decision Status returns to Proposed, Implementation Status to Not Started, active approval fields become Pending — reapproval required, and all implementation checks must be rerun after reapproval | @codex |
| 2026-10-15 | Revised-proposal review: RA-3, AC-3, traceability and capability-note scope now agree with the observed concrete-actor gate; requirement levels, conditional triggers, one boundary/subtask and all five risk dimensions were reviewed. After discarding the temporary test draft, npm test --prefix tools/governance-validator passes 208/208 (0 failures, 22.912 seconds); npm run validate --prefix tools/governance-validator and git diff --check pass. There is no source diff under tools/governance-validator; disposable fixtures and test output were deleted. This is proposal verification, not completion of T-1 | @codex |
| 2026-10-15 | @hailingu reapproved this revised record with exact Approve in this chat. Approval recorded at 2026-10-15T09:37:07Z; Decision Status becomes Accepted and T-1 enters In Progress. The other repair records remain Proposed and unapproved | @codex |
| 2026-10-15 | Implementation verified after reapproval: failing-first focused run at baseline source has 10 pass / 17 expected failures, each missing-equality case wrongly returning 0; after the shared actor comparison and capability note, the focused suite passes 27/27 (9.197 seconds) and the routed suite passes 235/235 (26.988 seconds), both with 0 skipped cases. Governance validation and whitespace checks pass. Scope, requirement levels, contract coverage, all five risk dimensions, source content identities and size/nesting guardrails were reviewed; only this record is marked Verified. Disposable test output and owned fixtures are deleted before task completion | @codex |
