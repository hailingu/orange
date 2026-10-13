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
- `docs/adr/fixtures/orange-issue-governance-cases.md` — fixed constructed
  issue bodies, closure comments, and positive/negative cases for review.

Excluded: source and configuration, ADD-0001, live GitHub label creation or
application, issue creation or closure, review-round limits, threat-model
changes, and restoration of the missing governance tooling. An issue still cannot authorize source,
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
   Existing non-P1 functional defects and contract inconsistencies use P2
   regardless of reproducibility or workaround availability. Classification,
   P3 scheduling, and known-boundary dispositions cannot waive mandatory
   merge or completion gates, including documentation before a newly added
   public symbol's implementation body.
3. Reporters supply title and body at creation; a reporter without label
   permissions may open an unlabeled issue awaiting triage. Maintainers with
   triage access own label application and correction before completing triage
   or closing an issue on behalf of the repository. Missing labels await
   provisioning by a maintainer with write access, not action by the reporter.
   This proposal does not create or apply live labels. A known boundary records
   the trigger, existing controls, residual impact,
   owner identity, explicit owner decision, rationale, supporting evidence,
   and an observable reopening criterion. Only P2/P3 issues may receive the
   known-boundary label or be closed as retained boundaries. Proposed retention
   of a P1 issue leaves it open until resolution or evidence-backed non-P1
   reclassification; willingness to retain the impact cannot justify a downgrade.
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

AC-03 through AC-05 and AC-07 through AC-12 use only the fixed inputs in
[the fixture corpus](fixtures/orange-issue-governance-cases.md), including its
stated synthetic premises and exact variant changes. A check passes only if
every listed case matches its expected Conforming/Nonconforming outcome and
decisive clause. Record actual outcomes by case ID and both document blobs;
do not choose live issues or infer real owner decisions or runtime evidence
from the constructed inputs.

| ID | Preconditions / input | Deterministic verification method | Exact expected result | Evidence to capture | Result |
| --- | --- | --- | --- | --- | --- |
| AC-01 | Eligible approver responds in the context of this exact record | Inspect approval context, metadata, and index row | Decision Status Accepted; Approval Evidence exactly Approve; concrete approver identity distinct from author; ISO 8601 approval time with offset; matching index statuses | Approval context, record and index blobs | Not Run — no approval received |
| AC-02 | Final issue route and standard | Resolve local links and inspect activation clauses and first legal comments | Route and standard cite this exact record; no obligations apply while Proposed; authoritative route requires Accepted | Commit and governed-file blobs | Not Run — inactive draft reviewed only |
| AC-03 | Fixture cases D-OK, D-LABEL, D-MULTI | Apply applicability, Title Format, Severity, Labels, and defect body rules to the fixed D inputs | Outcomes respectively Conforming, Nonconforming, Nonconforming; label mismatch and multiple classification labels rejected | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-04 | Fixture cases B-OK, B-OWNER, B-LABEL, B-REOPEN | Apply Labels and Known Boundary Content to the fixed B inputs | Outcomes respectively Conforming, Nonconforming, Nonconforming, Nonconforming; missing owner decision, boundary label, or reopening criterion rejected | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-05 | Fixture cases C-OK, C-EVIDENCE, U-OK, U-LINK, K-OK, K-REOPEN | Apply Closing to the fixed issue bodies, comments, and duplicate target | Outcomes respectively Conforming, Nonconforming, Conforming, Nonconforming, Conforming, Nonconforming; each missing closure element rejected under its cited clause | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-06 | Configured governance validator is available | Run npm test --prefix tools/governance-validator and npm run validate --prefix tools/governance-validator from repository root | Both exit 0; all configured document checks pass | Commands, exit codes, diagnostics | Not Run — validator package absent; attempted commands fail with exit 254 |
| AC-07 | Fixture cases T-OK, T-PREFIX | Apply Title Format and Labels only to the exact titles and labels | Outcomes respectively Conforming, Nonconforming; docs permitted in scope and summary, rejected as the classification prefix | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-08 | Fixture cases F-OK, F-PRIORITY, R-OK, R-BODY | Apply Severity and Body Structure to the fixed feature and defect-remedy bodies | Outcomes respectively Conforming, Nonconforming, Conforming, Nonconforming; pure feature assigned P3 without reproduction; defect remedy retains P2 and defect structure | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-09 | Fixture cases O-OK, O-P3 | Apply Severity to the fixed non-core defect with repeated failures and no workaround | Outcomes respectively Conforming, Nonconforming; non-P1 functional defect assigned P2, not P3 | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-10 | Fixture cases M-BLOCK, M-ALLOW, M-DOCUMENTED | Apply the issue standard's mandatory-gate clause and AGENTS.md Core Rules to the fixed P3 task and documentation dispositions | Outcomes respectively Conforming, Nonconforming, Conforming; missing required documentation blocks merge and completion despite P3; supplying it before the implementation body satisfies that gate | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-11 | Fixture cases P1-OPEN, P1-LABEL, P1-CLOSE, P2-RETAIN | Apply Severity, Labels, and Closing eligibility to the fixed P1 data-loss issue, retention action/comment, and P2 boundary | Outcomes respectively Conforming, Nonconforming, Nonconforming, Conforming; P1 retention cannot add the label or close the issue despite an owner decision; P2 retention remains eligible | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |
| AC-12 | Fixture cases L-CREATE, L-REJECT, L-TRIAGE-MISSING, L-TRIAGED, L-PROVISION | Apply the fixed permission, triage-stage, label-availability, and action inputs to Labels | Outcomes respectively Conforming, Nonconforming, Nonconforming, Conforming, Conforming; unlabeled external creation allowed; maintainers must apply labels before completing triage, and missing labels wait for write-access provisioning | Case IDs, actual outcomes, clauses, standard and fixture blobs | Not Run — proposal not active |

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
- Fixture preparation review from baseline
  `4cae5d9211998ffddeb4dedbc813fb51e2c48fc6`: inspected the fixed inputs against
  standard blob `d1f0dcafbc798de38620beed92a5a23fc6d16218` and fixture blob
  `b81fc943e5dc81dcaa0e9af72fef518f6a7f3fd6`. Actual outcomes are recorded below;
  all agree with their expected clause outcomes. This is preparation inspection
  by `@codex`, not acceptance, runtime verification, or passing automated tests.
  Both configured npm commands were attempted again and exited 254 with the
  missing validator package.json. `git diff --check` passed.

