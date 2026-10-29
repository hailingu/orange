# ADR-0011: Two-layer quality gates — hosted CI and a local SonarQube gate

## Metadata [Required]

- **Decision Status**: Proposed
- **Implementation Status**: Not Started
- **Date**: 2026-10-29
- **Author**: @zcode
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — individual approval required
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — individual approval required
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Pending — individual approval required
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
- **Related [Optional]**: docs/adr/ADR-0009-http-parser-verification-ci.md (the existing macOS hosted workflow this record extends); docs/adr/ADR-0003-local-runtime-path.md (the delivered runtime whose suites the gates run)
- **Architecture Source [Conditionally Required — product demand]**: N/A — CI and quality-gate tooling, not product-demand implementation
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

ADR-0003 delivered the local runtime with its full verification suites, and
ADR-0009 put disposable parser verification on a hosted macOS runner, but the
repository has no pull-request gate beyond that one workflow, no SonarQube
quality gate, and no branch protection: `dev` and `main` accept merges on
review alone. The owner directed (2026-10-29) to reference the PlotWeave
repository's gate model — hosted CI checks plus a SonarQube gate against a
locally hosted SonarQube server whose project (`orange`) and project-scoped
analysis token (`ORANGE_SONAR_TOKEN`) already exist — with the SonarQube
project pre-created, which the owner did in the server UI.

The SonarQube 26.8 server at the learner's machine exposes the bundled
TypeScript/JavaScript analyzer but no C/C++ analyzer plugin; hosted GitHub
runners cannot reach that local server. Any gate design must therefore split
duties the way PlotWeave does: hosted runners prove builds, tests and the
governance validator; the local script proves the SonarQube Quality Gate.

## Scope [Required]

In scope:

- `sonar-project.properties` — the `orange` project analysis descriptor:
  projectKey `orange`, UTF-8, sources `web/src` (TypeScript through the
  bundled analyzer), test inclusions and matching exclusions for `web/test`,
  and no C++ sources (no analyzer exists on the server; the service stays
  covered by the hosted CTest and sanitizer gates — recorded as a follow-up
  if a C++ analyzer is ever installed).
- `scripts/sonar-quality-gate.sh` — the local SonarQube gate: requires
  `SONAR_HOST_URL` to be set explicitly (never SonarCloud by accident) and
  `ORANGE_SONAR_TOKEN`; refuses when `sonar-scanner`, `curl` or `node` are
  missing; runs the web tests and build first; scans with
  `-Dsonar.qualitygate.wait=true`; reads the scanner's `report-task.txt`;
  then requires the server-side Quality Gate status `OK` and zero
  unresolved issues in the new-code period (`sinceLeakPeriod=true`); the
  token reaches the scanner and curl only through environment/standard
  input, never argv or logs; no coverage report is claimed in this first
  cut (no coverage pipeline exists yet — a named follow-up).
- `.github/workflows/ci.yml` — the hosted PR/push gate on `dev` and `main`
  with three jobs, all on the pinned actions already proven by PlotWeave
  (`actions/checkout` and `actions/setup-node` at their pinned SHAs):
  `service` (macos-15: the configured CMake/CTest trio; kqueue requires
  Darwin) gated on `service/**` and the workflow file; `web` (npm ci, test,
  build) gated on `web/**` and the workflow file; `governance` (the two
  governance-validator commands) gated on `tools/governance-validator/**`,
  `docs/**`, `AGENTS.md` and the workflow file. Read-only `permissions`,
  per-workflow concurrency with cancel-in-progress, ten-minute job
  timeouts, no secrets (the Sonar server is unreachable from hosted
  runners by design; Sonar stays the local gate).
- Branch protection on `dev` and `main`: no direct pushes; the three `ci`
  jobs and the ADR-0009 workflow's check required for merge.
- `AGENTS.md` Scope Routing rows for the two new maintained configuration
  paths, added in this same governed change.

Out of scope:

