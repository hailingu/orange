# ADR-0010: Measure real loopback HTTP serving separately from parsing

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: In Progress
- **Date**: 2026-10-22
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Project
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-28T14:32:45Z
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Approve
- **Approval Context Revision [Optional — informational and non-binding]**: 15ee45a — immutable revision whose document content equals the content present for approval (the Proposed record with approval fields still `Pending — individual approval required`); the approval edits were made after approval within the same change
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
- **Related [Optional]**: docs/reviews/2026-10-22-http-performance.md
- **Architecture Source [Conditionally Required — product demand]**: N/A — corrective performance verification, not product-demand implementation
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

The parser benchmark never executes poll or kqueue. A server regression cannot
be attributed from parser-only timing. Deliver a repeatable, real-socket
measurement that independently validates responses and separates event-loop
controls from the historical parser anchor.

### Retrospective Authorization [Required]

In the current task chat on 2026-10-22, the owner requested optimization,
a complete rerun and a rewritten report, then explicitly selected
“本轮豁免，实施并回补 ADR” for the question permitting real-socket benchmarks,
measured kqueue dispatch/write/input optimizations, and a scoped waiver of
ADR preapproval and serialization. This permits this record alongside the
unfinished runtime and CI ADRs. It does not accept this record, grant a
standing waiver, authorize remote writes, or complete the runtime/CI records.
Implementation and historical checks below ran under that exception. Keep
Proposed / Not Started until this exact record receives individual approval.
On 2026-10-23 the owner requested “按这个方向优化” after the proposed
process isolation, server-thread CPU and same-window controls. This extends
the same waived tooling slice; individual approval remains outstanding.
On 2026-10-28 the requesting user in that task chat, after the 10-22 report
analysis recommended deterministic per-request system-call and wakeup counts,
selected scope “A. 基准加确定性计数” and gate option “本轮豁免：像上次一样先实施，
再回补 Proposed ADR”. This extends T-3 with SB-6 only; no actor identity was
declared in that answer, so it is recorded as a scoped waiver, never as
`Approve`. It authorizes no production change, commit or remote write.
Later on 2026-10-28 the owner requested one measured round with a three-thread
client over the same scenarios (“增加一轮3线程的client 测试，场景和benchmark
里一样，也更新进文档里”), and in the same instruction directed overturning the
single-loop server default while keeping the old poll-era single-threaded
control untouched, which requires the driver to also select several server
acceptor loops. Both extend T-3 with SB-7 and SB-8 under the same scoped
waiver; they authorize no approval, standing waiver, or remote write.
On 2026-10-28 the repository owner @hailingu responded with the exact word
Approve in this record's sole context (a one-record approval question naming
this repository-relative path). The record is Accepted / In Progress;
fresh HEAD verification and the immutable commit anchor precede completion.

## Scope [Required]

In scope: one opt-in Release socket benchmark, eight default scenarios plus
two explicit idle-scaling cases, separate client/server processes, server-worker
CPU sampling, exact server-process system-call and context-switch counts,
optional multi-thread client shards and server acceptor threads (defaults keep
the historical single-threaded arms), child lifecycle tests, CLI validation and
reproduction guidance. Comparison evidence
includes a normalized disposable poll control, not a second production backend.

Out of scope: production optimization, open-loop capacity/SLO claims, TLS,
remote Actions execution, dependencies and build-flag changes.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | End-to-end evidence versus client overhead | Client polling, validation and scheduling can limit observed rates | Report closed-loop results, separate process CPU/RSS and worker CPU; do not infer server capacity |

### Constraints [Required]

Use Apple clang C++20, -O2 -DNDEBUG, no LTO or new dependency. Keep benchmark
instrumentation out of production. Preserve all earlier reports and approvals.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

None — independent server CPU is now measured; saturation capacity and small
wall-clock effects under shared host scheduling remain outside the claim.

## Decision Drivers [Required]

