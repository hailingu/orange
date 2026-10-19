# ADR-0006: Validate every table in decision and architecture indexes

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Verified
- **Date**: 2026-10-15
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-15T09:54:44Z
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

Quality-review finding 2 is reproduced at
`1f876464862c69c5a54424d91fbfd6937983e23c`.
`createRelationshipValidator::validateIndex` calls `tableFromContent`
once, so only the first Markdown table contributes indexed paths.

A disposable documentation copy placed a correctly retired record in
`archive/` and synchronized its references. Validation passed while its row
was in the first index table, then failed as missing from the index when the
same row moved to a second Archived Records table. The record, row fields and
path were otherwise unchanged.

This correction covers the index-validation boundary in one implementation
pull request, including the shared table scanner it consumes. Full ADR routing
applies because both repository decision and architecture indexes are governed
contracts.

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

- Enumerating real Markdown tables in recognized record indexes and validating every row.
- Shared duplicate-path detection across all tables in one index.
- Per-table column maps, existing record comparisons and complete path coverage.
- A table enumeration helper in `lib/markdown-parsing.mjs`, relationship-validator wiring in `validate.mjs`, and focused index regressions.

Out of scope:

- Changing record lifecycle, archive eligibility, path resolution, central index columns, ADD handoff or candidate-table semantics.
- General Markdown rendering, new dependencies and the unrelated product runtime.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Complete index coverage versus callers that intentionally use only the first table in a section | Changing the existing helper's return contract would affect candidate, acceptance and Mermaid checks | Add table enumeration for index callers while preserving tableFromContent's first-table contract |

### Constraints [Required]

- `AGENTS.md` Identity, Storage, Filenames, and Provenance makes the full repository-relative path the identity; repeated bare numbers across roots are legal.
- Keep all expected columns and the current record-field/path checks; use one seen-path set per complete index.
- Reuse the existing code-fence stripping, table-cell and separator parsing. Existing comment stripping remains at the CLI input boundary.
- The new enumeration helper consumes active Markdown after the existing comment stripper; direct parser checks apply that same input normalization.
- No dependency or command change. The outer relationship-validator factory's measured cap exception is declared below rather than expanding this fix into a factory redesign.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

N/A — table participation, per-table schema, duplicate scope and legacy helper compatibility are decided here.

## Decision Drivers [Required]

1. **Archive correctness**: moving a valid row between active and archived tables must not erase it from index coverage.
2. **Uniform validation**: a second table must receive the same schema, path and field checks as the first.
3. **Compatibility**: section-specific consumers keep their first-table API.

## Options Considered [Required]

### Option: Enumerate tables at the index boundary

Add `tablesFromContent` using the existing Markdown primitives; iterate its
tables with one index-wide seen-path set.

Pros:

- Supports the existing active/archive organization and detects cross-table duplicates.
- Keeps candidate and acceptance table consumers unchanged.

Cons:

- Adds one parser entry point and corresponding dependency wiring.

### Option: Require a single combined index table

Keep the parser and rewrite index authoring guidance to prohibit separate
archive tables.

Pros:

- Avoids scanner changes.

Cons:

- Avoids the demonstrated failure by restricting document organization and leaves later tables unchecked.

## Decision [Required]

**Selected option**: Enumerate tables at the index boundary.

**Rationale**: Table placement is presentation; the complete set of record rows
must remain subject to the existing index contract.

- **IX-1**: Every real structured table in a record index participates in validation. Each table must declare that index's expected columns; build its column map independently. Non-table prose such as an empty archive notice remains allowed.
- **IX-2**: Every participating row receives the existing path, type, ID, title, status and metadata checks. One shared seen-path set detects a duplicate repository-relative path even when its rows occur in different tables. Bare record IDs in different roots remain legal.
- **IX-3**: Fenced examples and HTML comments contribute no index tables or rows. Absence of a real structured table fails with `INDEX_TABLE_REQUIRED`; a missing required column fails with `INDEX_REQUIRED_COLUMN`; duplicate paths fail with `INDEX_DUPLICATE_PATH`. Each diagnostic includes the index path; preserve the existing explanatory message text after the code.
- **IX-4**: `tableFromContent` continues to return only the first table or undefined. Only index consumers switch to enumeration.

