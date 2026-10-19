# Follow-Up Assessment And Confirmed Repairs

## Review Basis And Authorization

Reviewed the owner's attachment `5fd214c6-2248-4fb8-8076-8ef049764454`,
“再次评估（2026-10-16 修复后）”, against HEAD
`1f876464862c69c5a54424d91fbfd6937983e23c` plus the existing uncommitted
repairs. This report records observations, direct repair authorization and
verification; it does not grant decision-record acceptance.

The owner explicitly authorized: “本次明确豁免 ADR 前置审批与串行限制，直接修复已证实问题”,
then requested fixes for this follow-up assessment. This same-task exception
covers the confirmed changes below. No approval identity, exact `Approve`,
Accepted status or runtime acceptance was inferred from it. Existing unrelated
work was preserved; nothing was committed, pushed or deployed.

## Independent Findings

| Peer claim | Independent evidence and judgment | Disposition |
| --- | --- | --- |
| Full tests leak 192 fixtures | Confirmed: the pre-change suite passed 290/290 under a fresh private TMPDIR and left exactly 192 `orange-governance-*` directories. The global cumulative count of 4,655 was not independently measured | Fixed shared lifecycle ownership, every caller and the fixed outside-path fixture |
| Ordinary Markdown record references can dangle | Confirmed before implementation: backtick, plain and Markdown-link references to a nonexistent concrete record returned exit 0; regression checks expected exit 1 | Added active reference validation with `RECORD_REFERENCE_INVALID` through the existing canonical, repository-contained regular-file resolver |
| README's 208 count is stale | Wording was ambiguous: it described the adoption baseline, while the actual pre-change suite had 290 tests | Made 208 explicitly historical; current totals come from the runner |
| The new metadata budget is absent from ADR-0003 | Confirmed: the public constant and error existed, but Scope, acceptance and resource-risk clauses still described only head/content budgets | Synchronized Scope, T-1, stable touchpoints, required Request Resource Budgets, CC-5/CC-8, the risk matrix and AC-1; preserved historical evidence |
| One MiB in 512 B chunks fails | Confirmed, but this is a consequence of independent content/framing budgets rather than a parser defect. It concerns HTTP chunk encoding, not socket/read fragmentation | Added explicit joint-budget contract and characterization tests; retained the 8 KiB bound |
| Missing JS formatter/linter proves a quality defect | Tooling absence is factual; 100-column formatting is not the repository's JavaScript contract, and no behavior defect follows from that metric alone | No new dependency, formatting policy or repository-wide reformat added |
| ADR-0003 remains Proposed / Not Started | True: its revised runtime decision has not received a new approval, and network/TLS/browser work is still absent | Kept truthful lifecycle and unexecuted socket acceptance. ADR-0007 separately records actual exception execution without inventing its formal approval |

The peer's revised grade is an opinion using selected weights, including code
line weighting; it is not a reproducible project-wide quality measurement.
The peer's benchmark, differential oracle and 400,000-case fuzz programs were
not supplied, so their timings and outcomes were not independently certified.

## Implemented Behavior And Regression Evidence

`test/fixtures.mjs::temporaryDirectory(t, prefix)` registers `t.after` for the
exact returned directory immediately after allocation, before population.
`validRepository(t)` uses it and populates immutable fixture data. Context-free
allocation is rejected before creating a directory. All existing callers now
pass their context, including multiple roots in one test. Cleanup errors are
not caught or swallowed; no global registry or historical-directory sweep was
introduced. The symlink rejection case owns a unique outside directory.

The real child-runner tests cover success, two roots, an already removed root,
deliberate body failure, outside-path ownership and concurrent owners. The
deliberate failure carries `ORANGE_EXPECTED_FIXTURE_FAILURE` and exits 1;
successful probes exit 0. Reported roots disappear after exit, while parent
directories and sibling sentinels survive. Explicit readiness/release messages
keep one child's root alive while the other finishes. Parent tests have a
30-second deadline, child tests 25 seconds, and parent teardown terminates an
unfinished child. Baseline success/multiple/failure/concurrent probes actually
failed because roots remained; the final probes pass.

`lib/record-reference-validation.mjs::validateRecordReferences` checks concrete
ADD/ADR/OCR identities in active prose, inline code and inline links. Explicit
repository identities use repository-root resolution; other inline links use
the document directory. External URLs, comments, code fences, authoring
templates and filename placeholders stay inert. Active records' untriggered
Archival sections contain future migration instructions; retired records and
ordinary documents do not receive that exemption. Valid links and active
missing links have opposite CLI outcomes. This does not validate arbitrary
hyperlinks or heading fragments.