| Preparation cases, in order | Actual outcomes, in order |
| --- | --- |
| D-OK, D-LABEL, D-MULTI | Conforming, Nonconforming, Nonconforming |
| B-OK, B-OWNER, B-LABEL, B-REOPEN | Conforming, Nonconforming, Nonconforming, Nonconforming |
| C-OK, C-EVIDENCE, U-OK, U-LINK, K-OK, K-REOPEN | Conforming, Nonconforming, Conforming, Nonconforming, Conforming, Nonconforming |
| T-OK, T-PREFIX | Conforming, Nonconforming |
| F-OK, F-PRIORITY, R-OK, R-BODY | Conforming, Nonconforming, Conforming, Nonconforming |

Follow-up preparation review by `@codex` from baseline
`8f8679e5aca12afa1aadc4e1494b667b802afbdf`: inspected all 24 fixed cases
against standard blob `4672aa5ca2bed1de8f45eff2cb2b49fda328db3f` and fixture
blob `96b269909be9e86da8b572094ae358a082e5fb29`. The 19 existing cases retain
the actual outcomes in the preceding table; the five additional outcomes
are below. All match their expected clauses. P2 covers ordinary non-core
failures without workarounds, while mandatory public-symbol documentation
still gates P3 feature implementation. ADR and index remain Proposed / Not
Started. This is preparation inspection, not acceptance or automated test
evidence. `git diff --check` passed. Both configured npm checks were attempted
again from repository root and exited 254 (ENOENT: missing
`tools/governance-validator/package.json`); they have not passed.

| Additional preparation cases, in order | Actual outcomes, in order |
| --- | --- |
| O-OK, O-P3 | Conforming, Nonconforming |
| M-BLOCK, M-ALLOW, M-DOCUMENTED | Conforming, Nonconforming, Conforming |

P1-retention preparation review by `@codex` from baseline
`6df2afdbbc5d489ad077a269bcbdda3737e0f3ec`: inspected all 28 fixed cases
against standard blob `66ac8714a5b8abd3df68ba2e3abc79e99eddbb93` and fixture
blob `4c8bc774a5159330b1042d3a09396d9fe4f49f4e`. The 24 existing cases retain
their previously recorded outcomes; the four additional outcomes are below.
All match the expected clauses. P1 retention cannot add the label, close the
issue, waive the fix-before-merge gate, or justify a severity downgrade; P2
retention remains eligible. ADR and index remain Proposed / Not Started.
This is preparation inspection, not acceptance or automated test evidence.
`git diff --check` passed. Both configured npm checks were attempted again
from repository root and exited 254 (ENOENT: missing
`tools/governance-validator/package.json`); they have not passed.

| P1-retention preparation cases, in order | Actual outcomes, in order |
| --- | --- |
| P1-OPEN, P1-LABEL, P1-CLOSE, P2-RETAIN | Conforming, Nonconforming, Nonconforming, Conforming |

