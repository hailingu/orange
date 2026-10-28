# ADR-0008: Use stable kqueue connection identities and avoid redundant request-path work

## Metadata [Required]

- **Decision Status**: Proposed
- **Implementation Status**: Not Started
- **Date**: 2026-10-22
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
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
- **Related [Optional]**: docs/reviews/2026-10-22-http-performance.md
- **Architecture Source [Conditionally Required — product demand]**: N/A — corrective performance maintenance, not product-demand implementation
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

A response generated on a read-only readiness event previously waited for a
later write event, even when a nonblocking send could succeed immediately.
Received bytes were first copied into a connection string and each pipeline
message erased its prefix. Each returned event also scanned the connection
vector by descriptor. Remove these costs at the transport-delivery boundary
while retaining owned decoder results, event identity lifetime and write
readiness recovery.

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
The owner subsequently requested removing per-event linear lookup and another
measured rerun. This is the explicitly waived kqueue dispatch scope; it extends
this same Proposed transport slice, not a standing or individual acceptance.
On 2026-10-23 the owner requested “按这个方向优化” after dirty-list, ordered
idle expiry and deferred-collection proposals. This extends the same waived
transport lifecycle boundary; individual acceptance remains outstanding.
Later on 2026-10-23 the owner reviewed the short-circuit analysis of the same
report, selected the interest-transition guard, tail-touch splice skip and
single-pass head-policy classification, and explicitly rejected the static-wire,
clock-batching, receive-buffer, sendmsg and storage-recycling candidates
(“S3、S4，S5 是有价值的，其他的反对”), requesting one measured rerun. This
extends the same waived transport slice once more, including the bounded
decoder-contract addition of a derived close flag; individual acceptance
remains outstanding.

## Scope [Required]

In scope: direct synchronous feeding of recv windows, pipeline view advancement,
immediate nonblocking send after read, stable kqueue udata dispatch, batch-safe
connection ownership, dirty interest work, activity-ordered idle expiry,
deferred reclamation, interest-transition gating before dirty queueing,
identity tail-splice skipping, single-pass head policy classification carrying
the Connection: close option on ParsedMessage, and regression tests. The
decoder contract gains one derived boolean; there is no new public serving
option and no grammar decision.

Out of scope: EV_CLEAR, production threads, body pooling, TLS and remote CI.
Fresh successful-I/O timestamps and event_capacity64 remain. Batch-send
scheduling is evaluated in a disposable control; retain it only with consistent
paired throughput/CPU benefit and no tail tradeoff.

## Tensions, Constraints, And Open Questions [Required]

### Identified Tensions [Conditionally Required — competing goals or trade-offs exist]

| ID | Tension | Impact | Decision |
| --- | --- | --- | --- |
| TN-1 | Immediate delivery versus blocked peer | A send may hit EAGAIN | Keep output offsets and EVFILT_WRITE recovery; no blocking socket or busy retry |
| TN-2 | Borrowing recv bytes versus result lifetime | Stack storage expires next recv | Feed and process pipeline suffixes synchronously; the existing decoder owns incomplete data and returned values |
| TN-3 | Direct event identity versus container movement and fd reuse | A dangling or recycled identity can misroute a batch event | Individually owned stable Connections; defer destruction until both event passes finish; reject invalid/mismatched descriptors |

### Constraints [Required]

Preserve all HTTP bytes, error precedence, sticky failures, 8 KiB head / 1 MiB
body / 8 KiB aggregate metadata limits, response order, idle timeout and
close-after-flush. Keep Apple clang C++20, single-threaded kqueue, current flags
and dependencies. Reset pooling is excluded because completed bodies move out.

### Open Questions [Conditionally Required — material questions exist or were resolved during drafting]

None — direct feed relies on the inspected RequestDecoder::Impl::feed invariant
that an incomplete successful feed consumes its whole fragment. Complete feeds
leave only pipeline suffixes, handled before recv storage expires; errors close
and discard the suffix. Existing decoder API and ownership remain unchanged.

## Decision Drivers [Required]

