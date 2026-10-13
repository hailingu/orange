# ADR-0001: Self-contained orange issue governance

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Blocked
- **Date**: 2026-10-13
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Repository-wide issue creation, classification, evidence, and closure
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-13T08:07:03Z
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Approve
- **Approval Context Revision [Optional — informational and non-binding]**: b919326794047e9f91be404a929ff61bb3f4a35a — immutable revision whose record content equaled the approved content; the later template-structure alignment is approval-preserving maintenance under the owner determination recorded in docs/adr/ADR-0002-governance-validator-adoption.md
- **Rejector [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Time [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Evidence [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Reason [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Retired By [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Time [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Evidence [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Reason [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Blocked From [Conditionally Required — Implementation Status is `Blocked`]**: Not Started
- **Blocker And Evidence [Conditionally Required — Implementation Status is `Blocked`]**: AC-06 requires the configured governance validator at `tools/governance-validator` whose `npm test` and `npm run validate` both exit 0. The package did not exist when this record entered Blocked; every recorded attempt exited 254 with ENOENT (see Supporting Notes). Creating it is source work outside this record's accepted scope — the Scope section explicitly excludes restoring the missing governance tooling — and requires a Full ADR, which the repository-wide serialization gate blocks while this record is non-terminal. Blocker-resolution ADR: docs/adr/ADR-0002-governance-validator-adoption.md
- **Blocker Owner [Conditionally Required — Implementation Status is `Blocked`]**: @hailingu
- **Blocker Exit Or Recheck Criterion [Conditionally Required — Implementation Status is `Blocked`]**: Re-run `npm test --prefix tools/governance-validator` and `npm run validate --prefix tools/governance-validator` from repository root after docs/adr/ADR-0002-governance-validator-adoption.md reaches an accepted, implemented state; both exit 0 with no reported document errors. On satisfaction, Implementation Status returns to the recorded prior status `Not Started` and T-1 proceeds to record AC-02 through AC-12.
- **Related [Optional]**: docs/adr/ADR-0002-governance-validator-adoption.md — sole blocker-resolution record under the AGENTS.md serialization exception
- **Architecture Source [Conditionally Required — product demand]**: N/A — process governance, not product-demand implementation
- **Supersedes [Conditionally Required — this ADR replaces another]**: None
- **Superseded By [Conditionally Required — this ADR is replaced]**: None — first issue-governance proposal

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

[PR #4](https://github.com/hailingu/orange/pull/4) added mandatory issue
governance without an Accepted Full ADR. Its copied standard named the
`hailingu/PlotWeave` tracker and relied on an absent Review And Response
Policy, severity policy, out-of-scope threat model, and review-round budget.

AGENTS.md Decision Records and Approval and Status remain authoritative. This
record was a proposal authorizing no active issue policy while Proposed, and
its route and standard explicitly remained inactive until this record was
Accepted. Publishing a proposal, a passing PR review, and an instruction to
push are not the exact approval evidence required by that contract. After being
informed that the Full ADR template, index, and validator were absent, the
repository owner instructed “直接修复后在本次PR里推送” (directly repair and push
in this PR); that authorized preparing and publishing the corrected proposal
and its index on the existing PR branch and was not recorded as `Approve`.

No ADR or ADR index existed at the original baseline revision
`5ec976c64263e0ddd5565a8847d3ac2b818d5fc1`; no non-terminal record competed
with this proposal under repository-wide serialization. The prescribed
templates were absent at drafting time; the repository owner restored the
template family before acceptance, and this record has since been aligned to
the restored template structure as approval-preserving maintenance under the
owner determination recorded in
docs/adr/ADR-0002-governance-validator-adoption.md. The configured governance
checks could not run until their package existed; those failures are retained
in Supporting Notes rather than reported as passing validation.

## Scope [Required]

The complete outcome is one authoritative, self-contained issue standard for
`hailingu/orange`, routed from AGENTS.md only after acceptance. A reader can
classify an issue, record a known boundary, and close an issue using local
requirements without consulting undefined policies or another tracker.

In scope:

- `AGENTS.md` — Issue Tracking; first legal comment position for provenance.
- `docs/development/issue-standard.md` — applicability, status, Required
  Reading, Title Format, Severity, Labels, Known Boundary Content, and
  Closing.
- `docs/adr/INDEX.md` — this record's identity and lifecycle row.
- `docs/adr/fixtures/orange-issue-governance-cases.md` — fixed constructed
  issue bodies, closure comments, and positive/negative cases for review.

Out of scope:

- Source and configuration changes of any kind.
- Changes to ADD-0001 or the architecture baseline.
- Live GitHub label creation or application, and issue creation or closure.
- Review-round limits, threat-model changes, and restoration of the missing
  governance tooling (adopted separately by
  docs/adr/ADR-0002-governance-validator-adoption.md).
- Waiving any mandatory merge or completion gate: an issue still cannot
  authorize source, configuration, builds, or operations requiring an
  Accepted ADR or OCR.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Self-contained local rules versus relying on external policies and trackers | Undefined referenced policies make issue dispositions undecidable and bind orange to another repository's example | Define severity, labels, bodies, and closure locally with orange scope examples and a local defect template |
| TN-2 | Activating the issue route immediately versus preserving the approval gate | Activation without an Accepted record violates the pre-existing governance gate; staying inactive leaves PR #4's deliverable inert | Keep the route and standard inactive while Proposed; activate only through canonical acceptance |
| TN-3 | Reporter convenience versus deterministic triage state | Missing labels at creation either block issue intake or leave permanently unlabeled records | Allow unlabeled creation; maintainers own labeling and missing-label provisioning before triage completion |

### Constraints [Required]

- AGENTS.md Decision Records and Approval and Status are authoritative for
  this record's lifecycle.
- The issue route and standard impose no obligations unless this record is
  `Accepted`.
- An issue records work; it does not authorize implementation or operation
  that requires an Accepted ADR or OCR.
- Severity, labels, and dispositions cannot waive mandatory AGENTS.md merge or
  completion gates, including intent-bearing documentation before a newly
  added public symbol's implementation body.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

| ID | Question | Owner | Due | Status | Resolution and evidence |
| --- | --- | --- | --- | --- | --- |
| Q-1 | Restore the prescribed Full ADR template and documentation catalog before acceptance, or obtain an explicit owner determination covering their absence | @hailingu | 2026-10-13 | Resolved | Owner restored the template family and instructed that `docs/README.md` be created; both were satisfied in the acceptance change of 2026-10-13 |
| Q-2 | Supply the configured governance validator before completing AC-06, or obtain an explicit owner-approved change to that verification requirement | @hailingu | Before AC-06 completion | Open | The owner adopted the validator through docs/adr/ADR-0002-governance-validator-adoption.md (Accepted, In Progress); AC-06 records the two commands' current failing state and completes only when both exit 0. Structured review is supplementary and does not turn ENOENT into passing validation |

## Decision Drivers [Required]

1. **Local applicability**: every disposition rule must be decidable from repository-local requirements.
2. **Deterministic disposition**: classification, retention, and closure have binary outcomes with cited clauses.
3. **Real approval evidence**: activation requires the exact canonical `Approve`, not publication or review passage.
4. **Preserved implementation gates**: issue workflow must not weaken any ADR, OCR, or documentation gate.
5. **Smallest reviewable repair**: the change stays the smallest coherent repair to PR #4.

## Options Considered [Required]

### Option: Retain a local standard, inactive until this Full ADR is Accepted

Keep the proposed issue workflow and the existing approval contract; the route
activates only through canonical acceptance.

Pros:

- Preserves the AGENTS.md approval gate and the full issue workflow.
- Keeps every disposition rule decidable locally.

Cons:

- Leaves the deliverable inert until approval is given.

### Option: Remove the new route and standard entirely

Delete PR #4's issue-governance deliverable.

Pros:

- No inactive policy is retained.

Cons:

- Removes the intended issue-governance capability entirely.

### Option: Activate the standard without an Accepted record

Apply the issue policy immediately on publication.

Pros:

- Immediate effect.

Cons:

- Violates the pre-existing governance gate and fabricates authority.

## Decision [Required]

**Selected option**: Retain a local standard, inactive until this Full ADR is
Accepted.

**Rationale**: It is the only option that delivers the complete outcome — a
self-contained, locally decidable issue standard — without breaking the
canonical approval contract or weakening any implementation gate. The record
was accepted by the repository owner on 2026-10-13 through the exact
`Approve` response, so the route and standard are now active; the remaining
lifecycle work is evidential, not structural.

### Consequences [Required]

Positive:

- Issue classification, retention, and closure in `hailingu/orange` are
  governed by one local standard with cited clauses and fixed fixtures.
- The approval contract survived intact: activation came only from the exact
  `Approve`, establishing the pattern for later records.

Negative:

- The standard's obligations now constrain all issue work, including the
  owner's, with no exception path short of governed revision.

Mitigations:

- Severity and label rules are deliberately local and small; drift is
  corrected through revision of this record rather than external policy.
- Existing GitHub issues and labels are unchanged by this decision.

## Implementation Plan [Required]

**Complete task outcome**: One authoritative, self-contained orange issue
standard, active under this Accepted record, with every acceptance check
recorded as `Pass` or a reasoned `N/A` after acceptance.

**Primary implementation boundary**: Repository process governance.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Activate one approved, self-contained orange issue standard | AGENTS Issue Tracking and the local standard, with governing-record provenance; re-record AC-02 through AC-12 after acceptance | Blocked | Route and standard activated at acceptance on 2026-10-13 (AGENTS.md conditional route; standard Status: Active; AC-01 Pass). AC-02 through AC-12 re-recording is blocked on the AC-06 validator delivery per this record's blocker fields |

**Affected paths**: `AGENTS.md`, `docs/development/issue-standard.md`,
`docs/adr/INDEX.md`, `docs/adr/fixtures/orange-issue-governance-cases.md`

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

N/A — this record changes documentation only; it has no source or
configuration implementation touchpoint.

**Migration and rollback strategy [Conditionally Required — this replaces or
changes existing behavior]**: N/A — no existing behavior is replaced; the
inactive-to-active transition occurred through canonical acceptance.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — no repository software-engineering standard exists yet and no
engineering rule is exceeded or waived by a documentation-only record.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

N/A — this record changes documentation only. It declares no normative public
or internal runtime contract clause; its governing clauses live in
`docs/development/issue-standard.md` and are exercised by AC-02 through AC-12
against the fixed fixture corpus.

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

N/A — this record changes documentation only; there is no runtime behavior
whose concurrency, deadline, cancellation, resource-bound, or trust-boundary
failure mode could occur. The five baseline dimensions cannot apply to a
documentation deliverable.

## Acceptance Checks [Required]

AC-03 through AC-05 and AC-07 through AC-12 use only the fixed inputs in
[the fixture corpus](fixtures/orange-issue-governance-cases.md), including its
stated synthetic premises and exact variant changes. A check passes only if
every listed case matches its expected Conforming/Nonconforming outcome and
decisive clause. Actual outcomes are recorded by case ID together with the
standard and fixture document blobs; no live issue, real owner decision, or
runtime evidence may be inferred from the constructed inputs.

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | The record is accepted with complete approval metadata and a matching index row | Eligible approver responds in the context of this exact record | Inspect approval context, metadata, and index row | Decision Status `Accepted`; Approval Evidence exactly `Approve`; concrete approver identity distinct from author; ISO 8601 approval time with offset; matching index statuses | Approval context, record and index blobs | Pass | Recorded 2026-10-13: repository owner @hailingu (self-declared, distinct from author @codex) responded with the exact word `Approve` in a context identifying this record path; Decision Status, approval metadata, and the index row were updated together in the acceptance change |
| AC-2 | T-1 | The final route and standard cite this exact record and activate only on acceptance | Final issue route and standard | Resolve local links and inspect activation clauses and first legal comments | Route and standard cite this exact record path; authoritative obligations apply only while the record is `Accepted` | Commit and governed-file blobs | Not Started | Pending — re-record after acceptance |
| AC-3 | T-1 | Defect-issue classification rules accept the conforming fixture and reject each nonconforming one | Fixture cases D-OK, D-LABEL, D-MULTI | Apply applicability, Title Format, Severity, Labels, and defect body rules to the fixed D inputs | Outcomes respectively Conforming, Nonconforming, Nonconforming; label mismatch and multiple classification labels rejected | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-4 | T-1 | Known-boundary rules accept the conforming fixture and reject each nonconforming one | Fixture cases B-OK, B-OWNER, B-LABEL, B-REOPEN | Apply Labels and Known Boundary Content to the fixed B inputs | Outcomes respectively Conforming, Nonconforming, Nonconforming, Nonconforming; missing owner decision, boundary label, or reopening criterion rejected | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-5 | T-1 | Closing rules accept the conforming fixtures and reject each nonconforming one | Fixture cases C-OK, C-EVIDENCE, U-OK, U-LINK, K-OK, K-REOPEN | Apply Closing to the fixed issue bodies, comments, and duplicate target | Outcomes respectively Conforming, Nonconforming, Conforming, Nonconforming, Conforming, Nonconforming; each missing closure element rejected under its cited clause | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-6 | T-1 | Both configured governance commands pass from repository root | Configured governance validator is available at `tools/governance-validator` | Run `npm test --prefix tools/governance-validator` and `npm run validate --prefix tools/governance-validator` from repository root | Both exit 0; all configured document checks pass | Commands, exit codes, diagnostics | Not Started | Blocked on the recorded blocker; every attempt before the blocker resolution exited 254 with ENOENT because the package did not exist (see Supporting Notes) |
| AC-7 | T-1 | Title format rules accept the conforming fixture and reject the nonconforming one | Fixture cases T-OK, T-PREFIX | Apply Title Format and Labels to the exact titles and labels | Outcomes respectively Conforming, Nonconforming; `docs` permitted in scope and summary, rejected as the classification prefix | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-8 | T-1 | Severity and body-structure rules classify feature and defect-remedy fixtures correctly | Fixture cases F-OK, F-PRIORITY, R-OK, R-BODY | Apply Severity and Body Structure to the fixed feature and defect-remedy bodies | Outcomes respectively Conforming, Nonconforming, Conforming, Nonconforming; pure feature assigned P3 without reproduction; defect remedy retains P2 and defect structure | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-9 | T-1 | Non-core defects without workarounds classify as P2, not P3 | Fixture cases O-OK, O-P3 | Apply Severity to the fixed non-core defect with repeated failures and no workaround | Outcomes respectively Conforming, Nonconforming | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-10 | T-1 | Mandatory documentation gates hold for every classification | Fixture cases M-BLOCK, M-ALLOW, M-DOCUMENTED | Apply the issue standard's mandatory-gate clause and AGENTS.md Core Rules to the fixed P3 task and documentation dispositions | Outcomes respectively Conforming, Nonconforming, Conforming; missing required documentation blocks merge and completion despite P3; supplying it before the implementation body satisfies that gate | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-11 | T-1 | P1 retention cannot label or close; P2 retention remains eligible | Fixture cases P1-OPEN, P1-LABEL, P1-CLOSE, P2-RETAIN | Apply Severity, Labels, and Closing eligibility to the fixed P1 data-loss issue, retention action/comment, and P2 boundary | Outcomes respectively Conforming, Nonconforming, Nonconforming, Conforming | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |
| AC-12 | T-1 | Reporter and maintainer label obligations classify correctly at every stage | Fixture cases L-CREATE, L-REJECT, L-TRIAGE-MISSING, L-TRIAGED, L-PROVISION | Apply the fixed permission, triage-stage, label-availability, and action inputs to Labels | Outcomes respectively Conforming, Nonconforming, Nonconforming, Conforming, Conforming; unlabeled external creation allowed; maintainers must apply labels before completing triage, and missing labels wait for write-access provisioning | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Started | Pending — re-record after acceptance |

Allowed final check statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
`Fail` blocks completion. `N/A` is valid only when the check's stated trigger or
precondition demonstrably does not apply.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Complete | Approver @hailingu, Approval Time 2026-10-13T08:07:03Z, Approval Evidence `Approve`, informational context revision `b919326794047e9f91be404a929ff61bb3f4a35a` |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Not Started | Pending — T-1 blocked on AC-6 |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | N/A — Architecture Source is `N/A`; no ADD task candidate applies | N/A |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Not Started | Pending |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Not Started | Pending |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | N/A — documentation-only record; the subsection records its N/A reason | N/A |
| A-7 | Contract and baseline risks covered, when applicable | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | N/A — documentation-only record; both sections record their N/A reasons | N/A |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Not Started | Pending — blocked with AC-6 |

## Supporting Notes [Optional]

Preparation and verification evidence retained from the proposal stage:

- Checkout and remote matched `hailingu/orange`, PR #4 head branch
  `docs/add-0001-orange-v1-baseline`, at baseline
  `5ec976c64263e0ddd5565a8847d3ac2b818d5fc1`. The PR targeted dev. No
  protected branch was modified or synchronized.
- `git ls-tree -r --name-only HEAD docs/adr` at the baseline returned no
  records. This was the first ADR and its single index row; no competing ADR
  was found.
- Both configured npm checks were attempted from repository root and exited
  254 because `tools/governance-validator/package.json` did not exist. They
  had not passed. No test suite or build artifact was claimed.
- Structured review at preparation: activation requires `Accepted` in both
  entry points; tracker applicability agrees; severity is local;
  known-boundary and non-code closure have explicit evidence; referenced
  AGENTS sections exist; index metadata and local links agree; conditional
  fields have specific N/A reasons; no approval metadata is fabricated; no
  source or configuration changes are included.
- Preparation fixture inspections by `@codex` across successive baselines
  (`9496b5f0…`, `4cae5d92…`, `8f8679e5…`, `6df2afdb…`, `8de32ca9…`,
  `7528b622…`) applied the standard's clauses to all 33 fixed fixture cases;
  actual outcomes matched the expected clause outcomes in every round. These
  were preparation inspections, not acceptance or automated-test evidence.
- Historical commit-trailer reviews confirmed one nonempty ADR and one OCR
  trailer on each named historical PR commit, and repaired the PR
  description's footer format so a constructed squash candidate parses both
  trailers. The reported squash object could not be resolved locally or
  through GitHub and was never claimed amended.

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Archive this record in the same change that retires it when either Decision
Status is `Rejected` with Implementation Status `Not Applicable`, or Decision
Status is `Deprecated` or `Superseded` with Implementation Status `Verified`,
`Complete`, or `Not Applicable`. Before that trigger, retain this section as
inactive future-lifecycle guidance; its checklist does not affect acceptance or
implementation completion. When triggered:

- [ ] Move this file to `archive/ADR-0001-orange-issue-governance.md` under this same ADR
      root (project `docs/adr/archive/` or `<service>/docs/adr/archive/`).
- [ ] Update every code marker that cites this file's pre-archive path to the new
      archive path, or remove the marker if the governed code was deleted.
- [ ] If Decision Status is `Superseded`, set the replacement record's
      `Supersedes` field and this record's `Superseded By` field to each other's
      final repository-relative path.
- [ ] If no record supersedes this one, retain `Superseded By: None — first issue-governance proposal`.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with the archived
      path, scope, and final status.
- [ ] Confirm no ADR or OCR outside an `archive/` directory, and no code marker, still cites the pre-archive path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-13 | Prepare the first Proposed Full ADR and index for PR #4; localize the issue standard and remove unapproved activation under the owner's direct repair-and-push instruction | @codex |
| 2026-10-13 | Resolve follow-up title-scope and feature-classification findings in the same Proposed ADR; add the title and feature checks without activating governance | @codex |
| 2026-10-13 | Replace generic disposition-check inputs with fixed constructed issue/closure fixtures and explicit positive/negative outcomes; use the same corpus for title and feature checks | @codex |
| 2026-10-13 | Cover reproducible non-core defects without workarounds in P2; preserve mandatory merge/completion documentation gates regardless of classification; add fixed non-core and mandatory-gate inputs without activating governance | @codex |
| 2026-10-13 | Restrict known-boundary labeling and retained-boundary closure to P2/P3; preserve P1 resolution and evidence-backed triage gates; add fixed P1-retention inputs without activating governance | @codex |
| 2026-10-13 | Move label obligations from issue creation to maintainer triage after creation; add fixed permission/stage cases and retain existing P1 restrictions without activating governance | @codex |
| 2026-10-13 | Scope trailer evidence to named historical revisions; reproduce missing trailers in a PR-title/body squash candidate and repair its footer format without claiming to amend an unavailable commit | @codex |
| 2026-10-13 | Record acceptance per repository owner @hailingu's exact `Approve`: Decision Status Proposed→Accepted with approval metadata and an informational approval context revision; resolve the pre-acceptance open question by versioning the owner-restored decision-record and ADD templates and creating `docs/README.md`; update the index row and the standard's displayed status; mark the approval check Pass. Implementation Status remained Not Started | @zcode |
| 2026-10-13 | Enter Blocked from Not Started: AC-6's governance validator did not exist and its creation is source work outside this record's accepted scope, blocked by repository-wide ADR serialization. Record blocker fields and cite the blocker-resolution ADR under the AGENTS.md serialization exception; update the index row in the same change | @zcode |
| 2026-10-13 | Align this record to the restored Full ADR template structure as approval-preserving maintenance under the owner determination recorded in ADR-0002: template section set, metadata field syntax, and acceptance-check columns change form only; the complete task outcome, scope, subtask semantics, check semantics, statuses (Accepted/Blocked), and approval metadata are unchanged | @zcode |