1. Independent response validation and bounded failure.
2. Identical driver and socket options across comparison controls.
3. Reproducible raw results and honest latency/resource definitions.

## Options Considered [Required]

| Option | Description | Pros | Cons |
| --- | --- | --- | --- |
| Parser timings alone | Reuse existing measurements | No new tooling | Cannot evaluate the serving loop |
| Standard-library/POSIX socket driver | One client process and a production server worker in a child | No dependency; wire validation and CPU attribution | Shared-host scheduling and boundary-sampling cost remain |
| External load framework | Add another toolchain | Broader capacity features | Dependency and deployment beyond scope |

## Decision [Required]

Select the standard-library/POSIX driver.

- **SB-1**: ORANGE_BUILD_BENCHMARKS remains OFF by default; ON in Release
  additionally builds http_server_bench without IPO and registers its CLI check.
- **SB-2**: The eight scenarios are health with 1/8/64 active connections,
  health with 64 idle plus one active, one connection with pipeline depth 16,
  1 KiB echo with 1/8 active connections, and one 1 MiB fixed-body echo.
  Explicit health_idle512/health_idle2048 cases add 512/2048 idle plus one
  active socket without changing the eight-case default. Raise only the
  disposable soft descriptor limit within its hard limit; fail if insufficient.
  Expected response bytes are constructed independently of write_response.
- **SB-3**: Invalid options, duration outside 1..10000 ms or unknown cases exit
  2. Socket, wire or deadline failures exit 1; only entirely validated runs exit
  0. Client no-progress waiting is bounded by requested duration plus 3 seconds.
- **SB-4**: Exclude setup, 20 ms warmup and shutdown. Drain issued work.
  Report completed requests/actual elapsed time and nearest-rank p50/p95/p99
  from window first-send to each response completion. Pipeline samples include
  queue position and are not divided by 16. Fork before starting the worker;
  client_pid and server_pid must differ. Report separate parent/child RUSAGE_SELF
  CPU deltas and worker THREAD_BASIC_INFO user/system CPU deltas. Sample CPU
  only at acknowledged boundaries with the worker alive, bracketing client
  issuing/draining. Report separate client/server RSS as lifetime high-water,
  including inherited pages; neither is per-connection live memory.
- **SB-5**: Bound local control I/O by 20-second timeouts. Close the channel and
  reap the child exactly once on normal completion or failure; child exceptions
  stop/join the worker. Abnormal child exit or invalid control transition fails
  explicitly; finish is idempotent.
- **SB-6**: Each case reports `server_unix_syscalls` and
  `server_context_switches`: non-negative integer deltas of the child task's
  TASK_EVENTS_INFO `syscalls_unix` and `csw`, sampled at the same acknowledged
  boundaries as SB-4 (task events read first in each sample) and subtracted
  modulo 2^32. They cover every child thread, so they include the control
  thread's constant boundary I/O; a 50 ms traffic-free window MUST report
  2..16 of each. Production gains no instrumentation. health_c1 MUST report
  at least two server Unix system calls per validated request, and
  health_pipeline16 MUST report fewer per request than health_c1. Client
  calls, Mach traps and in-kernel work per call are outside these counts.
- **SB-7**: `--client-threads N` (1..8, default 1) partitions the active
  connections into disjoint shards, each driven by its own poll loop sharing
  one deadline; per-shard latencies merge before nearest-rank percentiles and
  shard errors cross the thread boundary as strings. The child is forked
  before any client thread starts; the default single-shard path is
  behavior-identical to the historical driver and every case reports
  `client_threads`. Out-of-range counts exit 2 exactly like SB-3.
- **SB-8**: `--server-threads N` (1..8, default 1) forwards the acceptor count
  through `ServerProcess` into `LoopbackServer::serve`; with N above 1 the
  worker-CPU sample sums every child task thread except the control thread
  instead of sampling the one worker thread, and every case reports
  `server_threads`. Out-of-range counts exit 2 exactly like SB-3.

### Consequences [Required]