1. Remove per-event scans, demonstrated staging allocation and an unnecessary readiness wait.
2. Preserve real-socket framing, ordering and slow-reader recovery.
3. Retain raw comparisons instead of attributing parser noise to kqueue.

## Options Considered [Required]

| Option | Description | Pros | Cons |
| --- | --- | --- | --- |
| Keep staging and deferred writes | Existing path | No change | Extra copy, allocation and readiness boundary |
| Direct feed and immediate nonblocking send | Reuse incremental decoder and output offset | Removes demonstrated costs | Must verify lifetime and blocked-output recovery |
| Stable udata versus fd side table | Direct kernel-returned Connection pointer or indexed descriptor mapping | Stable ownership supports O(1) dispatch without a second index | Must defer closed-object destruction through the returned batch |
| Dirty queue and ordered owning list | Queue changed connections; splice successful I/O to tail; retire at batch end | Removes four live-connection management scans | Added node/hooks and activity-order invariant |
| Edge triggering or shared round timestamps | Change readiness/deadline precision | Possible further gains | No measured need in this slice |
| Short-circuit gating versus unconditional bookkeeping | Queue interest work only on a real transition, skip identity tail splices, resolve close in the framing pass | Removes provably dead per-request work | The guard must share the emitter's exact predicate or a needed transition could be missed |

## Decision [Required]

Select direct feed, immediate nonblocking send and stable udata identities.
A std::list<Connection> owns stable nonmovable Connections with inline decoders.
The active list is activity-ordered; a second list owns retired nodes until
batch end. Intrusive dirty hooks avoid separate queue allocations; splice
preserves all node addresses. Each filter add/enable/disable carries that same Connection pointer.
The listener keeps null udata and its explicit descriptor branch. Apple
[kqueue(2)](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/kevent.2.html)
specifies unchanged udata and automatic removal of descriptor events on close.

- **KS-1**: After a read queues output, attempt nonblocking send before the
  next kevent wait. Retain unsent bytes/offset and arm EVFILT_WRITE if blocked.
- **KS-2**: Feed recv's at-most-16-KiB window directly; consume every pipeline
  suffix by advancing a string_view in the same call. Do not allocate a
  separate connection input-staging string or shift each consumed prefix.
- **KS-3**: Preserve wire bytes and response order, rejection closure, peer
  progress while another output is blocked, idle timeout and clean shutdown.
  Read-pause accounting uses undelivered output; unparsed staging no longer
  exists. Decoder-local incomplete state remains bounded by existing caps.
- **KS-4**: Retain only with KS-1/KS-2 regression checks green, complete routed
  and sanitizer suites green, and no greater-than-15% median throughput loss
  across the eight before/after scenarios. This guard alone is not a claim of
  a statistically resolved latency gain; disclose every paired sample and tail.
- **KS-5**: Every connection filter transition carries its stable Connection
  pointer in udata. Read, write and registration-error dispatch use only that
  pointer and the local descriptor guard, with no scan over other connections.
- **KS-6**: Keep Connection addresses stable during acceptance, activity
  reordering and retirement. Retain closed objects until both event passes end; ignore an old
  event after close even if accept reuses its descriptor. Release closed owners
  at the batch boundary after close has removed their kernel filters.
- **KS-7**: Queue changed live nodes at most once; process only dirty nodes for
  interests. Order active nodes by fresh monotonically sampled successful-I/O
  time, splicing activity to the tail without allocation; timestamp new accepts
  freshly. Read the nearest deadline from the head; retire only consecutive due
  heads, including the exact deadline, never early. Retirement closes/unlinks
  immediately and is idempotent. Destroy only retired nodes after both event
  passes. These four management operations must not traverse live connections.
- **KS-8**: Queue a serviced live connection for interest work only when the
  wanted kernel filter set differs from the tracked set. One shared predicate
  (wanted_interest / interest_transition_pending) feeds both that guard and
  the changelist emitter, so a skipped queue entry is provably a no-op and a
  needed transition is never skipped.
- **KS-9**: Skip the activity-tail splice when the touched node is already
  newest; the identity relink never changes order and its
  splice(end, list, prev(end)) form hits the standard's position == ++i
  undefined case on every single-connection touch.