External-reporter preparation review by `@codex` from baseline
`8de32ca9e89225d7dda89dea0f82c86dd7035ef2`: inspected all 33 fixed cases
against standard blob `27d68f32af54195be20e42c9e4c2d3c940ef2985` and fixture
blob `bf045196a395785f51f2d4ad6a2913760886ab21`. The 28 existing cases retain
their recorded outcomes at the now-explicit completed-triage stage; the five
additional outcomes are below. All match the expected clauses. External
reporters can open unlabeled issues; maintainers own labeling and missing-label
provisioning before triage completion or repository closure. The P1 retention
prohibition applies at every stage. ADR and index remain Proposed / Not Started.
This is preparation inspection, not acceptance or automated test evidence.
`git diff --check` passed. Both configured npm checks were attempted again
from repository root and exited 254 (ENOENT: missing
`tools/governance-validator/package.json`); they have not passed.

| External-reporter preparation cases, in order | Actual outcomes, in order |
| --- | --- |
| L-CREATE, L-REJECT, L-TRIAGE-MISSING, L-TRIAGED, L-PROVISION | Conforming, Nonconforming, Nonconforming, Conforming, Conforming |

Historical commit-trailer review at baseline
`8de32ca9e89225d7dda89dea0f82c86dd7035ef2`: the GitHub PR commit collection
then contained `5ec976c`, `9496b5f`, `4cae5d9`, `8f8679e`, `6df2afd`, and
`8de32ca`. For each exact commit, its `git show -s --format=%B` output was
passed to `git interpret-trailers --parse`; all six yield one nonempty ADR
and one nonempty OCR trailer. The reviewed `12c6e2f` was not in that collection
and could not be resolved locally or through GitHub at that inspection.
These results cover only the named immutable commits, not the current PR
commit set or any later squash or rebase message.

Squash-message preparation review by `@codex` from baseline
`7528b622c580d37134a1e72623544bf6aeab7ce1`: `git ls-remote` independently
confirmed this live PR branch head, and the GitHub PR commit collection had
the six historical commits above plus `7528b62`. Each actual message was
passed to `git interpret-trailers --parse`; all seven contain one ADR and one
OCR trailer. The reported squash `d9f1da89f34e4308e73c35048f594ee65fe3dd5a`
could not be resolved by local `git cat-file` or GitHub's commit lookup.
This inspection does not claim to have amended that unavailable object.

The PR description's ADR/OCR references were still prose bullets. A candidate
message constructed from the PR title, a blank line, and that description
produced no output from `git interpret-trailers --parse`. The repaired
candidate description ends with separate, unbulleted footer lines:

```text
ADR: docs/adr/ADR-0001-orange-issue-governance.md
OCR: N/A — documentation proposal repair; no governed build or operation
```

Parsing the repaired candidate returns exactly those two trailers. This
checks the prepared message, not an already-created squash commit; the exact
final commit message must still be checked after any squash or rebase under
AGENTS.md's existing commit policy. ADR and index remain Proposed / Not Started.

For this evidence repair, `git diff --check` passed. Both configured npm
commands were attempted again from repository root and exited 254 (ENOENT:
missing `tools/governance-validator/package.json`). Structured review confirms
that historical and candidate-message results are distinguished, approval and
index metadata are unchanged, and no issue-policy or fixture behavior changes.
These checks do not claim passing governance validation or record acceptance.

## Change Log [Required]

| Date | Actor | Change |
| --- | --- | --- |
| 2026-10-13 | @codex | Prepare the first Proposed Full ADR and index for PR #4; localize the issue standard and remove unapproved activation under the owner's direct repair-and-push instruction |
| 2026-10-13 | @codex | Resolve follow-up title-scope and feature-classification findings in the same Proposed ADR; add AC-07 and AC-08 without activating governance |
| 2026-10-13 | @codex | Replace generic disposition-check inputs with fixed constructed issue/closure fixtures and explicit positive/negative outcomes; use the same corpus for title and feature checks |
| 2026-10-13 | @codex | Cover reproducible non-core defects without workarounds in P2; preserve mandatory merge/completion documentation gates regardless of classification; add fixed AC-09 and AC-10 inputs without activating governance |
| 2026-10-13 | @codex | Restrict known-boundary labeling and retained-boundary closure to P2/P3; preserve P1 resolution and evidence-backed triage gates; add fixed AC-11 inputs without activating governance |
| 2026-10-13 | @codex | Move label obligations from issue creation to maintainer triage after creation; add fixed permission/stage cases and retain existing P1 restrictions without activating governance |
| 2026-10-13 | @codex | Scope trailer evidence to named historical revisions; reproduce missing trailers in a PR-title/body squash candidate and repair its footer format without claiming to amend an unavailable commit |
