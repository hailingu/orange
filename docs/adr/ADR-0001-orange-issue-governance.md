# ADR-0001: Self-contained orange issue governance

## Metadata [Required]

- **Type**: Full ADR
- **Record Identity**: `docs/adr/ADR-0001-orange-issue-governance.md`
- **Author**: `@codex`
- **Decision Owner**: `@hailingu`
- **Required Approver**: Repository owner `@hailingu`; MUST NOT be the author
- **Decision Status**: Proposed
- **Implementation Status**: Not Started
- **Scope**: Repository-wide issue creation, classification, evidence, and closure
- **Primary Implementation Boundary**: Repository process governance
- **Architecture Source**: N/A — process governance, not product-demand implementation
- **Baseline Revision**: `5ec976c64263e0ddd5565a8847d3ac2b818d5fc1`
- **Approver** [Conditionally Required — accepted decision]: N/A — no approval received
- **Approval Time** [Conditionally Required — accepted decision]: N/A — no approval received
- **Approval Evidence** [Conditionally Required — accepted decision]: N/A — no exact `Approve` received
- **Rejector / Rejection Time / Rejection Evidence / Rejection Reason** [Conditionally Required — rejected decision]: N/A — not rejected
- **Retired By / Retirement Time / Retirement Evidence / Retirement Reason** [Conditionally Required — retired decision]: N/A — not retired
- **Supersedes / Superseded By**: None — first issue-governance proposal
- **Blocker / Prior Implementation Status / Blocker Owner / Exit Criterion** [Conditionally Required — Implementation Status is Blocked]: N/A — proposal has not entered implementation

## Requirement Levels [Required]

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

## Context And Constraints [Required]

