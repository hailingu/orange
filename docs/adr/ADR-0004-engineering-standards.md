# ADR-0004: Engineering standards preceding implementation

## Metadata [Required]

- **Decision Status**: Proposed
- **Implementation Status**: Not Started
- **Date**: 2026-10-14
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
- **Rejection Reason [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Retired By [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with the concrete `@<actor-id>`
- **Retirement Time [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with an ISO 8601 date-time containing `Z` or an explicit `±HH:MM` offset
- **Retirement Evidence [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: Pending — replace with exactly `Deprecate` or `Supersede`
- **Retirement Reason [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Blocked From [Conditionally Required — Implementation Status is `Blocked`]**: N/A — Implementation Status is not Blocked
- **Blocker And Evidence [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Owner [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Exit Or Recheck Criterion [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Related [Optional]**: Blocked record docs/adr/ADR-0003-local-runtime-path.md (Accepted, Blocked) — this ADR is its sole blocker-resolution record under the AGENTS.md serialization exception
- **Architecture Source [Conditionally Required — product demand]**: N/A — blocker-resolution governance documentation under the AGENTS.md serialization exception, not product-demand implementation
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

AGENTS.md requires reading `docs/development/software-engineering-standard.md`
and every matching language or platform standard before writing, modifying, or
reviewing source code, and its validator routing row names the common standard
as required reading. Neither document exists. ADR-0003 (the accepted runtime
slice) entered implementation on 2026-10-13; the repository owner then
directed on 2026-10-14 that the engineering standards precede any
implementation ("先完善 development 里的软件工程标准，c++20 开发标准"), reverted the
started transport code (never committed), and pointed at the PlotWeave
repository's `docs/development` standards as the reference for both content
and usage model.

ADR-0003 is now the sole non-terminal record, is `Accepted` and `Blocked`,
its recorded blocker (this standards prerequisite) cannot be resolved within
its accepted scope or by an OCR, and resolving it requires a Full ADR — every
condition of the AGENTS.md blocker-resolution exception holds, and this ADR
exists solely to remove that named blocker.

PlotWeave's model, studied on 2026-10-14, is a two-tier system: a
repository-wide software-engineering standard (precedence, size and
complexity guardrails, module boundaries, dependency direction, abstraction
and pattern guidance, state/concurrency/failure boundaries, testing, and an
exceptions process) plus per-language standards that narrow it (required
reading chain, pinned toolchain, exact verification commands, layout,
language-specific practices, and a before-writing-code checklist). Standards
are used through the required-reading chain and AGENTS.md routing rows, not
as standalone prose.

## Scope [Required]

In scope:

- `docs/development/software-engineering-standard.md` — the repository-wide
  baseline adapted from the PlotWeave structure: purpose and precedence;
  size and complexity guardrails; responsibility and module boundaries;
  dependency design; abstractions and pattern selection; state, concurrency,
  and failure boundaries; testing and change design; incremental adoption
  and exceptions (recorded in ADR Engineering Exceptions subsections per
  orange's governance); and a before-writing-code checklist.
- `docs/development/cpp-development-standard.md` — the C++20 standard
  narrowing the baseline: required reading chain (baseline plus the C++ Core
  Guidelines); pinned toolchain and verification commands (CMake, compiler,
  CTest trio); `service/` layout and naming; error-handling policy; resource
  and concurrency practices; testing rules; dependency rules.
- `docs/README.md` — catalog both standards in the Language And Platform
  Standards Catalog so the AGENTS.md required-reading rule resolves them.

Out of scope:

- A TypeScript standard (authored when web client source first lands, in the
  same governed change that adds its routing row).
- Automated file-size or complexity enforcement in the governance validator
  (future governed validator change; until then the guardrails are
  review-enforced and say so).
- A workflow document, quality-gate tooling, coverage tooling, and CI — none
  exist in orange and each requires its own decision.
- Any change to AGENTS.md, the ADR/ADD records, or product source.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Adopting a proven standards structure versus tailoring every number to a greenfield project | Copying PlotWeave wholesale imports tooling promises orange cannot keep; inventing from scratch loses a validated model | Adapt PlotWeave's two-tier structure and usage model, keep its proven numeric guardrails, and state honestly which rules are review-enforced because no automated check exists yet |
| TN-2 | Strict guardrails versus learning purpose | The project exists to study C++ and the Web; over-heavy process can bury the learning | Keep the guardrails small, mechanically checkable where possible, and route every exception through one recorded instrument (ADR Engineering Exceptions) |

### Constraints [Required]

- AGENTS.md remains authoritative; a language standard may narrow the
  baseline but not weaken or contradict it.
- Standards must not promise tooling that does not exist; every automated
  claim must name its command, and review-enforced rules must say so.
- The exception instrument is the ADR Engineering Exceptions subsection per
  AGENTS.md, not pull-request records.
- orange is single-user local software (ADD-0001 D-06): concurrency and
  distribution rules scale to that reality.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

| ID | Question | Owner | Due | Status | Resolution and Evidence |
| --- | --- | --- | --- | --- | --- |
| Q-1 | Which numeric guardrails apply? | @hailingu | 2026-10-14 | Resolved | Keep PlotWeave's proven values: review thresholds 600/1,000 physical lines and caps 800/1,800 for maintained source/test files; executable-unit threshold 60 and cap 80 code lines (comments excluded); nesting threshold 4 and cap 6. File caps are review-enforced in orange until automated enforcement is separately governed |
| Q-2 | What is the C++ error-handling policy? | @hailingu | 2026-10-14 | Resolved | No exceptions across module API boundaries; recoverable failures return `std::optional` or `std::variant` error values owned by the failing layer; assertions only for provably local invariants |
| Q-3 | Which C++ toolchain and verification commands? | @hailingu | 2026-10-14 | Resolved | CMake 3.21+, the installed Apple clang with `-std=c++20`, and the trio `cmake -S service -B service/build`, `cmake --build service/build`, `ctest --test-dir service/build`; formatting via clang-format with the repository configuration landing in the first C++ implementation change |

## Decision Drivers [Required]

1. **Standards before code**: the owner's sequencing direction unblocks every later slice.
2. **Proven model, honest enforcement**: reuse PlotWeave's structure without inheriting its tooling debts.
3. **AGENTS.md integration**: the required-reading chain and routing rows must resolve the standards by name.
4. **Small and reviewable**: two documents, no tooling promises beyond existing commands.

## Options Considered [Required]

### Option: Adapt PlotWeave's two-tier standards

Author the baseline plus a C++20 standard using PlotWeave's structure,
numbers, and usage model, adjusted for orange's governance instruments.

Pros:

- Validated structure and thresholds; immediate familiarity for the owner.
- Integrates with AGENTS.md required reading and ADR exceptions as-is.

Cons:

- Requires honest down-scoping of automated enforcement claims.

### Option: Author minimal orange-only standards from scratch

Write short bespoke rules without the reference model.

Pros:

- Smallest documents.

Cons:

- Loses the proven guardrail calibration and pattern guidance; likely to be
  rewritten once real code arrives.

### Option: Defer standards until after the runtime slice

Implement ADR-0003 first, standards later.

Pros:

- Fastest path to running code.

Cons:

- Directly contradicts the owner's 2026-10-14 direction and the AGENTS.md
  read-before-source rule's intent.

## Decision [Required]

**Selected option**: Adapt PlotWeave's two-tier standards.

**Rationale**: the owner named PlotWeave as the reference; its model is
proven in a sibling repository, its guardrails transfer unchanged, and its
usage model (required-reading chain plus routing) is exactly how orange's
AGENTS.md already consumes standards. The only adaptation is enforcement
honesty and the ADR-based exception instrument.

### Consequences [Required]

Positive:

- Every later implementation slice reads one baseline plus one matching
  language standard before writing code, as AGENTS.md already requires.
- ADR-0003's blocker is removable and V1 implementation resumes under
  written rules.

Negative:

- Review-enforced (not automated) file caps can drift between reviews.

Mitigations:

- The standards state this gap explicitly; adding automated size enforcement
  to the governance validator is a named future governed change.

## Implementation Plan [Required]

**Complete task outcome**: `docs/development/software-engineering-standard.md`
and `docs/development/cpp-development-standard.md` exist as the orange
baseline and C++20 standard adapted from the PlotWeave model, are cataloged
in `docs/README.md`, satisfy the AGENTS.md required-reading contract, and
ADR-0003's exit criterion is demonstrably satisfied.

**Primary implementation boundary**: Repository governance documentation.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Author both standards | The two `docs/development/` files with the sections listed in Scope, orange's governance instruments, and the resolved Q-1 through Q-3 parameters | Not Started | Pending |
| T-2 | Catalog and resume readiness | `docs/README.md` Language And Platform Standards Catalog entries; verification runs; confirmation that ADR-0003's Blocker Exit Or Recheck Criterion is satisfiable | Not Started | Pending |

**Affected paths**: `docs/development/software-engineering-standard.md`,
`docs/development/cpp-development-standard.md`, `docs/README.md`

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| `docs/development/software-engineering-standard.md` | the `Size And Complexity Guardrails` and `Incremental Adoption And Exceptions` headings | N/A — stable symbol or anchor is sufficient | Own the repository-wide thresholds, caps, and the ADR-exception instrument that every language standard narrows | Planned pre-commit state 2026-10-14 — replace with the implementing commit before completion |
| `docs/development/cpp-development-standard.md` | the `Required Reading` and `Baseline Practices` headings | N/A — stable symbol or anchor is sufficient | Own the C++20 toolchain, verification trio, error policy, and layout rules that narrow the baseline for `service/**` | Planned pre-commit state 2026-10-14 — replace with the implementing commit before completion |
| `docs/README.md` | the `Language And Platform Standards Catalog` section | N/A — stable symbol or anchor is sufficient | Resolve the AGENTS.md required-reading rule to both standards by name | Planned pre-commit state 2026-10-14 — replace with the implementing commit before completion |

**Migration and rollback strategy [Conditionally Required — this replaces or
changes existing behavior]**: N/A — new documents; rollback is deleting them
and the catalog rows.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

| Rule and affected unit | Measured value or condition | Rationale | Risks | Compensating controls | Owner | Removal, review, or permanent rationale | Verification evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| N/A — no software-engineering standard exists before this ADR, so no rule is exceeded or waived; the documents themselves define the rules going forward | N/A — no standard to measure against | N/A | N/A | N/A | N/A | N/A | N/A |

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| CC-1 | `AGENTS.md` › Core Rules | Before writing, modifying, or reviewing source code, first read `docs/README.md` and every matching language or platform standard listed there in full | AC-1, AC-2 | AC-2 asserts the README catalog lists both standards by name and path; AC-1 asserts both files exist with the sections that make the rule executable |
| CC-2 | `AGENTS.md` › Scope Routing › `tools/governance-validator/**` row | Required reading includes `docs/development/software-engineering-standard.md` | AC-1 | AC-1 asserts the file exists with the baseline sections |
| CC-3 | `docs/adr/ADR-0003-local-runtime-path.md` › Blocker Exit Or Recheck Criterion | ADR-0004 is Accepted and Complete with both standards in place and cataloged; ADR-0003 then returns to In Progress | AC-3, AC-4 | AC-3 demonstrates the criterion's inputs; AC-4 records the readiness statement ADR-0003's resume change cites |
| CC-4 | `AGENTS.md` › Document Requirement Levels | Documents that define process behavior distinguish requirement levels and complete every triggered section | AC-1 | Structured review plus the repository validate command |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | N/A — documentation deliverable with no runtime behavior | Documentation | N/A | N/A | N/A | N/A — no concurrent behavior exists | N/A — not run |
| timeout and deadline | N/A — documentation deliverable with no runtime behavior | Documentation | N/A | N/A | N/A | N/A — no deadline-bound behavior exists | N/A — not run |
| cancellation and interruption | N/A — documentation deliverable; an interrupted authoring run resumes by editing the files | Documentation | N/A | N/A | N/A | N/A — no interruptible runtime state exists | N/A — not run |
| resource bounds and backpressure | N/A — static Markdown files with no inputs or queues | Documentation | N/A | N/A | N/A | N/A — no resource admission exists | N/A — not run |
| framework or trust-boundary rejection | N/A — the documents consume no external input and execute nothing | Documentation | N/A | N/A | N/A | N/A — no external trust boundary exists | N/A — not run |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Both standards exist with their required sections | Drafted `docs/development/` files | `test -f` on both paths, then inspect headings | Both files exist; the baseline contains Purpose And Precedence, Size And Complexity Guardrails, Responsibility And Module Boundaries, Dependency Design, Abstractions And Design Patterns, State/Concurrency/Failure, Testing And Change Design, Incremental Adoption And Exceptions, and Before Writing Code; the C++ standard contains Required Reading, Baseline Practices, layout, error policy, testing, and Before Writing Code | Command output and inspection record | Not Started | Pending |
| AC-2 | T-2 | The README catalog resolves the required-reading rule | Updated `docs/README.md` | `grep -n "software-engineering-standard\|cpp-development-standard" docs/README.md` | Both standards are listed in the Language And Platform Standards Catalog with paths and scope | Grep output | Not Started | Pending |
| AC-3 | T-2 | The repository passes configured validation | Authored documents | `npm run validate --prefix tools/governance-validator` from repository root | Exit code 0 with no diagnostics | Command output and exit code | Not Started | Pending |
| AC-4 | T-2 | ADR-0003's exit criterion is demonstrably satisfiable | ADR-0004 accepted and complete | Inspect ADR-0003's Blocker Exit Or Recheck Criterion against the delivered state | Both standards exist and are cataloged, so the criterion's inputs hold; the resume statement is recorded for ADR-0003's resume change to cite | Inspection record citable by ADR-0003's resume change | Not Started | Pending |

Allowed final check statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.
`Fail` blocks completion. `N/A` is valid only when the check's stated trigger or
precondition demonstrably does not apply.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Not Started | Pending |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Not Started | Pending |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | N/A — Architecture Source is `N/A`; blocker-resolution governance documentation selects no ADD task candidate | N/A |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Not Started | Pending |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Not Started | Pending |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | N/A — no standard exists to exceed; the subsection records its N/A reason | N/A |
| A-7 | Contract and baseline risks covered, when applicable | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | Not Started | Pending |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Not Started | Pending |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Archive this record in the same change that retires it when either Decision
Status is `Rejected` with Implementation Status `Not Applicable`, or Decision
Status is `Deprecated` or `Superseded` with Implementation Status `Verified`,
`Complete`, or `Not Applicable`. Before that trigger, retain this section as
inactive future-lifecycle guidance; its checklist does not affect acceptance or
implementation completion. When triggered:

- [ ] Move this file to `archive/ADR-0004-engineering-standards.md` under this same ADR
      root (project `docs/adr/archive/` or `<service>/docs/adr/archive/`).
- [ ] Update every code marker that cites this file's pre-archive path to the new
      archive path, or remove the marker if the governed code was deleted.
- [ ] If Decision Status is `Superseded`, set the replacement record's
      `Supersedes` field and this record's `Superseded By` field to each other's
      final repository-relative path.
- [ ] If no record supersedes this one, retain `Superseded By: None — not superseded`.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with the archived
      path, scope, and final status.
- [ ] Confirm no ADR or OCR outside an `archive/` directory, and no code marker, still cites the pre-archive path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-14 | Initial Proposed draft under the AGENTS.md blocker-resolution exception for ADR-0003's recorded standards blocker; records the PlotWeave two-tier adaptation decision, the resolved guardrail, error-policy, and toolchain parameters, and the cataloging plan | @zcode |