- **KS-10**: Resolve the request's Connection: close option (RFC 9110 7.6.1)
  in the same single validated-field pass that resolves body framing, and
  carry it on ParsedMessage.close; the event loop performs no second header
  scan at response time.


### Consequences [Required]

One receive-staging copy disappears; its first-use allocation is absent in
the fixed-body probe. The old staging capacity was reused on keep-alive and
usually paid for before warmed timing, so no per-request allocation saving
is inferred from that probe. No prefix memmove remains. Most small writable responses avoid a write-interest round. Blocked
sends still use the output queue and write filter. Decoder result allocations
remain. Management work scales with changed, due or closed nodes; this does not
bound accept work, byte work or response generation by ready-event count alone.
List links and dirty/position hooks add metadata and affect cache locality;
process high-water RSS does not quantify live per-connection memory cost.
On this arm64 libc++ build, sizeof(Connection) rises from 80 to 112 bytes; list
links add implementation overhead while the old vector owner slots disappear.
This is an explicit metadata tradeoff, not a claim of lower memory use.
The first benchmark measures the combined input/write change. The additional
udata experiment isolates this follow-up from that frozen first-round source;
it includes the necessary stable-storage change, not just pointer lookup.
The short-circuit round adds no allocation calls; the decoder's one Impl
allocation requests 8 more bytes per decoder fixture for the ParsedMessage
close flag (padding-aligned bool), and steady-state keep-alive exchanges no
longer queue, unlink and rebuild no-op interest transitions. Its measured
throughput and worker-CPU changes are within run noise on every scenario
(disclosed, not claimed as gains); the removals are retained because each
eliminated step is provably dead work, and KS-9 additionally removes a
standards-undefined splice edge.

## Implementation Plan [Required]

**Complete task outcome**: The existing kqueue transport handles identical
requests without per-event lookup or full live-connection management scans,
receive staging or a mandatory extra wait before sending.

**Primary implementation boundary**: HTTP socket transport delivery.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Deliver direct input and immediate-send path | http_server.cpp/.hpp and focused scheduling/input tests | Complete | Historical exception: KS-1 wire Pass/wait Fail becomes Pass/Pass; KS-2 staging-band allocations 1 becomes 0 with body Pass |
| T-2 | Verify lifecycle and measured retention | Extended server tests; full suites; report/evidence | Complete | Historical exception: Debug 10/10, Release 12/12, sanitizer 10/10; 48 socket checks; all eight median throughput ratios >= 0.85 |
| T-3 | Deliver event-proportional management and batch-safe direct dispatch | http_server.cpp; ConnectionStore; lifecycle/dispatch tests; same-driver controls | Complete | Historical exception: wire Pass / stable_udata Fail becomes Pass / Pass with 161 registrations and same-batch hints; Debug 11/11, Release 13/13, ASan/UBSan 11/11; five 2000-ms pairs, 18,457,885 responses, zero errors; all eight median ratios >= 0.85; udata evidence JSON and patch retained. 10-23: dirty/order mutants rejected; lifecycle tests green; split-process scaling controls and full checks retained in 10-23 evidence |
| T-4 | Deliver interest gating, tail-touch skip and single-pass head policy | http_server.cpp; http_connection_store.cpp; http_body_framing.cpp/.hpp; http_request_decoder.cpp/.hpp; decoder/store tests | Complete | Historical exception: decoder suite compile-red on missing ParsedMessage.close, then green with option-list/case/OWS/duplicate/fragment/reset pins; store tail-touch order pin; Debug 12/12, Release 15/15, ASan/UBSan 12/12; parser guard 18/18; equivalence 68,241,183 bytes byte-identical; allocation calls unchanged with +8 requested bytes per decoder fixture; five alternating 2000-ms groups over ten scenarios, 18,628,104 responses, zero errors; short-circuit evidence JSON and patch retained |