### Consequences [Required]

Positive:

- Active and archived rows share full validation and coverage.

Negative:

- A previously ignored invalid later table will start failing validation.

Mitigations:

- Report the index path and existing row-level details, and test valid multi-table controls before interpreting a new failure as a document defect.

## Implementation Plan [Required]

**Complete task outcome**: Valid record indexes pass with one or several
tables, while later-table schema errors, row errors and cross-table duplicates
are rejected.

**Primary implementation boundary**: Governance validator record-index policy,
with bounded support from its Markdown parser.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Correct complete-index validation with regression coverage | Table enumeration, relationship-validator iteration and CLI wiring; focused real CLI and parser tests, failing on the current second-table omission before implementation | Complete | tablesFromContent enumerates real tables; validateIndex applies independent column maps and the existing row checks with one shared seen-path set; the CLI supplies the new parser. test/index-tables.test.mjs covers IX-1–IX-4 through real CLI and parser cases; source identities and command results below |

**Affected paths**: `tools/governance-validator/lib/markdown-parsing.mjs`,
`tools/governance-validator/lib/relationship-validation.mjs`,
`tools/governance-validator/validate.mjs`,
`tools/governance-validator/test/index-tables.test.mjs`.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| tools/governance-validator/lib/markdown-parsing.mjs | tableFromContent; tableCells; isSeparatorRow; stripFencedCode | N/A — stable symbols are sufficient | Reuse grammar primitives while retaining the first-table contract | 1f876464862c69c5a54424d91fbfd6937983e23c |
| tools/governance-validator/lib/relationship-validation.mjs | createRelationshipValidator::validateIndex; validateIndexRow | N/A — stable symbols are sufficient | Apply row checks to all tables with one shared seen-path set | 1f876464862c69c5a54424d91fbfd6937983e23c |
| tools/governance-validator/validate.mjs | relationshipValidator construction; validateMarkdownFile | N/A — stable symbols are sufficient | Supply enumeration and retain index-wide record coverage | 1f876464862c69c5a54424d91fbfd6937983e23c |

**Migration and rollback strategy [Conditionally Required — this replaces or
changes existing behavior]**: No record or data migration. Stop on any newly
reported error and compare the exact indexed row to its source record; do not
retire or rewrite records merely to restore a green result. Revert the parser,
index consumer and their wiring together to restore the previous behavior.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

| Rule and affected unit | Measured value or condition | Rationale | Risks | Compensating controls | Owner | Removal, review, or permanent rationale | Verification evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Software Engineering Standard › Size And Complexity Guardrails: 80-code-line function cap; lib/relationship-validation.mjs::createRelationshipValidator | 211 code lines including blanks and excluding comment lines, 226 physical lines at the cited revision; scoped result may not exceed 240 code lines | The enclosing factory binds one explicit parser/filesystem context to private validators; moving every nested validator is a separate responsibility-boundary refactor. This patch changes only index iteration inside that existing factory | The enclosing function remains harder to navigate | Every new or modified nested executable unit stays within 60 code lines; focused index tests and the existing relationship suite run through the real CLI; no new mutable global state | @hailingu | Review by 2026-11-15 or the next material factory-boundary refactor, whichever occurs first; remove when its private validators no longer make the factory exceed 80 code lines | Baseline review at 1f876464862c69c5a54424d91fbfd6937983e23c: 211 code lines; final factory 214 code / 229 physical lines, within the approved 240-line limit. validateIndex is 20 code lines; validateIndexRow is 17; focused suite 36/36 and routed suite 271/271 pass. Final source identities below |

The CLI's 761-line file also crosses the 600-line review threshold. Its small
dependency-wiring change remains in the existing composition root and must
leave the file within the 800-line cap; no additional cap waiver is proposed.

**Verified implementation evidence**: The content IDs below were computed with
`git hash-object <path>` over uncommitted source bytes; no commit or push was
created. The repository baseline remains
`1f876464862c69c5a54424d91fbfd6937983e23c`. The failing-first run also included
the already verified ADR-0005 correction (CLI content ID
`7cc83e17ffcd65acea55f83eda48bce349221405`), which this slice preserves.