- Coverage generation and a coverage floor (PlotWeave's 80% model) — no
  coverage pipeline exists for either stack yet; adopting one is a separate
  governed change.
- Sonar analysis of `service/**` C++ sources and of `tools/governance-validator`.
- The gate-evidence ledger and pre-push hook machinery PlotWeave carries;
  orange's local gate is invoked manually or by future hooks.
- Any change to the ADR-0009 workflow beyond its already-delivered state.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Sonar coverage breadth versus server reality | The local SonarQube has no C++ analyzer, so most of orange's code (the service) has no static analysis | Accept TS-only static analysis in this record; the service is gated by hosted CTest and sanitizers, and a C++ analyzer is a follow-up if installed |
| TN-2 | Local Sonar reachability versus hosted runners | The Sonar server is on the learner's machine; hosted runners cannot scan | Split the gates: hosted CI proves builds/tests/governance, the local script proves the Quality Gate — the same split PlotWeave chose |

### Constraints [Required]

- The C++ service requires Darwin (kqueue), so hosted jobs run on macOS.
- Dependencies and actions stay exactly pinned (AGENTS.md; PlotWeave's proven
  action SHAs are reused rather than floating tags).
- Secrets never enter the repository; `ORANGE_SONAR_TOKEN` stays a local
  environment variable.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

| ID | Question | Owner | Due | Status | Resolution and Evidence |
| --- | --- | --- | --- | --- | --- |
| Q-1 | Which Sonar project and token? | @hailingu | 2026-10-29 | Resolved | Owner direction 2026-10-29: the pre-created server project `orange` with the project-scoped `ORANGE_SONAR_TOKEN`; reconnaissance confirmed both valid and the two gate APIs reachable |

## Decision Drivers [Required]

1. Reviews alone proved too weak a merge gate for a learning codebase where
   every round lands large test-first changes.
2. The gate must run the exact verification commands the standards define,
   not a parallel weaker set.
3. The Sonar server's locality is a fixture of the owner's environment, not
   a choice this record can change.

## Options Considered [Required]

### Option: GitHub-only checks

Run CTest, web tests and the governance validator on hosted runners; no
Sonar at all.

Pros: one environment, no split. Cons: leaves static analysis entirely to
review; ignores the working local SonarQube the owner already set up.

### Option: SonarCloud

Move scanning to SonarCloud so hosted runners can reach it.

Pros: single hosted pipeline. Cons: a second external service and secret;
the owner's direction names the local server and the existing project.

### Option: Two-layer gates (hosted CI + local Sonar gate)

The PlotWeave shape adapted to orange.

Pros: each gate runs where its dependencies live; no new services or
secrets in GitHub; the token never leaves the learner's machine. Cons:
the Sonar gate is manual (no hosted enforcement) until hook machinery
lands in a follow-up.

## Decision [Required]

**Selected option**: Two-layer gates.

**Rationale**: the hosted runners can only prove what they can build and
run, and the Sonar server can only be reached from the learner's machine;
splitting the gates along that line reuses each environment for exactly
what it can enforce, keeps the repository free of new secrets, and matches
the proven PlotWeave model the owner named. The first cut deliberately
omits coverage floors and C++ static analysis rather than inventing weak
substitutes, recording both as follow-ups.

### Consequences [Required]

Positive:

- Pull requests into `dev`/`main` fail loudly when suites, the web build or
  governance validation regress.
- New code cannot merge with SonarQube new-code issues once the local gate
  is part of the owner's flow.
- Branch protection turns the checks from advisory into blocking.

Negative:

- The Sonar gate relies on the owner running it; nothing hosted enforces it
  (the follow-up hook/ledger machinery is out of scope).
- C++ sources carry no static analysis in this record.

Mitigations:

- The hosted CI covers every service suite including sanitizers and the
  real-socket integration run.
- The omission list (coverage, C++ analysis, hosted Sonar enforcement) is
  explicit here so each follow-up is a deliberate governed change.

## Implementation Plan [Required]

**Complete task outcome**: Pull requests into `dev`/`main` must pass the
hosted `ci` workflow (service trio, web tests/build, governance validation)
as required status checks with direct pushes disabled, and the local
`scripts/sonar-quality-gate.sh` scan of the `orange` Sonar project passes
its server Quality Gate with zero new-code issues on demand.

**Primary implementation boundary**: Provider/transport integration
(verification tooling at the CI and analysis boundary).

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Local SonarQube gate | `sonar-project.properties`; `scripts/sonar-quality-gate.sh`; a green end-to-end gate run against the pre-created `orange` project | Not Started | Pending |
| T-2 | Hosted CI workflow | `.github/workflows/ci.yml` (service/web/governance jobs, pinned actions, path filters, concurrency, timeouts); AGENTS.md Scope Routing rows for both new paths | Not Started | Pending |
| T-3 | Branch protection and end-to-end verification | `dev`/`main` protection (no direct pushes; required checks); a hosted run of the new workflow on a real pull request; updated index row | Not Started | Pending |

**Affected paths**: `sonar-project.properties`; `scripts/sonar-quality-gate.sh`;
`.github/workflows/ci.yml`; `AGENTS.md`; `docs/adr/INDEX.md`

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| `scripts/sonar-quality-gate.sh` | the script's documented command contract (`SONAR_HOST_URL` + `ORANGE_SONAR_TOKEN`, scan, gate wait, new-code zero) | N/A — the documented contract is sufficient | The local SonarQube gate | Planned pre-commit state — replace with the implementing commit before completion |
| `.github/workflows/ci.yml` | the workflow's job set and pinned action SHAs | N/A — the pinned refs are the anchor | The hosted PR/push gate | Planned pre-commit state — replace with the implementing commit before completion |
| `sonar-project.properties` | `sonar.projectKey=orange` | N/A | The analysis descriptor for the pre-created server project | Planned pre-commit state — replace with the implementing commit before completion |

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**: N/A — additive gates; rollback deletes the two files and the workflow and lifts branch protection.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| CC-1 | docs/adr/ADR-0011-ci-quality-gates.md › Scope | The local gate requires SONAR_HOST_URL and ORANGE_SONAR_TOKEN, scans the orange project, waits for the gate, and requires Quality Gate OK with zero new-code unresolved issues | AC-1 | The gate script runs end-to-end green and each refusal path (missing URL, missing token) exits nonzero |
| CC-2 | docs/adr/ADR-0011-ci-quality-gates.md › Scope | Hosted ci jobs run the configured service trio, web test/build and both governance commands on PRs and pushes to dev/main, on pinned actions, read-only, with concurrency and ten-minute timeouts | AC-2 | A real hosted run URL with all three jobs green on a pull request |
| CC-3 | docs/adr/ADR-0011-ci-quality-gates.md › Scope | dev and main accept no direct pushes and require the new checks for merge | AC-3 | Branch-protection API state shows the required checks and disabled direct pushes |
| CC-4 | AGENTS.md › Scope Routing | The two new maintained configuration paths carry routing rows in the same governed change | AC-4 | Row inspection plus governance validation exit 0 |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable: overlapping same-ref workflow runs must not race; gate script must not double-scan | CI workflow / gate script | Concurrency group with cancel-in-progress; local inspection | Superseded runs cancel; no duplicated scan side effects | AC-2 | Not Started | Pending |
| timeout and deadline | Applicable: hung jobs must be bounded | CI workflow | timeout-minutes on each job; hosted run metadata | Every job bounded at ten minutes | AC-2 | Not Started | Pending |
| cancellation and interruption | Applicable: cancelled runs still dispose runner state | CI workflow | always()-style cleanup steps where output is retained | Cancellation leaves no required artifact missing | AC-2 | Not Started | Pending |
| resource bounds and backpressure | Applicable: token handling and runner disk | Gate script / CI | Token via environment/stdin only; disposable build directories | No token in argv or logs; runner disk reclaimed | AC-1; AC-2 | Not Started | Pending |
| framework or trust-boundary rejection | Applicable: fork PRs and read-only permissions | CI workflow | permissions: contents read; persist-credentials false | Workflow grants no write surface | AC-2; AC-3 | Not Started | Pending |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | The local SonarQube gate passes end-to-end and refuses missing configuration | `SONAR_HOST_URL` and `ORANGE_SONAR_TOKEN` set; `orange` project exists; scanner/curl/node installed | Run `sh scripts/sonar-quality-gate.sh`; then rerun with each variable unset | Exit 0 with Quality Gate OK and zero new-code issues; each unset-variable rerun exits nonzero with a coded message | Command transcripts | Not Started | Pending |
| AC-2 | T-2 | The hosted ci workflow passes all three jobs on a real pull request | The workflow committed and a pull request into dev open | Inspect the Actions run URL for the pull request | service, web and governance jobs all conclude success under the pinned actions | Hosted run URL and per-job conclusions | Not Started | Pending |
| AC-3 | T-3 | Branch protection enforces the gates on dev and main | Protection applied through the GitHub API | `gh api repos/hailingu/orange/branches/dev/protection` (and main) | Both branches show enforce_admins/direct-push disabled and the required status checks named | API transcripts | Not Started | Pending |
| AC-4 | T-3 | Governance accepts the change | Repository with the new files and routing rows | `npm run validate --prefix tools/governance-validator` and `npm test --prefix tools/governance-validator` | Validator exits 0; test suite passes | Command outputs | Not Started | Pending |

Allowed final check statuses are `Pass`, `Fail`, or `N/A — <specific reason>`.

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | Eligible non-author approver responds exactly Approve in this record's sole context | Approval metadata | Not Started | Pending — individual approval required |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` | Implementation Plan and Acceptance Checks rows | Not Started | Pending |
| A-3 | Reciprocal ADD link synchronized, when applicable | N/A — CI tooling creates no product-demand candidate | Architecture Source assessment | N/A — no ADD handoff | No ADD candidate selected or changed |
| A-4 | Requirement levels satisfied | Every required section is complete and every conditional trigger assessed | Structured document review | Not Started | Pending |
| A-5 | Acceptance checks are decidable | Each row names one subtask, exact input, method, binary result and evidence | Structured check review | Not Started | Pending |
| A-6 | Engineering exceptions governed, when applicable | No engineering rule is exceeded by the planned change | Engineering assessment | N/A — no exception triggered | The change adds configuration files below the size thresholds and one workflow within the existing CI pattern |
| A-7 | Contract and baseline risks covered | Every clause maps to checks and all five baseline risks are assessed | Traceability, Risk Coverage Matrix | Not Started | Pending |
| A-8 | Governance validation passed | Validator exits 0 for the changed repository | Validator output | Not Started | Pending |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Inactive future-lifecycle guidance — this Proposed record is not retired.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-29 | Initial Proposed draft after the owner chose the sequential path (close ADR-0009 and service ADR-0008 first, then draft normally); records the two-layer gate design, the TS-only Sonar scope, the split token boundary and the branch-protection scope | @zcode |
