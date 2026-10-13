# ADR-0002: Adopt and localize the governance validator

## Metadata [Required]

- **Decision Status**: Proposed
- **Implementation Status**: Not Started
- **Date**: 2026-10-13
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — replace with the concrete `@<actor-id>`
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — replace with an ISO 8601 date-time containing `Z` or an explicit `±HH:MM` offset
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — replace with exactly `Approve`
- **Approval Context Revision [Optional — informational and non-binding]**: remove — no immutable revision contains the approved content yet
- **Rejector [Conditionally Required — Decision Status is `Rejected`]**: Pending — replace with the concrete `@<actor-id>`
- **Rejection Time [Conditionally Required — Decision Status is `Rejected`]**: Pending — replace with an ISO 8601 date-time containing `Z` or an explicit `±HH:MM` offset
- **Rejection Evidence [Conditionally Required — Decision Status is `Rejected`]**: Pending — replace with exactly `Reject`
- **Rejection Reason [Conditionally Required — Decision Status is `Rejected`]**: Pending — replace with a concrete explanation of why the proposal was rejected, including the deciding constraint or evidence when applicable; outside `Rejected`, retain a reasoned `N/A — <reason>` value
- **Retired By [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with the concrete `@<actor-id>`
- **Retirement Time [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with an ISO 8601 date-time containing `Z` or an explicit `±HH:MM` offset
- **Retirement Evidence [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with exactly `Deprecate` or `Supersede`
- **Retirement Reason [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending
- **Blocked From [Conditionally Required — Implementation Status is `Blocked`]**: Pending — `Not Started` or `In Progress`
- **Blocker And Evidence [Conditionally Required — Implementation Status is `Blocked`]**: Pending
- **Blocker Owner [Conditionally Required — Implementation Status is `Blocked`]**: Pending
- **Blocker Exit Or Recheck Criterion [Conditionally Required — Implementation Status is `Blocked`]**: Pending
- **Related [Optional]**: Blocked record docs/adr/ADR-0001-orange-issue-governance.md (Accepted, Blocked) — this ADR is its sole blocker-resolution record under the AGENTS.md serialization exception
- **Architecture Source [Conditionally Required — product demand]**: N/A — blocker-resolution governance tooling under the AGENTS.md serialization exception
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

AGENTS.md Scope Routing makes `npm test --prefix tools/governance-validator`
and `npm run validate --prefix tools/governance-validator` the configured
verification commands for every governance-document change, and
docs/adr/ADR-0001-orange-issue-governance.md AC-06 requires both to exit 0
before that record can complete. The package has never existed in this
repository; every recorded attempt exits 254 with ENOENT. ADR-0001 is the sole
non-terminal record, is `Accepted` and now `Blocked`, its Complete Task And
Scope explicitly excludes restoring the missing governance tooling, and an OCR
cannot authorize source creation — so every condition of the AGENTS.md
blocker-resolution exception holds, and this Full ADR exists solely to remove
that named blocker.

On 2026-10-13 the repository owner restored the decision-record and ADD
template family and copied that system's validator into the worktree. The
copied package passes its own 208-test suite and exits 1 on validation errors,
and its status, subtask, check, candidate, and risk-dimension contracts match
AGENTS.md exactly. An assessment run against this repository reported about
sixty errors from two causes: source-repository remnants (package scope,
legacy-record exemptions, a `Trello Source` index-column expectation, and
governance markers citing foreign ADR paths) and orange documents written
before the templates existed (ADR-0001, ADD-0001, and both indexes use
different section names, metadata syntax, and index columns). The owner
decided on 2026-10-13 that demand tracking uses GitHub Projects at
`https://github.com/hailingu/orange/projects`, not Trello.

## Scope [Required]

In scope:

- Adopt the copied validator at `tools/governance-validator/` as orange's
  configured governance validator with its existing behavior and test suite.
- Remove source-repository remnants: rename the package to an orange scope,
  empty the legacy touchpoint exemptions, rename the `Trello Source` index
  column expectation to `Demand Source` with values normalized as GitHub
  issue or Projects links, replace every foreign `// ADR:` marker with this
  record's path, and refresh the package README's point-in-time review note.
- Replace `Trello` wording in the restored decision-record and ADD templates
  with the GitHub Projects demand-tracking vocabulary.
- Align ADR-0001, ADD-0001, docs/adr/INDEX.md, and docs/architecture/INDEX.md
  to the restored template syntax without changing approved semantics.
- Update `.gitignore` for `node_modules/` and `.DS_Store`.
- Record verification evidence for both configured commands from repository
  root.

Out of scope:

- New validation rules beyond the AGENTS.md contracts the copied package
  already enforces, and any behavior change not required by orange vocabulary.
- CI workflow integration, GitHub Projects live configuration, or any remote
  write.
- Authoring `docs/development/software-engineering-standard.md` or any other
  missing catalog standard.
- Any change to AGENTS.md, to ADR-0001's approved outcome, or to product
  source code.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Reuse a proven foreign implementation versus write orange's validator from scratch under test-first discipline | Writing from scratch delays AC-06 and duplicates 208 passing tests; adopting blindly imports foreign vocabulary and markers | Adopt the copied package and localize it with a behavior test committed before each behavioral edit |
| TN-2 | Template authority versus the legacy format of existing accepted records | Leaving documents unaligned keeps both configured commands failing; rewriting records risks touching approved content | Align documents to template syntax under the recorded approval-preserving determination, with the re-approval ritual as the mandatory fallback |
| TN-3 | Foreign `Trello Source` tracking vocabulary versus orange's GitHub Projects demand tracking | A wrong column name makes every ADD index fail validation | Rename the expectation to `Demand Source` and normalize Markdown links to their URLs, matching the existing orange index column |

### Constraints [Required]

- AGENTS.md is authoritative wherever the copied package's conventions differ
  from it; the package adapts, not the guide.
- The validator and its tests are source work: behavior changes are developed
  test-first, and dependencies stay exactly locked.
- The blocker-resolution exception permits exactly these two non-terminal
  records; no unrelated outcome may ride on this ADR.
- ADR-0001 keeps Decision Status `Accepted` and Implementation Status
  `Blocked` throughout this ADR's implementation; its approved semantics must
  not change.
- The validator reads only repository files; it must not gain network,
  credential, or side-effecting behavior.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

| ID | Question | Owner | Due | Status | Resolution and Evidence |
| --- | --- | --- | --- | --- | --- |
| Q-1 | Is ADR-0001's template alignment approval-preserving? | @hailingu | 2026-10-13 | Resolved | Owner determination 2026-10-13: structural alignment that preserves the complete task outcome, scope, subtask semantics, acceptance-check semantics, and approval metadata is approval-preserving formatting maintenance under AGENTS.md; any semantic change halts for the re-approval ritual instead. Evidence: owner approved the plan containing this determination in the 2026-10-13 session, and approval of this ADR carries it |
| Q-2 | May the foreign implementation be copied into orange? | @hailingu | 2026-10-13 | Resolved | The owner copied it from their own repository and instructed adoption and localization on 2026-10-13; the package is private and unmodified in behavior. Evidence: owner instruction in the 2026-10-13 session and the worktree copy at `tools/governance-validator/` |
| Q-3 | Which tracking vocabulary replaces Trello? | @hailingu | 2026-10-13 | Resolved | GitHub Projects at `https://github.com/hailingu/orange/projects` plus repository issues; index values normalize Markdown links to their URLs. Evidence: owner instruction in the 2026-10-13 session |

## Decision Drivers [Required]

1. **Unblock ADR-0001 and serialization**: AC-06 must become satisfiable so the sole non-terminal record can complete and V1 implementation can proceed.
2. **Configured commands must exist and be honest**: the two AGENTS.md routing commands must run real checks and exit 0 only when the repository actually passes.
3. **Reuse proven behavior**: the copied suite already enforces the AGENTS.md lifecycle, index, reciprocity, and Mermaid contracts with 208 passing tests.
4. **Orange vocabulary**: demand tracking is GitHub Projects, and every governed file marker must cite orange record paths.

## Options Considered [Required]

### Option: Adopt and localize the copied validator

Move the package to `tools/governance-validator/`, remove source-repository remnants test-first, rename the demand-source column expectation, replace markers, and align the existing documents to the templates.

Pros:

- Fastest path to both commands exiting 0 with 208 existing tests retained.
- Proven contract coverage that already matches AGENTS.md's status sets.
- Keeps templates and validator in the same coherence they were built with.

Cons:

- Carries foreign provenance that must be audited and rewritten.
- Forces alignment of one accepted record's format.

### Option: Write orange's validator from scratch

Implement a new Node package against AGENTS.md under full test-first development.

Pros:

- No foreign lineage to unwind.

Cons:

- Duplicates a large, already-tested implementation.
- Delays AC-06 and the whole serialization unblock by that effort.

### Option: Adapt the validator to orange's legacy document format

Teach the copied package orange's pre-template section names and metadata syntax instead of aligning documents.

Pros:

- Existing records stay untouched.

Cons:

- Diverges the validator from the restored templates AGENTS.md routes to.
- Freezes a legacy format as the enforced contract.

## Decision [Required]

**Selected option**: Adopt and localize the copied validator.

**Rationale**: It is the only option that satisfies the AC-06 exit criterion
quickly while keeping the restored templates authoritative. The foreign
remediation cost is bounded and enumerated, the 208-test suite anchors the
behavior contract during localization, and document alignment is a one-time
structural conversion already covered by the owner's approval-preserving
determination with a mandatory re-approval fallback.

### Consequences [Required]

Positive:

- Both configured commands run real deterministic checks and can exit 0.
- ADR-0001's blocker becomes removable and repository serialization reopens.
- Governance documents gain a machine-checked template, lifecycle, index,
  reciprocity, and Mermaid contract.

Negative:

- One accepted record and one draft ADD are restructured, with a residual
  risk that a seemingly structural edit changes meaning.
- The package carries inherited code this ADR's review has only
  spot-verified.

Mitigations:

- Q-1's determination mandates the re-approval ritual the moment any semantic
  change appears, and the completion checks assert ADR-0001's approval
  metadata survives alignment.
- Localization runs the inherited suite test-first on every behavior edit,
  and AC-2 blocks completion on any surviving foreign reference.

## Implementation Plan [Required]

**Complete task outcome**: `tools/governance-validator/` exists as orange's
configured governance validator with no source-repository remnants, the
existing governance documents satisfy the restored template syntax, and both
`npm test --prefix tools/governance-validator` and
`npm run validate --prefix tools/governance-validator` exit 0 from repository
root — satisfying ADR-0001 AC-06 and its recorded exit criterion.

**Primary implementation boundary**: Repository governance tooling (validator
source).

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Localized validator source and package identity | `tools/governance-validator/` source, tests, `package.json`, lockfile, README; each behavior edit lands its failing test first; suite stays green | Not Started | Pending |
| T-2 | Template wording and existing-document alignment to template syntax | `docs/adr/template/`, `docs/architecture/template/`, `docs/adr/ADR-0001-orange-issue-governance.md`, `docs/architecture/ADD-0001-orange-v1-baseline.md`, both `INDEX.md` files, `.gitignore`; semantics and statuses unchanged | Not Started | Pending |
| T-3 | Repository-root verification evidence | Run both configured commands from repository root, capture exit codes and output, confirm ADR-0001's exit criterion is demonstrated satisfiable | Not Started | Pending |

**Affected paths**: `tools/governance-validator/**`,
`docs/adr/template/0000-template.md`,
`docs/adr/template/0000-lightweight-template.md`,
`docs/adr/template/0000-operational-change-template.md`,
`docs/architecture/template/0000-add-template.md`,
`docs/adr/ADR-0001-orange-issue-governance.md`,
`docs/architecture/ADD-0001-orange-v1-baseline.md`, `docs/adr/INDEX.md`,
`docs/architecture/INDEX.md`, `.gitignore`

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| `tools/governance-validator/lib/relationship-validation.mjs` | `expectedIndexColumns` | N/A — stable symbol or anchor is sufficient | Declares required index columns; rename the ADD-index `Trello Source` entry to `Demand Source` so GitHub Projects and issue links validate | Pre-commit worktree state 2026-10-13 — must be replaced with the adoption commit before completion |
| `tools/governance-validator/lib/relationship-validation.mjs` | `compareIndexedTrelloSource` | N/A — stable symbol or anchor is sufficient | Normalizes Markdown links to URLs for the demand-source column comparison; rename to the `Demand Source` vocabulary and keep URL normalization | Pre-commit worktree state 2026-10-13 — must be replaced with the adoption commit before completion |
| `tools/governance-validator/lib/accepted-records.mjs` | `LEGACY_TOUCHPOINT_EXEMPTIONS` | N/A — stable symbol or anchor is sufficient | Foreign legacy-record exemption set; must be emptied so no orange record can bypass the touchpoint gate | Pre-commit worktree state 2026-10-13 — must be replaced with the adoption commit before completion |
| `tools/governance-validator/package.json` | `name` | N/A — stable symbol or anchor is sufficient | Package identity; rename from the foreign scope to the orange scope and regenerate the lockfile name entry without touching the exactly-pinned dependency versions | Pre-commit worktree state 2026-10-13 — must be replaced with the adoption commit before completion |
| `tools/governance-validator/**/​*.mjs` | first-legal-comment `// ADR:` markers | N/A — stable symbol or anchor is sufficient | Governed-file provenance; every marker must cite `docs/adr/ADR-0002-governance-validator-adoption.md` | Pre-commit worktree state 2026-10-13 — must be replaced with the adoption commit before completion |

**Migration and rollback strategy [Conditionally Required — this replaces or
changes existing behavior]**: No runtime behavior is replaced — the validator
is newly introduced, so migration is only the document alignment in T-2.
Rollback removes `tools/governance-validator/`, reverts T-2's document edits
and `.gitignore`, and leaves ADR-0001 `Blocked` with its exit criterion
unmet; no other system state depends on this package.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

| Rule and affected unit | Measured value or condition | Rationale | Risks | Compensating controls | Owner | Removal, review, or permanent rationale | Verification evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| N/A — no repository software-engineering standard exists yet; the validator follows its AGENTS.md routing-row rules (test-first development, exactly locked dependencies), and no rule is exceeded or waived | N/A — no standard to measure against | N/A | N/A | N/A | N/A | N/A | N/A |

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| CC-1 | `AGENTS.md` › Scope Routing › `tools/governance-validator/**` row | The validator and its tests are source work: develop behavior test-first and keep dependencies exactly locked | AC-1, AC-2, AC-3 | AC-1 keeps the suite green across every behavior edit; AC-2 and AC-3 block completion on any surviving foreign reference or unpinned dependency |
| CC-2 | `AGENTS.md` › Scope Routing verification columns; `docs/adr/ADR-0001-orange-issue-governance.md` › Acceptance Checks › AC-06 | `npm test --prefix tools/governance-validator` and `npm run validate --prefix tools/governance-validator` run from repository root and both exit 0 with all configured document checks passing | AC-6, AC-7 | Both commands are executed from repository root on the aligned repository and their exit codes captured |
| CC-3 | `AGENTS.md` › Architecture Design Documents › Index | The ADD index row records its demand-source field; orange demand sources are GitHub Projects items and issues, not Trello cards | AC-2, AC-6 | AC-2 removes the `Trello Source` expectation from source and templates; AC-6 proves the orange `Demand Source` column validates |
| CC-4 | `AGENTS.md` › Identity, Storage, Filenames, and Provenance | A governed file cites its current record path at the first legal comment position | AC-2 | AC-2 asserts every `// ADR:` marker in the package cites exactly this record's path |
| CC-5 | `AGENTS.md` › Approval and Status › approval-invalidating changes | An approval-invalidating change to an Accepted record returns it to `Proposed`; formatting maintenance that changes no listed content is approval-preserving | AC-4, AC-5 | AC-4 asserts ADR-0001's approval metadata and `Accepted`/`Blocked` statuses survive alignment; any required semantic change must instead leave AC-4 unsatisfiable until the re-approval ritual runs |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | N/A — single-process, single-threaded batch execution with no shared mutable state; output ordering follows the deterministic repository traversal | Validator source | Inspect execution model; run AC-6 | No concurrency construct exists; repeated runs produce identical output | AC-6 | N/A — single-threaded batch tool with deterministic traversal | Not run — implementation not started |
| timeout and deadline | N/A — the validator is a batch command with no deadlines, no waits, and no partial-output commitment | Validator source | Inspect execution model; run AC-6 | Process runs to completion and exits; no timeout logic exists | AC-6 | N/A — no deadline-bound behavior exists | Not run — implementation not started |
| cancellation and interruption | N/A — validation is read-only; an interrupted run leaves no persisted partial state and reruns from scratch | Validator source | Inspect for writes outside the process; run AC-6 | No repository or external state is mutated by a validation run | AC-6 | N/A — read-only batch execution | Not run — implementation not started |
| resource bounds and backpressure | N/A — inputs are bounded by the repository tree itself with ignored directories excluded; there is no interactive admission or queue to apply backpressure to | Validator source | Inspect input enumeration; run AC-6 | Traversal enumerates repository files only, skipping ignored directories, and completes | AC-6 | N/A — repository-bounded batch input | Not run — implementation not started |
| framework or trust-boundary rejection | N/A — all inputs are repository-versioned files at the developer's own checkout; there is no network, credential, or external-input boundary to reject at | Validator source | Inspect for network or credential use; run AC-6 | No network, credential, or side-effecting API is invoked | AC-6 | N/A — no external trust boundary exists | Not run — implementation not started |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | The validator's own test suite passes after localization | Localized package at `tools/governance-validator/` | `npm test --prefix tools/governance-validator` from repository root | Exit code 0 with at least 208 passing and 0 failing tests | Command output summary with pass/fail counts and exit code | Not Started | Pending |
| AC-2 | T-1 | No source-repository reference survives in governed files | Localized package and restored templates | `grep -rn "koduck\|Trello\|provider-neutral-turn-kernel\|required-ai-ci-postgres" tools/governance-validator docs/adr/template docs/architecture/template --exclude-dir=node_modules --exclude=package-lock.json`; then inspect every `// ADR:` marker line in `tools/governance-validator` outside `node_modules` | First grep exits 1 with no matches; every marker line references exactly `docs/adr/ADR-0002-governance-validator-adoption.md` | Both command outputs | Not Started | Pending |
| AC-3 | T-1 | Package identity and dependency pins are orange-local and exact | Localized `package.json` and `package-lock.json` | `node -p "const p=require('./tools/governance-validator/package.json');p.name==='@orange/governance-validator'&&Object.values(p.dependencies).every(v=>/^\\d+\\.\\d+\\.\\d+$/.test(v))"` plus `grep -m1 '"name"' tools/governance-validator/package-lock.json` | The node expression prints `true`; the lockfile's first name entry is `@orange/governance-validator` | Both command outputs | Not Started | Pending |
| AC-4 | T-2 | ADR-0001's approval metadata and statuses survive template alignment | Aligned `docs/adr/ADR-0001-orange-issue-governance.md` | `grep -n "Approval Evidence.*Approve\|Decision Status.*Accepted\|Implementation Status.*Blocked" docs/adr/ADR-0001-orange-issue-governance.md` | All three assertions are present in active metadata, with Approver `@hailingu` intact and no re-approval ritual recorded as required | Grep output; validator run of AC-6 confirming index agreement | Not Started | Pending |
| AC-5 | T-2 | ADD-0001 remains a Draft ADD with agreeing index rows after alignment | Aligned ADD and both index files | `grep -n "Design Status.*Draft" docs/architecture/ADD-0001-orange-v1-baseline.md` plus AC-6's validator run | ADD-0001 active metadata shows Design Status `Draft`; the architecture index row shows `Draft` and the same scope fields | Grep output and validator output | Not Started | Pending |
| AC-6 | T-3 | The full repository passes configured validation | Aligned repository | `npm run validate --prefix tools/governance-validator` from repository root | Exit code 0 and no error diagnostics printed | Command output and exit code | Not Started | Pending |
| AC-7 | T-3 | ADR-0001's recorded exit criterion is demonstrated satisfiable from repository root | Both configured commands available | Run `npm test --prefix tools/governance-validator` and `npm run validate --prefix tools/governance-validator` from repository root in one shell | Both commands exit 0 | Combined command transcript with both exit codes, citable by ADR-0001's resume change | Not Started | Pending |

Allowed final check statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
`Fail` blocks completion. `N/A` is valid only when the check's stated trigger or
precondition demonstrably does not apply.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Not Started | Pending |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Not Started | Pending |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | N/A — Architecture Source is `N/A`; no ADD task candidate is selected under the blocker-resolution exception | N/A |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Not Started | Pending |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Not Started | Pending |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | Not Started | Pending |
| A-7 | Contract and baseline risks covered, when applicable | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | Not Started | Pending |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Not Started | Pending |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Archive this record in the same change that retires it when either Decision
Status is `Rejected` with Implementation Status `Not Applicable`, or Decision
Status is `Deprecated` or `Superseded` with Implementation Status `Verified`,
`Complete`, or `Not Applicable`. Before that trigger, retain this section as
inactive future-lifecycle guidance; its checklist does not affect acceptance or
implementation completion. When triggered:

- [ ] Move this file to `archive/ADR-0002-governance-validator-adoption.md` under this same ADR
      root (project `docs/adr/archive/` or `<service>/docs/adr/archive/`).
- [ ] Update every code marker that cites this file's pre-archive path to the new
      archive path, or remove the marker if the governed code was deleted.
- [ ] If Decision Status is `Superseded`, set the replacement record's
      `Supersedes` field and this record's `Superseded By` field to each other's
      final repository-relative path.
- [ ] If no record supersedes this one, retain `Superseded By: None`.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with the archived
      path, scope, and final status.
- [ ] Confirm no ADR or OCR outside an `archive/` directory, and no code marker, still cites the pre-archive path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-13 | Initial Proposed draft under the AGENTS.md blocker-resolution exception for ADR-0001's recorded blocker; records the owner's GitHub Projects demand-tracking decision and the approval-preserving alignment determination | @zcode |