**Affected paths**: service/src/http_server.cpp; service/src/http_server.hpp;
service/src/http_connection_store.cpp; service/src/http_body_framing.cpp;
service/src/http_body_framing.hpp; service/src/http_request_decoder.cpp;
service/src/http_request_decoder.hpp;
service/tests/http_server_tests.cpp; service/tests/http_server_scheduling_tests.cpp;
service/tests/http_server_input_tests.cpp; service/tests/http_server_dispatch_tests.cpp;
service/tests/http_connection_store_tests.cpp;
service/tests/http_request_decoder_tests.cpp;
service/CMakeLists.txt.
**Affected evidence files**: docs/reviews/2026-10-22-http-performance.md;
docs/reviews/2026-10-22-http-performance-evidence.json;
docs/reviews/2026-10-22-http-performance-source.patch;
docs/reviews/2026-10-22-kqueue-udata.patch;
docs/reviews/2026-10-22-kqueue-udata-evidence.json;
docs/reviews/2026-10-23-head-policy-interest-shortcircuit.patch;
docs/reviews/2026-10-23-head-policy-interest-shortcircuit-evidence.json.

### Stable Implementation Touchpoints [Conditionally Required — source or configuration implementation]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/src/http_server.cpp | orange::http::(anonymous namespace)::drive_messages; ::read_connection; ::service_connection | N/A — symbols sufficient | Direct input, first-send scheduling and dirty/retirement transitions | 08f85e9 plus first-round, udata and 10-23 incremental patches; SHA-256 manifests |
| service/src/http_server.cpp | orange::http::(anonymous namespace)::build_interest_changes | N/A — symbol sufficient | Output-only pause accounting; write filter for remaining bytes | Same manifest |
| service/tests/http_server_scheduling_tests.cpp | ::kevent; ::send; ::main | N/A — symbols sufficient | Observes real I/O without a production test seam | Same manifest |
| service/tests/http_server_input_tests.cpp | ::operator new; ::main | N/A — symbols sufficient | Counts ordinary allocations in declared staging band during real body delivery | Same manifest |
| service/tests/http_server_tests.cpp | (anonymous namespace)::check_large_pipeline; ::check_slow_reader | N/A — symbols sufficient | Cross-window ordering and blocked-output recovery | Same manifest |
| service/src/http_server.cpp | orange::http::detail::Connection; orange::http::detail::ConnectionStore; orange::http::(anonymous namespace)::event_connection; ::set_interest; ::accept_incoming; orange::http::LoopbackServer::serve | N/A — symbols sufficient | Stable list nodes, unchanged udata, direct dispatch and retired-only batch-end collection; accepted descriptors owned before allocation | 10-23 incremental patch and manifest |
| service/tests/http_server_dispatch_tests.cpp | ::kevent; (anonymous namespace)::exercise; ::observe_changes; ::observe_events | N/A — symbols sufficient | Real registrations and wires through growth, alternating collection, descriptor reuse and same-batch write hints | Same incremental manifest |

Current source touchpoints: `service/src/http_connection_store.cpp` and
`service/src/http_connection_store.hpp`,
`orange::http::detail::ConnectionStore::{add,mark_dirty,take_dirty,touch,retire,
next_deadline,expire,collect}`; `service/tests/http_connection_store_tests.cpp`,
anonymous `dirty_and_retired`/`ordered_deadlines`. Source revision: 10-23
incremental patch and SHA-256 manifest. Additional affected paths include these
three files; additional evidence is `docs/reviews/2026-10-23-kqueue-lifecycle.patch`
and `docs/reviews/2026-10-23-kqueue-lifecycle-evidence.json`.
Short-circuit round touchpoints:
`orange::http::(anonymous namespace)::wanted_interest`,
`::interest_transition_pending` and `::set_interest` in
`service/src/http_server.cpp`; `orange::http::detail::ConnectionStore::touch`;
`orange::http::detail::head_policy` in `service/src/http_body_framing.cpp`;
`orange::http::ParsedMessage::close` in `service/src/http_request_decoder.hpp`.
Source revision: 10-23 head-policy/interest short-circuit patch, SHA-256
`e6e95ac2b0f31ea5924728482ee335c690be815399c74fe7ec6cbeeb14e3c3b6`.