The driver detects invalid responses and provides actual serving evidence.
Closed-loop load, client work, shared-host scheduling and boundary cost limit
capacity inference and small-effect sensitivity. Historical 500-ms shared-process
and current 2000-ms split-process data are separate experiments. Retain all five raw runs and
state overlap rather than equating a 15% guard with proof of no regression.
SB-6 counts are exact and do not drift with host speed, so they can serve as
binary evidence for call- or wakeup-reduction work; they measure call counts,
not the cost of each call, and do not replace wall-clock or CPU results.
SB-7/SB-8 thread counts change what a run measures: a multi-shard client
spends more client CPU per request to remove the single-loop client ceiling,
so comparisons must pin both thread counts, and single-connection scenarios
(c1, pipeline16, idle) cannot benefit from either flag and serve as
invariance controls.

## Implementation Plan [Required]

**Complete task outcome**: An opt-in socket driver validates eight scenarios
and two explicit scaling cases with reproducible separate process/worker CPU
metrics, exact server system-call/context-switch counts and explicit
measurement limits.

**Primary implementation boundary**: Server performance verification tooling.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Deliver driver and executable CLI contract | service/bench/http_server_bench.cpp; verify_server.cmake; CMake target | Complete | Historical exception: missing-executable CLI red, then eight-scenario Release CLI Pass |
| T-2 | Reproduce comparison controls and retain evidence | bench README and 10-22 evidence/patches | Complete | Historical exception: five groups per control, exact patches and SHA-256 manifest retained in the 10-22 report/evidence |
| T-3 | Isolate process/worker measurement, add exact server event counts, optional client shards and server acceptor plumbing, and verify lifecycle/scaling | http_bench_server.cpp/.hpp; process tests; driver/CLI; same-window controls; SB-6 counters; SB-7/SB-8 thread flags | Complete | Owner-waived execution: missing-server-metric and missing-idle-case reds; process/CLI green; raw 10-23 evidence retained. 10-28 waived extension: missing-member compile red and missing-field CLI red, then Release 15/15 green (AC-6). 10-28 thread rounds: `--client-threads 3` CLI red (flag exited 2) then green with the eight-case threaded report (e912dfa); `--server-threads 3` CLI red then green with the aggregated worker CPU (63c43d8); Release 16/16 then 17/17; Round 4/5 raw runs retained in the 10-28 report |

**Affected paths**: service/CMakeLists.txt; service/bench/http_server_bench.cpp;
service/bench/verify_server.cmake; service/bench/README.md;
service/bench/http_bench_server.cpp; service/bench/http_bench_server.hpp; service/bench/http_bench_process_tests.cpp.
**Affected evidence files**: docs/reviews/2026-10-22-http-performance.md;
docs/reviews/2026-10-22-http-performance-evidence.json;
docs/reviews/2026-10-22-http-performance-source.patch;
docs/reviews/2026-10-22-poll-control.patch.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/bench/http_server_bench.cpp | (anonymous namespace)::exercise; ::receive_window; ::measure; ::main | N/A — symbols sufficient | Closed-loop load, wire validation and metrics | Base 08f85e9 plus 10-22 source patch and SHA-256 manifest |
| service/bench/verify_server.cmake | executable CLI contract | N/A — named script sufficient | Eight validated cases and invalid invocation handling | Same evidence manifest |
| service/CMakeLists.txt | ORANGE_BUILD_BENCHMARKS; http_server_bench; http_server_bench_cli | N/A — stable keys sufficient | Release-only opt-in registration | Same evidence manifest |

Additional current touchpoints: `service/bench/http_bench_server.cpp`,
`orange::bench::ServerProcess` and anonymous `snapshot`/`run_child`; process tests
`clean_stop`, `interrupted_child`, `invalid_transition`. Their source revision
is the 10-23 incremental patch and SHA-256 manifest. Additional evidence:
`docs/reviews/2026-10-23-kqueue-lifecycle-evidence.json`,
`docs/reviews/2026-10-23-kqueue-lifecycle.patch`,
`docs/reviews/2026-10-23-poll-control.patch`,
`docs/reviews/2026-10-23-batch-send-control.patch`.