HTTP characterization uses the actual `RequestDecoder` through
`service/tests/http_request_decoder_tests.cpp::check_joint_chunk_budgets`.
For a 1,048,576-byte body without extensions/trailers, 1,024-byte HTTP chunks
spend 7,173 metadata bytes and pass; 512-byte chunks would spend 14,341 and
fail with `chunk_metadata_too_long`. Feeds of 17 bytes, 8,192 bytes and the
whole wire input agree. There is no universal minimum chunk size: content
length, chunk count, extensions and trailers jointly determine the budget.
No C++ production behavior changed in this follow-up.

## Structured Risk And Scope Review

| Dimension | Concrete boundary and verification | Result |
| --- | --- | --- |
| Concurrency and ordering | Real child processes coordinate readiness/release; one cleanup leaves the other live root intact | Pass |
| Timeout and deadline | Lifecycle probes run under bounded real node:test deadlines and terminate unfinished children on teardown | Pass; no new production wait/timeout introduced |
| Cancellation and interruption | A real test-body failure still executes teardown and exits with its expected failure marker | Pass; forced OS/process termination before hooks remains outside the cleanup guarantee |
| Resource bounds and backpressure | Full suite in a private TMPDIR leaves only an unchanged sentinel; HTTP exact-cap and joint-budget cases run through the actual decoder | Pass |
| Framework or trust-boundary rejection | Actual node:test teardown, CLI missing-link rejection, the retained symlink escape case, and the shared canonical containment resolver | Pass |

FX-1 through FX-5 map to the executed checks in
`docs/adr/ADR-0007-governance-fixture-lifecycle.md`. Runtime CC-8 maps to the
named parser checks; CC-5's actual HTTP response/EOF/reconnection acceptance
remains Not Started because no server is implemented. No fake socket result
is substituted for parser evidence.

The fixture migration preserves original validation assertions and dependency
files. Required/conditional document content, reciprocal links and unchanged
index statuses were reviewed. The owner exception is recorded separately from
normal approval metadata. Historical results remain historical.

The CLI is 765 physical lines, above the 600-line review threshold and below
the 800-line cap. This change adds only the reference-validator import and
dispatch; the new 58-line leaf owns that separate policy. The CLI's existing
traversal/lifecycle/diagnostic responsibility remains cohesive. The fixture
factory has five code lines and the new allocation helper six. New lifecycle
and reference-test files are below 100 physical lines each; new functions are
below 60 code lines and executable nesting at most four levels.

The context-only migration retains the existing 1,163/1,211-line lifecycle and
structure cohorts, below the 1,800-line cap. Four inherited callbacks exceed
the 60-line review threshold at 61, 64, 77 and 78 noncomment lines; they remain
below 80. Three mainly embed complete OCR grammar for their single lifecycle
case; the 77-line callback retains its existing grouped trailing-content
validation scenario. Assertions/data are unchanged, so broader test splitting
is deferred rather than coupled to resource ownership. Cyclomatic complexity:
N/A — no configured complexity tool; spans and manual nesting review used.

## Verification And Retained Evidence

Exact commands, summaries, source Git-blob IDs and point-in-time measurements
are retained in
[`2026-10-16-peer-assessment-followup-evidence.json`](2026-10-16-peer-assessment-followup-evidence.json).

- Baseline: 290/290 tests, 192 leftover fixture directories.
- Final governance suite: 303/303; private TMPDIR contains only its unchanged
  sentinel, with zero leftover fixture directories.
- Focused lifecycle/reference checks: 13/13; missing-reference and cleanup
  regressions were observed failing before their repairs. The inactive archival
  control also failed first, then passed after the false-positive fix.
- `npm run validate --prefix tools/governance-validator`: exit 0.
- CMake/CTest with Apple clang 21.0.0: 2/2, zero build warnings. ASan/UBSan:
  2/2. New HTTP characterization adds 12 passing checks.
- `node --check` for affected JavaScript, clang-format check mode and
  `git diff --check`: exit 0.

CMake used a task-owned temporary directory instead of `service/build` to
preserve existing user output. All task-owned fixtures and compiler output
were removed after retaining command evidence. No preexisting shared temporary
directories were removed. No new performance benchmark, fuzz campaign,
listener, TLS or browser verification was run in this follow-up.