**Migration and rollback strategy [Conditionally Required — this replaces or changes existing behavior]**:
No data migration. Revert only this record's production patch to restore staging
and deferred writes if any wire, lifecycle or retention check fails. Revert
only the incremental udata patch to restore first-round dispatch/storage while
keeping its input/write optimizations if this follow-up fails; retain the
benchmark and report as evidence. Revert only the 10-23 production/CMake delta
to restore stable vector ownership and four management scans if its lifecycle
or retention checks fail. Revert only the short-circuit patch to restore
unconditional dirty queueing, identity tail splices and the response-time
requests_close scan if the close-option, lifecycle or retention checks fail.
No deployment or artifact promotion occurs.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — no cap is exceeded. The event-loop orchestration remains above the
60-code-line review threshold and below 80 after extracting the shared
connection-local I/O/failure action. Retain its cohesive readiness-batch,
listener, idle and descriptor-collection owner; further extraction would split
one loop lifecycle. Current serve span is 62 non-comment physical lines, below
the 80-line cap. New helpers are below 60 code lines. The input-test main is 64 code lines;
its one real-socket fixture keeps setup, worker lifetime and the joined result
together below the 80-line cap, with both red and green executions. File spans
are below their thresholds. Cyclomatic complexity: N/A — no configured tool;
span and nesting are reviewed instead.

## Contract-To-Check Traceability [Conditionally Required — source or configuration implementation]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| KS-1 | This record, Decision | Attempt send before next readiness wait; recover blocked writes | AC-1; AC-3 | Real syscall observation; slow-reader response recovery |
| KS-2 | This record, Decision | No input staging; process pipeline suffix before stack expiry | AC-2; AC-3 | Allocation bound, 1 MiB fragmented body and 512-request cross-window pipeline |
| KS-3 | This record, Decision | Wire/lifecycle/resource invariants unchanged | AC-3 | Full real-socket suites, decoder checks and sanitizers |
| KS-4 | This record, Decision | Retention guard and honest performance evidence | AC-4; AC-6 | Five identical-driver before/after groups, all eight scenarios in each experiment |
| KS-5 | This record, Decision | Stable udata on every connection filter; no per-event scan | AC-5 | Real registration identity observation and deterministic inspection of event_connection and all three dispatch sites |
| KS-6 | This record, Decision | Stable addresses and safe close/reuse through the returned batch | AC-5 | 161 real accepts, retained early connection, alternating closes, reused descriptors and currently writable same-batch hints; ASan/UBSan |
| KS-7 | This record, Decision | Deduplicated dirty work, activity order, exact expiry and retired-only collection | AC-7; AC-8 | Real-owner unit cases, ordering/unlink mutants, fd-reuse/backpressure suites and scaling controls |
| KS-8 | This record, Decision | Interest work queued only on a real wanted-versus-tracked transition, via one shared predicate | AC-9; AC-10 | Deterministic inspection that the guard and emitter share the predicate; full real-socket suites exercising backpressure, slow-reader and close transitions that must still re-arm filters |
| KS-9 | This record, Decision | Identity tail splice skipped; no position == ++i splice | AC-9; AC-10 | Store tail-touch order pin; ASan/UBSan suites |
| KS-10 | This record, Decision | Close option resolved in the single framing pass on ParsedMessage.close; no response-time header scan | AC-9; AC-11 | Decoder close-option suite (list, case, OWS, duplicates, fragments, reset); real-socket Connection: close lifecycle; byte-identical equivalence stream |