SB-6 touchpoints, source revision `865fa4e` plus the 10-28 uncommitted
worktree change: `orange::bench::ServerMetrics::unix_syscalls` and
`::context_switches`; anonymous `snapshot` (TASK_EVENTS_INFO read first),
`event_delta` and `run_child` End delta; JSON keys `server_unix_syscalls` and
`server_context_switches` in `(anonymous namespace)::measure`; the SB-6 block
of `verify_server.cmake`; and the bounded idle window in `clean_stop`.
SB-7 touchpoints, source revision `e912dfa`: `(anonymous namespace)::drive`
and `::exercise` (shard partition, shared deadline, latency merge),
`::thread_count`, the `client_threads` JSON key in `::measure`, and the SB-7
block of `verify_server.cmake`. SB-8 touchpoints, source revision `63c43d8`:
`orange::bench::ServerProcess::ServerProcess(int)`, anonymous `Worker`,
`server_threads_cpu` and the two-branch `snapshot`, the `server_threads` JSON
key in `::measure`, and the SB-8 block of `verify_server.cmake`.

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**:
N/A — additive opt-in tooling; remove the target, CLI registration and new files
to discard it without changing the production library.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — new files are below review thresholds, new executable units below 60 code
lines, no dependencies or intrinsics. Cyclomatic complexity: N/A — no configured
tool; review executable span and nesting instead. The ADR gate exception is
recorded under Retrospective Authorization, not an engineering exception.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| SB-1 | This record, Decision | Release-only opt-in target and CLI | AC-1 | Configure OFF/ON and existing Debug rejection |
| SB-2 | This record, Decision | Default8 plus explicit idle512/2048; soft-only descriptor budget; independent wires | AC-1; AC-2; AC-5 | CLI fixtures/wire mutation; deterministic prepare_descriptor_budget inspection verifies only rlim_cur changes within rlim_max |
| SB-3 | This record, Decision | Exit codes and bounded failure | AC-1; AC-2 | CLI invalid matrix; wire mutation exits 1 |
| SB-4 | This record, Decision | Distinct PIDs, separate process/worker CPU and timing semantics | AC-3; AC-4; AC-5 | Actual Mach/RUSAGE sampling, CLI schema and same-window raw runs |
| SB-5 | This record, Decision | Bounded control I/O, explicit failure and exactly-once reaping | AC-4 | Clean/repeated finish, own-child SIGKILL and invalid begin; waitpid ECHILD |
| SB-6 | This record, Decision | Integer child-task syscall/csw deltas; idle window 2..16; health_c1 >= 2 calls/request; pipeline16 below health_c1 | AC-6 | clean_stop asserts the idle bounds; verify_server.cmake asserts integer fields for all eight cases, the health_c1 floor and the cross-multiplied pipeline comparison |
| SB-7 | This record, Decision | --client-threads 1..8 shards the active connections with one shared deadline and merged latencies; default identical; client_threads reported; out-of-range exits 2 | AC-7 | verify_server.cmake runs the eight scenarios with three client threads and checks the reported count and zero errors, then the invalid-count matrix |
| SB-8 | This record, Decision | --server-threads 1..8 selects the child's acceptor count; worker CPU sums every non-control task thread when above 1; server_threads reported; out-of-range exits 2 | AC-8 | verify_server.cmake runs the eight scenarios with three client and three server threads and checks the reported count and zero errors, then the invalid-count matrix |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Multiple clients and depth-16 pipeline | Socket driver | Eight-scenario CLI | Every response matches independent wire | AC-1 | Pass | Historical Release CLI |
| timeout and deadline | Socket stalls or loop fails | Client multiplexer | Inspect steady-clock end+3s, 100ms poll and configure_channel 20-second receive/send timeouts; CLI timeout45s | Failure exits 1; CTest cannot hang indefinitely | AC-1; AC-2; AC-4 | Pass | Bounded control flow and executable checks |
| cancellation and interruption | Own child killed or invalid begin transition | Benchmark process ownership | Real interruption/protocol tests; waitpid after finish | Failure explicit, child reaped; repeated clean finish0 | AC-4 | Pass | http_bench_process_tests; 10-23 evidence |
| resource bounds and backpressure | 1 MiB responses and idle sockets; 32-bit kernel event tallies | Benchmark client/server | Execute eight bounded scenarios; inspect event_delta modular subtraction against the 10-second duration cap | No wire error or timeout; separate metrics and explicit idle512/2048 cases; event deltas exact across one wrap | AC-1; AC-3; AC-5; AC-6 | Pass | Raw reports and scaling CLI; 10-28 Release CLI |
| framework or trust-boundary rejection | Invalid CLI or mismatched server bytes | Public benchmark CLI | Invalid matrix and mutation | Exit 2 for invalid CLI; 1 for wire mismatch | AC-1; AC-2 | Pass | CLI and mutation evidence |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | CLI contract passes | Release driver; eight scenarios | ctest -R ^http_server_bench_cli$; invalid-duration/case matrix | Exit 0 for CLI suite; eight nonempty zero-error scenarios; invalid invocations exit 2 | CTest and JSON | Pass | Historical exception: Release CLI passes |
| AC-2 | T-1 | Wire validation detects corruption | Disposable driver with one expected health-body byte changed | Compile same driver, run --quick --case health_c1 | Exit 1 and response wire mismatch | Captured diagnostic | Pass | Historical exception: deliberate health-wire mutation exits 1 with response wire mismatch; complete output in evidence JSON |
| AC-3 | T-2 | Raw comparison is reproducible | Same driver, flags and normalized socket options | Five alternating groups for poll control/before/after; inspect patch and hashes | Five complete eight-case reports per variant, zero errors, source fingerprints and all p50/p95/p99/CPU/RSS fields retained | Evidence JSON and two source patches | Pass | Historical exception: five complete groups per control; zero errors; all eight before/after median throughput changes above -15%; raw data and source fingerprints retained |
| AC-4 | T-3 | Process/CPU and lifecycle contract passes | Release process/CLI suites | run the process and CLI benchmark suites (ctest -R alternation of http_bench_process_tests and http_server_bench_cli); inspect configure_channel timeouts=20s | Distinct positive PIDs; positive server process/worker CPU and separate RSS; clean finish0; killed/invalid child fails and is reaped; repeated finish0 | CTest and red diagnostics | Pass | 10-23 Release suites; prior executable fails missing server_process_cpu_seconds |
| AC-6 | T-3 | Server event counts satisfy SB-6 | Release benchmark build; Release+ASan/UBSan build | run the process and CLI benchmark suites (ctest -R alternation of http_bench_process_tests and http_server_bench_cli) in both builds; process tests repeated 30 times | Both suites exit 0 in both builds; 30/30 process-test repeats exit 0 | CTest output and recorded per-request counts | Pass | 10-28 waived execution: reds were 4 missing-member compile errors and `server event count missing or not an integer: server_unix_syscalls`; green Release 15/15, 30/30 repeats, Release+ASan/UBSan 2/2. Informative 500 ms run: health_c1 3.000, health_c64 2.369, pipeline16 0.188, echo_1m_c1 67.045 server Unix calls/request; health_c1 1.039 context switches/request |
| AC-5 | T-3 | Same-window controls complete | Identical driver; Release -O2 without LTO | Five alternating 2000-ms groups of poll/before/after; default8 plus idle512/2048; inspect prepare_descriptor_budget soft-only assignment | Exactly five samples per variant/case; soft descriptor budget stays <= unchanged hard limit; all errors0; all CPU/RSS/tail fields and hashes retained | 10-23 evidence JSON/patches | Pass | Measurement plan, raw samples and fingerprints retained |
| AC-7 | T-3 | Threaded client keeps the case contract | Release driver; `--quick --client-threads 3`; invalid counts 0, -1, 9, nan, 2.5 | ctest -R ^http_server_bench_cli$ (SB-7 block) | Exit 0 with eight zero-error cases reporting client_threads 3; every invalid count exits 2 | CTest output | Pass | Waived execution: red at HEAD (`--client-threads 3` exited 2: invalid invocation), green at e912dfa with Release 16/16; Round 4 five-group raw runs retained in the 10-28 report |
| AC-8 | T-3 | Acceptor-threaded child keeps the case contract | Release driver; `--quick --client-threads 3 --server-threads 3`; invalid counts 0, -1, 9, nan, 2.5 | ctest -R ^http_server_bench_cli$ (SB-8 block) | Exit 0 with eight zero-error cases reporting server_threads 3; every invalid count exits 2 | CTest output | Pass | Waived execution: red at e912dfa (`--server-threads 3` exited 2), green at 63c43d8/cd6fcf7 with Release 17/17 twice; Round 5 five-group raw runs retained in the 10-28 report |

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | Eligible non-author approver responds exactly Approve in this record's sole context | Approval metadata | Complete | Reapproved-as-initial-approval 2026-10-28: Approver @hailingu (non-author; author @codex), Approval Time 2026-10-28T14:32:45Z, Approval Evidence: Approve, in this record's sole context; informational Approval Context Revision commit 15ee45a exactly represents the approved document content |
| A-2 | Complete task delivered | Every applicable acceptance check passes and every subtask has actual evidence after acceptance | Checks and evidence JSON | Not Started | Historical exception execution only; formal completion follows individual approval |
| A-3 | Reciprocal ADD link | N/A — corrective performance maintenance | Architecture Source assessment | N/A — no product-demand handoff | No ADD candidate changed |
| A-4 | Requirement levels | Every required item and conditional assessment is complete | Structured review | Complete | Reviewed at drafting; approval fields remain pending in Proposed |
| A-5 | Decidable checks | Every check has input, binary expected result and deterministic method | Acceptance table | Complete | Checks below identify executable contracts and exact outcomes |
| A-6 | Engineering exceptions | Triggered rules have rationale, owner and controls | Engineering assessment | Complete | Assessment included above |
| A-7 | Contracts and risks | Every clause maps to checks; all five risks assessed | Tables above | Complete | Historical exception evidence is distinguished from approval |
| A-8 | Governance validation | Validator and its suite exit 0 | Validation output | Complete | Historical exception and current 10-23 verification: governance suite303/303; repository validator exits0; complete logs retained in 10-23 lifecycle evidence JSON; 10-28 SB-6 extension: governance suite 303/303 and repository validator pass |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Inactive future-lifecycle guidance — this Proposed record is not retired.
When triggered, archive in this ADR root, update markers and the central
index, and preserve reciprocal supersession paths when applicable.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-28 | Approval: repository owner @hailingu responded with the exact word Approve in this record's sole context (one-record approval question naming this path). Decision Status Proposed→Accepted; Implementation Status Not Started→In Progress; Approver, Approval Time and Approval Evidence recorded; Approval Context Revision recorded informationally at commit 15ee45a. Fresh HEAD verification and the immutable commit anchor precede completion | @zcode |
| 2026-10-28 | Extend waived T-3 with SB-7 client shards and SB-8 server acceptor plumbing, AC-7/AC-8 and the Round 4/5 measured rerun recorded in the 10-28 report; approval remains outstanding | @zcode |
| 2026-10-28 | Extend waived T-3 with SB-6 exact server system-call/context-switch counts and AC-6; approval remains outstanding | @copilot |
| 2026-10-23 | Extend waived tooling with split processes, worker CPU, child lifecycle and same-duration scaling controls; approval remains outstanding | @codex |
| 2026-10-22 | Backfill the owner-waived performance task with test-first evidence, explicit measurement boundaries and individual approval still outstanding | @codex |