| Affected path | Stable implementation anchor | Git blob content ID |
| --- | --- | --- |
| tools/governance-validator/lib/markdown-parsing.mjs | tablesFromContent; unchanged tableFromContent | 54e4724f13ffe3cc135c952e3e915cfceca6e592 |
| tools/governance-validator/lib/relationship-validation.mjs | createRelationshipValidator::validateIndex; validateIndexRow | f8a0b57d552ed8416f6bd1fbcc379c5d81f47539 |
| tools/governance-validator/validate.mjs | tablesFromContent import and relationshipValidator construction | f4da255b758ccfafeffdca0c4932f76528c31a00 |
| tools/governance-validator/test/index-tables.test.mjs | IX-1, IX-2, IX-3 and IX-4 cases; repository; assertIndexFailure | a9a1fe37bba9b3fe55c2d3a7d21b1a8a519737b6 |

**Structured engineering review**: The scanner advances its cursor on every
branch and never restarts a whole-document search for the next table. Existing
comment/fence normalization and cell/separator parsing are reused. A single
seen-path set is created before table iteration; each table builds its own
column map and every row reaches the existing record comparisons. The factory
is 214 code lines under its accepted exception; changed nested methods are
20 and 17, the new scanner 22, and test helpers at most 20. Changed routine
control nesting is at most three levels. Production files are 258, 351 and
770 physical lines; the test module is 234. No cap outside the recorded
factory exception is exceeded. Cyclomatic complexity: N/A — no configured
complexity tool; unit spans and nesting were reviewed instead. The CLI's
existing threshold review still applies to its bounded composition wiring.
Existing fixture builders, metadata parser and dependency manifests have no
diff from the baseline. Tests exercise fixture semantics and stable diagnostic
codes; none asserts repository prose or implementation layout.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| IX-1 | AGENTS.md › Identity, Storage, Filenames, and Provenance and Archival; this record › Decision › IX-1 | Index coverage includes every real table and each table has its own complete expected schema | AC-1, AC-2 | Equivalent one-, two- and three-table fixtures; later headers reorder columns or omit one required column |
| IX-2 | AGENTS.md › Identity, Storage, Filenames, and Provenance; this record › Decision › IX-2 | All rows retain existing checks; duplicate full paths fail across tables while same numbers in different roots remain legal | AC-1, AC-2 | Later-table field mutation, duplicate full path and project/service same-number controls |
| IX-3 | This record › Decision › IX-3 | Examples/comments do not supply index data; missing table/column and duplicate path use their declared diagnostic codes | AC-2, AC-3 | Real CLI fixtures exercise every declared failure code and ignored examples |
| IX-4 | This record › Decision › IX-4 | First-table consumers retain the existing API contract | AC-3, AC-4 | Direct parser expectations plus the routed suite's existing candidate and acceptance checks |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable — the same rows appear in differently ordered tables or a path repeats in a later table | Index policy | Permute table order and distribute rows across tables | Valid permutations exit 0; duplicate full paths exit 1 regardless of table order | AC-1, AC-2 | Pass | AC-1/AC-2 pass: four equivalent layouts, independent header order, and duplicate full paths in both forward/reverse table order across both index schemas; focused suite 36/36 |
| timeout and deadline | N/A — this is a synchronous local Markdown scan with no timer, transport or retry | Markdown parser | Inspect the enumeration loop and run the routed suite | Each source line is advanced without repeated whole-document rescans; suite exits 0 | AC-4 | N/A — no deadline-bearing operation | N/A — no timer |
| cancellation and interruption | N/A — table enumeration changes no process lifecycle and retains no partial external state | Index policy | Inspect the scoped diff | No handler, transaction or externally visible partial write | AC-4 | N/A — read-only synchronous scan | N/A — no retained state |
| resource bounds and backpressure | Applicable — multiple tables must not reset duplicate tracking or repeatedly rescan the full document | Index policy and parser | Scanner review plus multi-table tests and resulting-unit measurements | One advancing scan; one seen-path set per index; declared size limits satisfied | AC-1, AC-2, AC-4 | Pass | AC-1/AC-2/AC-4 pass: one advancing table loop, one seen-path set outside table iteration; factory 214 code lines, new scanner 22, index method 20, CLI 770 physical lines; routed suite 271/271 |
| framework or trust-boundary rejection | Applicable — a later table contains an invalid field, missing column or misleading fenced/comment example | Index policy | Mutate real CLI fixtures, including examples before real tables | Invalid real content exits 1 with the declared code where specified; examples never satisfy coverage | AC-2, AC-3 | Pass | AC-2/AC-3 pass: both later-table schemas and all seventeen field/path mutations are rejected; literal examples cannot satisfy membership; INDEX_TABLE_REQUIRED / INDEX_REQUIRED_COLUMN / INDEX_DUPLICATE_PATH appear with index paths |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Index membership is invariant under valid table distribution | Otherwise-valid records include an archived Rejected ADR, an archived OCR, an archived ADD and project/service ADRs sharing a bare number; all references and lifecycle metadata agree; compare one, two and three tables and table order permutations | Real CLI parameterized cases in test/index-tables.test.mjs using node --test tools/governance-validator/test/index-tables.test.mjs | Every equivalent layout exits 0; second-table archive records are never reported missing; reordered columns compare by their own table header | Failing-first second-table reproduction, passing cases, CLI exit codes and revision | Pass | After approval, the valid single-table control exits 0 while the two-table, reversed-table and three-table cases fail because later records are missing. After implementation, all four layouts exit 0 with independently reversed later headers, archived Rejected ADR / completed OCR / Deprecated ADD, and project/service ADR-0001 records. Focused suite: 36/36 |
| AC-2 | T-1 | Later-table invalid content receives full index validation | Valid multi-table controls mutated one at a time: duplicate full path across tables; missing Path column in the later table; later-row Type, ID, Title, status, Scope, Architecture Source or Superseded By changed to a value different from its source record; Path changed to a nonexistent record | Focused CLI regressions with one case per mutated field | Each exits 1; duplicate includes INDEX_DUPLICATE_PATH, missing column includes INDEX_REQUIRED_COLUMN; each diagnostic names the index; field/path disagreement names the affected record path | Inputs, statuses and diagnostic codes/paths | Pass | Both index schemas reject duplicates in forward/reverse table order with INDEX_DUPLICATE_PATH and the record path; both reject a later table missing Path with INDEX_REQUIRED_COLUMN. All seventeen isolated later-row field/path mutations exit 1 with an index-prefixed diagnostic naming the affected record. Focused suite: 36/36 |
| AC-3 | T-1 | Examples and legacy first-table semantics remain distinct from real index coverage | Markdown containing two different real tables plus fenced and HTML-comment examples; an index with only examples; a real table missing a record that appears only in an example | Direct parser checks after the existing stripHtmlComments normalization and real CLI fixtures; no repository-source text assertions | Enumeration returns only real tables; tableFromContent returns the first real table or undefined; example-only index exits 1 with INDEX_TABLE_REQUIRED; example-only membership never satisfies coverage | Parser results and CLI statuses/codes | Pass | Both schemas pass with misleading fenced/comment tables before real multi-table data; example-only indexes exit 1 with INDEX_TABLE_REQUIRED; example-only record membership fails. Direct parser cases preserve independent cells, return no tables for examples/unstructured input, and retain tableFromContent's first-table-or-undefined result. Focused suite: 36/36 |
| AC-4 | T-1 | Routed checks and scoped engineering controls pass | Accepted implementation slice | npm test --prefix tools/governance-validator; npm run validate --prefix tools/governance-validator; git diff --check; structured scanner and size review | All commands exit 0; one advancing table scan; factory at most 240 code lines, other changed executable units at most 60; validate.mjs at most 800 physical lines; no candidate/parser API or dependency change | Command summaries, source revision, size measurements, loop and scope review | Pass | npm test --prefix tools/governance-validator: 271/271 pass, 0 failures/skips, 26.363 seconds. npm run validate --prefix tools/governance-validator and git diff --check exit 0. Review confirms one advancing table scan and one index-wide seen set; factory 214 code lines, other changed routines at most 22, CLI 770 physical lines. Existing first-table API and dependencies are unchanged |

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Complete | @hailingu replied with exact Approve for this record in this chat; active metadata records 2026-10-15T09:54:44Z and the same self-declared identity |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Complete | T-1 is Complete; AC-1–AC-4 pass with actual CLI/parser results and source content identities; all three applicable risk rows are Pass |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | N/A — maintenance, not product-demand implementation | N/A — Architecture Source is reasoned N/A; no ADD selection or relink |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Complete | Final structured review confirms required content is complete, every conditional trigger has evidence or a specific N/A assessment, active lifecycle metadata and index agree, and no completion evidence is Pending |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Complete | All four checks retain explicit inputs, deterministic methods and binary outcomes; the passing cases cover layout/header permutations, each declared diagnostic, seventeen field/path mutations and both parser APIs |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | Complete | The accepted factory exception remains bounded: resulting createRelationshipValidator is 214 code / 229 physical lines, below 240; modified nested routines are 20 and 17 code lines; new enumeration is 22. Owner @hailingu and review deadline 2026-11-15 remain in force; focused and routed suites pass |
| A-7 | Contract and baseline risks covered, when applicable | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | Complete | IX-1–IX-4 map to passing checks; all five risk dimensions were reviewed. Table ordering, resource bounds and trust-boundary rejection are Pass with permutation, duplicate, field/path, schema and example tests; timer/interruption rows retain specific N/A reasons |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Complete | npm run validate --prefix tools/governance-validator exits 0 after implementation; final record/index validation is included in the completion command summary |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