## Risk Coverage Matrix [Conditionally Required — source or configuration implementation]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Pipeline crosses multiple recv windows while other clients progress | Socket transport | 512-pipeline, slow-reader and dispatch-lifetime tests | Every response exact and ordered; active peer progresses; old batch events cannot route to replacement connections | AC-3; AC-5; AC-7; AC-8 | Pass | Historical suites plus current list reordering/retirement and sanitized dispatch; short-circuit round keeps the same suites and wire-exact bench green |
| timeout and deadline | Touched oldest node; exact expiry; stalled output | ConnectionStore/socket lifecycle | Synthetic monotonic deadline unit cases and real slow-reader/idle tests | Touched node moves behind older peer; exact-boundary expiry without early close; active peer progresses | AC-3; AC-7; AC-8 | Pass | 10-23 unit, mutation and full socket evidence; tail-touch skip keeps the pinned deadline order in AC-9 |
| cancellation and interruption | Allocation failure, peer close, stop flag | Connection-local I/O | Existing decoder fault injection and shutdown/EOF tests; sanitizer suite | Coded failure or connection-local close; no dangling recv views; clean stop | AC-2; AC-3; AC-7; AC-8 | Pass | Current lifecycle owner, real socket and ASan/UBSan evidence, rerun for the short-circuit round |
| resource bounds and backpressure | 1 MiB fragmented input, output becomes blocked | Decoder and output queue | Allocation probe and three queued large responses | Zero staging-band allocations; body intact; blocked bytes eventually delivered | AC-2; AC-3; AC-8 | Pass | KS-2 and current slow-reader/scaling checks; AC-10 shows allocation call counts unchanged |
| framework or trust-boundary rejection | Malformed/over-cap input followed by pipeline suffix | Actual parser and sockets | Existing 400/413/431 rejection lifecycle checks | Exact rejection, EOF, no follow-up response; reconnect succeeds | AC-3; AC-8 | Pass | Current real-socket rejection checks; close-option rejection lifecycle unchanged under the fused single pass (AC-11) |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Immediate-send contract | One real request with observed kevent/send boundaries | ctest -R ^http_server_scheduling_tests$ | Exit 0; wire=Pass and first_send_before_next_wait=Pass | Red/green diagnostic | Pass | Red delivered exact wire but waited; green avoids wait |
| AC-2 | T-1 | No separate input staging allocation | 1 MiB fixed body, <=16 KiB client writes, server-thread ordinary-new counting | ctest -R ^http_server_input_tests$ | Exit 0; body=Pass; allocations of 8193..65536 requested bytes equal 0 | Red/green count | Pass | Red 1 allocation/body Pass; green 0/body Pass |
| AC-3 | T-2 | Full behavior and sanitizer checks pass | Routed Debug trio, opt-in Release tree and ASan/UBSan tree | Full CTest in each, ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 | Debug 11/11, Release 13/13, sanitizer 11/11; no warnings or sanitizer diagnostics | Complete logs | Pass | Historical exception: extended suites 11/11, 13/13, 11/11; complete current logs in udata evidence, original first-round counts retained separately |
| AC-4 | T-2 | Retention guard holds | Same-driver five alternating groups against pre-task source, -O2 without LTO | Compare eight median requests_per_second values | No median throughput reduction exceeds 15%; zero socket/wire/deadline errors | Raw five groups and summary | Pass | Historical exception: five complete groups per control; zero errors; all eight before/after median throughput changes above -15%; raw data and source fingerprints retained |
| AC-5 | T-3 | Constant-time identity and safe batch lifetime | Real kqueue/socket fixture; 96 retained clients, alternating closes and 64 replacements | http_server_dispatch_tests in Debug/Release/ASan; inspect event_connection and filter transitions | Exit 0; wire=Pass; stable_udata=Pass; registrations=161; same_batch_write_hints>0; all connection transitions pass their stable owner address; all dispatch sites use local pointer/fd checks only | Red/green output, sanitized suite and source inspection | Pass | Historical exception: red has wire Pass / stable_udata Fail; green has both Pass, 161 registrations and 258 same-batch hints; sanitizer test passes |
| AC-6 | T-3 | Isolated dispatch retention guard | Frozen first-round source versus udata source; identical driver/options/flags | Five alternating before/after groups; 2000 ms per scenario; compare eight medians | Zero errors; no median throughput decrease greater than 15%; all raw tails, CPU and RSS retained; gains only claimed with five positive paired differences and disclosed ranges | Additional evidence JSON and incremental patch | Pass | Historical exception: five complete 2000-ms pairs; 18,457,885 validated responses, zero errors; smallest median change -0.9%, largest +12.6%; mixed paired signs disclosed rather than claimed as repeatable gain |
| AC-7 | T-3 | Dirty/expiry/retirement behavior passes | Real ConnectionStore, descriptors and synthetic monotonic time | http_connection_store_tests; separately remove touch splice and retirement dirty unlink in disposable source | Production passes all checks; each mutant exits1 naming broken order/dirty invariant; no early expiry or loss of live nodes | Test/mutation diagnostics | Pass | 10-23 evidence includes actual owner suite and both mutation logs |
| AC-8 | T-3 | Full suites and measured retention pass | Same-window split-process controls | Routed Debug, Release, ASan/UBSan; five alternating 2000-ms groups over default8 and idle512/2048 | All applicable tests exit0; all measurements errors0; legacy8 after/before median rps ratio >=0.85; retain every tail/CPU field and pair sign | 10-23 evidence/report | Pass | Full verification and raw controls retained; gain claim distinguished from retention |
| AC-9 | T-4 | Close option and tail-touch behavior pass | Decoder close-option fixtures; store tail-order pin; routed suites | http_request_decoder_tests; http_connection_store_tests; full Debug/Release/ASan/UBSan CTest | Decoder suite compiles only with the new member and exits0; every option-list/case/OWS/duplicate/fragment/reset pin passes; store order preserved; all suites exit0 | Red/green and suite logs | Pass | Compile red on missing close member, then green; 12/12, 15/15, 12/12 in the three trees |
| AC-10 | T-4 | Wire semantics and lifecycle unchanged | Byte-identical drivers; five alternating groups over ten scenarios; parser equivalence and allocations | http_server_bench --duration-ms 2000; http_parser_equivalence; http_parser_allocations | Zero socket/wire/deadline errors; every median rps ratio >= 0.85; equivalence stream byte-identical with the recorded SHA-256; allocation call counts unchanged | Raw runs, hashes and allocation report | Pass | 18,628,104 responses, zero errors; smallest median change -2.2% with mixed pairs disclosed; 68,241,183-byte stream identical; calls unchanged, +8 requested bytes per decoder fixture disclosed |
| AC-11 | T-4 | Real-socket close lifecycle still honored | Connection: close request over the event loop | http_server_tests close case | Response carries Connection: close and the connection then EOFs | Suite log | Pass | Existing real-socket close check passes unchanged against the fused flag |

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | Eligible non-author approver responds exactly Approve in this record's sole context | Approval metadata | Not Started | Pending — individual approval required |
| A-2 | Complete task delivered | Every applicable acceptance check passes and every subtask has actual evidence after acceptance | Checks and evidence JSON | Not Started | Historical exception execution only; formal completion follows individual approval |
| A-3 | Reciprocal ADD link | N/A — corrective performance maintenance | Architecture Source assessment | N/A — no product-demand handoff | No ADD candidate changed |
| A-4 | Requirement levels | Every required item and conditional assessment is complete | Structured review | Complete | Reviewed at drafting; approval fields remain pending in Proposed |
| A-5 | Decidable checks | Every check has input, binary expected result and deterministic method | Acceptance table | Complete | Checks below identify executable contracts and exact outcomes |
| A-6 | Engineering exceptions | Triggered rules have rationale, owner and controls | Engineering assessment | Complete | Assessment included above |
| A-7 | Contracts and risks | Every clause maps to checks; all five risks assessed | Tables above | Complete | Historical exception evidence is distinguished from approval |
| A-8 | Governance validation | Validator and its suite exit 0 | Validation output | Complete | Historical exception and current 10-23 verification: governance suite303/303; repository validator exits0; complete logs retained in 10-23 lifecycle evidence JSON |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

Inactive future-lifecycle guidance — this Proposed record is not retired.
When triggered, archive in this ADR root, update markers and the central
index, and preserve reciprocal supersession paths when applicable.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-23 | Extend the waived transport slice with the owner-selected short circuits KS-8/KS-9/KS-10, subtask T-4 and AC-9..AC-11; measured round retained on structural grounds with disclosed noise-level results; approval pending | @k3 |
| 2026-10-23 | Extend waived lifecycle scope to dirty work, ordered expiry and retired-only reclamation; keep fresh clocks/capacity64; approval pending | @codex |
| 2026-10-22 | Backfill the owner-waived performance task, then extend the same transport slice for the owner-requested stable-udata dispatch rerun; individual approval still outstanding | @codex |