[PR #4](https://github.com/hailingu/orange/pull/4) added mandatory issue
governance without an Accepted Full ADR. Its copied standard named the
`hailingu/PlotWeave` tracker and relied on an absent Review And Response Policy,
severity policy, out-of-scope threat model, and review-round budget.

AGENTS.md Decision Records and Approval and Status remain authoritative.
This record is a proposal and authorizes no active issue policy. The issue
route and standard explicitly remain inactive until this ADR is Accepted.
Publishing a proposal, a passing PR review, and an instruction to push are
not the exact approval evidence required by that contract.

No ADR or ADR index existed at the baseline revision; there is no non-terminal
record competing with this proposal under repository-wide serialization.

### Preparation Authorization And Missing Prerequisites

After being informed that the Full ADR template, index, and validator were
absent, the repository owner instructed: “直接修复后在本次PR里推送”
(directly repair and push in this PR). This authorizes preparing and publishing
the corrected proposal and its index on the existing PR branch. It is not
recorded as `Approve` and does not accept this ADR.

The prescribed `docs/adr/template/0000-template.md` and `docs/README.md` are
absent. This first proposal follows the existing AGENTS.md requirement-level,
scope, metadata, subtask, acceptance-check, and index contracts directly.
It adds no substitute template, validator, dependency, or general workflow
exception. The configured governance checks cannot run successfully until
their package exists; their failures are retained below rather than reported
as passing validation.

## Complete Task And Scope [Required]

The complete outcome is one authoritative, self-contained issue standard for
`hailingu/orange`, routed from AGENTS.md only after acceptance. A reader can
classify an issue, record a known boundary, and close an issue using local
requirements without consulting undefined policies or another tracker.

Included paths and stable anchors:

- `AGENTS.md` — Issue Tracking; first legal comment position for provenance.
- `docs/development/issue-standard.md` — applicability, status, Required Reading,
  Title Format, Severity, Labels, Known Boundary Content, and Closing.
- `docs/adr/INDEX.md` — this record's identity and lifecycle row.

Excluded: source and configuration, ADD-0001, GitHub label creation, issue
creation or closure, review-round limits, threat-model changes, and restoration
of the missing governance tooling. An issue still cannot authorize source,
configuration, builds, or operations requiring an Accepted ADR or OCR.

## Decision Drivers And Options [Required]

Drivers are local applicability, deterministic disposition rules, real approval
evidence, preservation of existing implementation gates, and the smallest
reviewable repair to PR #4.

| Option | Consequence | Proposed disposition |
| --- | --- | --- |
| Retain a local standard, keep it inactive until this Full ADR is Accepted | Preserves the proposed issue workflow and existing approval contract | Selected proposal |
| Remove the new route and standard entirely | Removes PR #4's issue-governance deliverable | Not selected |
| Activate the standard without an Accepted record | Violates the pre-existing governance gate | Rejected option |

These option dispositions are design choices within a Proposed record; they
are not Decision Status acceptance or rejection events.

## Proposed Decision [Required]

1. Route issues to `hailingu/orange`; use orange scope examples and a local
   defect template instead of another repository's example as a policy input.
2. Define P1–P3 severity within the issue standard. Remove dependencies on the
   absent threat model and review-round budget. Restrict Conventional-Commit
   type-word prohibition to the leading title tag, allowing `docs` as a scope.
   Pure new-capability requests use P3 as a planning category with an unmet
   user need, expected benefit, and observable acceptance outcomes; they do not
   require a defect trigger or reproduction. Requests remedying an existing
   defect retain its severity and the defect-report structure.
3. A known boundary records the trigger, existing controls, residual impact,
   owner identity, explicit owner decision, rationale, supporting evidence,
   and an observable reopening criterion.
4. Closure with code references the resolving commit and verification evidence.
   Closure without code records a named disposition, rationale, evidence, and
   reopening criterion; duplicates link the issue tracking the same problem,
   and retained boundaries include the required owner decision.
5. Keep both the AGENTS route and the standard inactive while this record is
   Proposed. Acceptance uses the existing canonical approval contract, followed
   by consistent status and index maintenance before activation.

## Subtasks [Required]

| ID | Objective / deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| ST-01 | Activate one approved, self-contained orange issue standard | AGENTS Issue Tracking and the local standard, with governing-record provenance | Not Started | None — only the inactive proposal has been published; no issue policy is activated |

Approval, index maintenance, and verification are lifecycle gates, not extra
implementation deliverables.

## Acceptance Checks [Required]

All checks own ST-01. Results must be recorded again after acceptance before
Implementation Status becomes Complete or Verified.

| ID | Preconditions / input | Deterministic verification method | Exact expected result | Evidence to capture | Result |
| --- | --- | --- | --- | --- | --- |
| AC-01 | Eligible approver responds in the context of this exact record | Inspect approval context, metadata, and index row | Decision Status Accepted; Approval Evidence exactly Approve; concrete approver identity distinct from author; ISO 8601 approval time with offset; matching index statuses | Approval context, record and index blobs | Not Run — no approval received |
| AC-02 | Final issue route and standard | Resolve local links and inspect activation clauses and first legal comments | Route and standard cite this exact record; no obligations apply while Proposed; authoritative route requires Accepted | Commit and governed-file blobs | Not Run — inactive draft reviewed only |
| AC-03 | Orange issue titled P2(graph): a stated defect, with P2 label | Apply applicability, Title Format, Severity, and Labels | Tracker hailingu/orange; one area scope; exactly one P2 severity label matching title; trigger or reproduction evidence required locally | Clause-based review report | Not Run — proposal not active |
| AC-04 | Owner decides to retain a known behavior | Apply Labels and Known Boundary Content | known-boundary label and all eight elements: trigger, controls, residual impact, owner identity, owner decision, rationale, evidence, reopening criterion | Clause-based review report | Not Run — proposal not active |
| AC-05 | One code fix, one duplicate, and one retained boundary | Apply Closing to all three cases | Code fix cites commit and verification; duplicate cites disposition, rationale, evidence, reopening criterion, and duplicate issue; retained boundary also cites owner decision | Clause-based review report | Not Run — proposal not active |
| AC-06 | Configured governance validator is available | Run npm test --prefix tools/governance-validator and npm run validate --prefix tools/governance-validator from repository root | Both exit 0; all configured document checks pass | Commands, exit codes, diagnostics | Not Run — validator package absent; attempted commands fail with exit 254 |
| AC-07 | Titles P3(docs): a documentation gap and docs(graph): a documentation gap | Apply Title Format to both titles | First title permitted with P3 label; second rejected because its leading tag is a commit type; type words in scope or summary do not violate the prefix prohibition | Clause-based review report | Not Run — proposal not active |
| AC-08 | A new matrix capability without an existing failure; a feature request remedying an existing P2 defect | Apply Severity and Body Structure to each request | New capability uses P3 title and label plus Goal, Draft Approach, and observable Acceptance Criteria without reproduction; defect remedy uses P2 and the defect structure with trigger or reproduction evidence | Clause-based review report | Not Run — proposal not active |

## Source And Configuration Contracts [Conditionally Required — source or configuration changes]

N/A — this proposal changes documentation only. Source touchpoints,
Contract-To-Check Traceability, and the five-dimension runtime Risk Coverage
Matrix are not triggered. The document acceptance checks above still apply.

## Consequences, Activation, And Rollback [Required]

Before acceptance, the corrected standard can be reviewed without introducing
mandatory issue obligations. After acceptance, its local evidence and
disposition rules govern orange issues. Existing GitHub issues and labels
are not changed by this proposal.

Acceptance must populate actual approval metadata and update the index in the
same change; the standard's displayed status must also agree. Activation and
final checks belong to ST-01. No approval or implementation status is inferred
from this repair's publication.

If the proposal is rejected, use the canonical rejection and archival ritual.
For any later authorized rollback of active governance, remove the issue
route and standard obligations, retain decision history, and record a truthful
terminal status with evidence. Do not label partial active policy Not Applicable.

## Open Questions [Required]

- Before acceptance, restore the prescribed Full ADR template and documentation
  catalog, or obtain an explicit owner determination covering their absence.
- Before completing AC-06, supply the configured governance validator or obtain
  an explicit owner-approved change to that verification requirement. Structured
  review below is supplementary and does not turn ENOENT into passing validation.

No uncertainty blocks publication of the inactive proposal. These prerequisites
remain visible and prevent a false claim of accepted or verified governance.

## Preparation And Verification Evidence [Required]

- Checkout and remote match `hailingu/orange`, PR #4 head branch
  `docs/add-0001-orange-v1-baseline`, at baseline `5ec976c64263e0ddd5565a8847d3ac2b818d5fc1`.
  The PR targets dev. No protected branch was modified or synchronized.
- `git ls-tree -r --name-only HEAD docs/adr` at the baseline returned no records.
  This is the first ADR and its single index row; no competing ADR was found.
- Both configured npm checks were attempted from repository root and exited 254
  because `tools/governance-validator/package.json` does not exist. They have
  not passed. No test suite or build artifact is claimed.
- Structured review: activation requires Accepted in both entry points; tracker
  applicability agrees; severity is local; known-boundary and non-code closure
  have explicit evidence; referenced AGENTS sections exist; index metadata and
  local links agree; conditional fields have specific N/A reasons; no approval
  metadata is fabricated; no source or configuration changes are included.
- `git diff --check` passed for the initial preparation changes. Historical
  preparation blobs from repair commit `9496b5f0c82ae6516ed24314c5983b55efec195d`:
  `AGENTS.md` at `53676b2d74f84df6c5316f2ce6aaf8bbbf950bba`,
  `docs/development/issue-standard.md` at
  `5bd97a193a039f3868520be15905930ccba322f2`, and `docs/adr/INDEX.md` at
  `9a168874a920ad33604198162990d50a7ebf9c9d`. These are preparation evidence,
  not accepted implementation evidence.
- Follow-up preparation from baseline `9496b5f0c82ae6516ed24314c5983b55efec195d`:
  type-word exclusion is confined to the title prefix; `docs` remains a valid
  scope; pure feature requests have a P3 planning category and require goals,
  benefits, and observable outcomes instead of defect reproduction; defect
  remedies retain their defect classification. Reviewed standard blob:
  `d1f0dcafbc798de38620beed92a5a23fc6d16218`. `git diff --check` passes; both
  configured npm checks were attempted again and exit 254 with the same missing
  validator package.json. No approval status changes.

## Change Log [Required]

| Date | Actor | Change |
| --- | --- | --- |
| 2026-10-13 | @codex | Prepare the first Proposed Full ADR and index for PR #4; localize the issue standard and remove unapproved activation under the owner's direct repair-and-push instruction |
| 2026-10-13 | @codex | Resolve follow-up title-scope and feature-classification findings in the same Proposed ADR; add AC-07 and AC-08 without activating governance |