This record is Accepted and has not been retired. Retain the following as
inactive future-lifecycle guidance; it does not affect proposal approval or
implementation completion. When the archival trigger in the heading applies:

- [ ] Move this record to `docs/adr/archive/ADR-0006-complete-record-index-validation.md`.
- [ ] Update every source marker and reference to the final archived path in the same change.
- [ ] Preserve reciprocal replacement paths when superseded; otherwise retain Superseded By: None.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with its final path, scope and statuses.
- [ ] Confirm no active record or code marker still cites the former path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-15 | Draft a separate Proposed repair for quality-review finding 2, with one implementation boundary, deterministic acceptance checks, contract traceability and five-dimension risk assessment. The owner explicitly waived serialization for preparing findings 1, 2, 4 and 5 in this chat; individual approval before implementation remains required. No implementation is authorized or claimed | @codex |
| 2026-10-15 | Proposal verification: npm test --prefix tools/governance-validator passes 208/208; npm run validate --prefix tools/governance-validator and git diff --check pass. Four disposable Accepted-stage preflights pass without changing repository approval state. A fresh C++ verification build with Apple clang 21.0.0 passes 2/2 CTest targets without warnings; clang-format check passes. The build used a task-owned temporary directory to preserve the existing service/build output. Structured planning review covers requirement levels, routing and per-root number uniqueness, one boundary/subtask per record, exact acceptance inputs and outputs, contract/risk traceability and truthful unexecuted implementation checks | @codex |
| 2026-10-15 | @hailingu replied with exact Approve in this chat identifying only this record. Approval recorded at 2026-10-15T09:54:44Z; identity was explicitly declared in this approval context. Decision Status becomes Accepted and T-1 enters In Progress; the remaining C++ decomposition and fixture-lifecycle records stay Proposed | @codex |
| 2026-10-15 | Implementation verified: after completing the archived OCR fixture's required planned runbook fields, the valid single-table control passes and the failing-first suite records 4 pass / 32 expected failures (10.737 seconds). After table enumeration and index iteration, focused cases pass 36/36 (10.660 seconds); npm test --prefix tools/governance-validator passes 271/271 (26.363 seconds), with no failures or skipped cases. Governance validation, whitespace and structured requirement/contract/risk/scope reviews pass. Factory exception and source content identities are recorded above. This record becomes Verified; other Proposed repair records remain unapproved. Disposable test output and owned fixtures are deleted before task completion | @codex |
