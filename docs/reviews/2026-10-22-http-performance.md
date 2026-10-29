# HTTP Parser And Loopback Serving Performance — 2026-10-22

**Metric direction**: **↑ higher is better**; **↓ lower is better**, when the
workload and measurement scope are comparable. Throughput (req/s) is ↑;
latency percentiles, processing time (µs/ns), CPU time per request, allocation
calls and requested allocation bytes are ↓. For percentage changes, a positive
throughput change is better, while a negative time/cost change is better.
Arrows on ranges describe the measured values, not the width of the range.
Peak RSS is ↓ only with comparable process scope and fixture state; the
process-lifetime high-water marks here do not establish server memory savings.
These directions do not establish statistical significance.

## Latest Follow-Up: Single-Pass Head Policy And Interest Short-Circuits — 2026-10-23

The owner selected three short-circuit candidates from this report's analysis
and rejected the static-wire, clock-batching, receive-buffer, sendmsg and
storage-recycling candidates. First, a serviced connection is queued for
interest work only when the wanted kernel set provably differs from the
tracked state; one shared predicate drives both that guard and the changelist
emitter, so a steady-state exchange makes no dirty-queue round trip and the
skipped entries are exactly the no-ops set_interest would have dropped.
Second, ConnectionStore::touch skips the activity-tail splice when the node
is already newest — the identity relink the single-connection steady state
performed on every touch, which is also the splice(end, list, prev(end))
position == ++i case the standard leaves undefined. Third, head policy is
resolved in one validated-field pass: detail::head_policy returns body framing
together with the request's Connection: close option (RFC 9110 7.6.1), carried
on ParsedMessage.close; the event loop's separate requests_close header scan
is deleted. sizeof(ParsedMessage) grows by one padding-aligned bool: the
decoder's single Impl allocation requests 8 more bytes per decoder fixture,
with allocation call counts and semantic checksums unchanged. This is a
metadata tradeoff of the same class as the 10-23 Connection growth, not a
memory-saving claim.

### Measurement And Controls

The split-process harness is unchanged: the client in the parent, the
uninstrumented production server on a worker thread in the child. **Before** is
the 10-23 lifecycle-round source snapshot taken before this round's edits;
**After** is the worktree with byte-identical benchmark drivers. Five groups
alternate before/after and its reverse; each variant runs the default eight
cases plus explicit health_idle512 and health_idle2048 at 2000-ms issuing
intervals. clang-format whitespace was applied after timing; the rebuilt After
binary passes every suite and compiles the same statements. No compiler,
sanitizer suite or other benchmark ran during formal timing.

### Real-Socket Results

| Scenario | Before req/s ↑ | After req/s ↑ | After/before throughput change ↑ |
| --- | ---: | ---: | ---: |
| health_c1 | 32,968 | 33,120 | +0.5% |
| health_c8 | 98,295 | 97,260 | -1.1% |
| health_c64 | 101,698 | 99,470 | -2.2% |
| health_idle64 | 32,871 | 32,992 | +0.4% |
| health_pipeline16 | 468,642 | 464,887 | -0.8% |
| echo_1k_c1 | 32,251 | 32,080 | -0.5% |
| echo_1k_c8 | 96,200 | 95,880 | -0.3% |
| echo_1m_c1 | 4,351 | 4,383 | +0.7% |
| health_idle512 | 32,290 | 32,660 | +1.1% |
| health_idle2048 | 31,723 | 31,687 | -0.1% |

| Scenario | Before server worker CPU µs/request ↓ | After worker CPU µs/request ↓ | CPU cost change ↓ | After server process CPU µs/request ↓ | After client CPU µs/request ↓ |
| --- | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 9.88 | 9.86 | -0.2% | 9.86 | 13.69 |
| health_c8 | 8.19 | 8.21 | +0.2% | 8.21 | 9.75 |
| health_c64 | 7.66 | 7.70 | +0.5% | 7.70 | 9.68 |
| health_idle64 | 10.03 | 9.90 | -1.3% | 9.90 | 13.72 |
| health_pipeline16 | 0.80 | 0.79 | -0.7% | 0.79 | 0.88 |
| echo_1k_c1 | 10.33 | 10.23 | -0.9% | 10.23 | 13.92 |
| echo_1k_c8 | 8.40 | 8.39 | -0.1% | 8.39 | 9.92 |
| echo_1m_c1 | 155.13 | 153.85 | -0.8% | 153.85 | 143.98 |
| health_idle512 | 9.97 | 9.90 | -0.6% | 9.90 | 14.01 |
| health_idle2048 | 9.88 | 9.82 | -0.7% | 9.82 | 14.97 |

| Scenario | Before p99 µs ↓ | After p99 µs ↓ | After p50 µs ↓ | After p95 µs ↓ |
| --- | ---: | ---: | ---: | ---: |
| health_c1 | 53.29 | 51.42 | 27.04 | 33.67 |
| health_c8 | 97.62 | 99.96 | 56.67 | 76.25 |
| health_c64 | 653.75 | 648.38 | 314.08 | 489.58 |
| health_idle64 | 56.00 | 46.92 | 27.08 | 34.25 |
| health_pipeline16 | 55.04 | 56.42 | 30.54 | 39.12 |
| echo_1k_c1 | 55.58 | 48.42 | 28.00 | 34.58 |
| echo_1k_c8 | 102.62 | 96.79 | 57.71 | 78.08 |
| echo_1m_c1 | 376.25 | 361.83 | 209.96 | 268.58 |
| health_idle512 | 56.17 | 48.92 | 27.08 | 33.79 |
| health_idle2048 | 56.92 | 50.00 | 27.58 | 34.33 |

| Scenario | Before req/s range ↑ | After req/s range ↑ | Five paired throughput changes ↑ |
| --- | ---: | ---: | --- |
| health_c1 | 32,610–34,315 | 32,605–33,888 | -2.0%, +2.3%, +0.4%, +0.5%, -0.0% |
| health_c8 | 94,167–100,598 | 94,840–99,149 | -1.4%, -1.7%, -0.9%, -0.1%, +0.7% |
| health_c64 | 97,476–104,202 | 96,818–102,784 | -1.4%, -0.8%, -2.5%, -4.8%, +2.0% |
| health_idle64 | 32,560–34,521 | 32,536–33,802 | -2.1%, +1.8%, -1.1%, -0.3%, +0.4% |
| health_pipeline16 | 461,511–491,507 | 463,168–484,702 | -1.4%, +3.0%, -1.9%, -0.9%, +0.7% |
| echo_1k_c1 | 31,661–32,389 | 31,602–33,276 | +1.2%, +4.9%, -2.3%, -1.7%, +1.3% |
| echo_1k_c8 | 92,018–98,650 | 93,366–98,821 | +0.2%, -0.3%, -1.3%, -1.6%, +1.5% |
| echo_1m_c1 | 4,260–4,362 | 4,319–4,633 | +8.7%, +2.2%, +0.7%, -0.4%, -1.0% |
| health_idle512 | 31,949–33,526 | 32,288–34,062 | +5.5%, -1.0%, +0.7%, -1.1%, +1.1% |
| health_idle2048 | 31,510–31,883 | 30,861–31,764 | +0.4%, -0.6%, +0.6%, -0.0%, -2.7% |

No scenario meets the predeclared five-positive-pairs rule; every median
throughput change lies within ±2.2% and every worker-CPU change within ±1.3%,
with overlapping ranges throughout. The three removals are retained on
structural grounds — a provably no-op queue round trip, an identity splice
that was also undefined-behavior-adjacent, and one header pass instead of two —
not as a measured speedup, matching this report's prediction that constant-factor
shavings fall inside the ±9% run noise on active cases. The health_c64 -2.2%
median has one positive pair in five and overlapping ranges; it is disclosed,
not resolved. Tail medians improve in the idle and echo cases (idle64 p99
56.00→46.92 µs, echo_1k_c1 55.58→48.42 µs) and rise 2.4% on health_c8; the
mixed tails are reported without a claim. Across the ten-case five-group
experiment **18,628,104 measured responses** validated with zero
socket/wire/deadline errors.

### Full Verification And Reproduction

Test-first evidence: the decoder suite first failed to compile on the missing
ParsedMessage.close member, then passed after head_policy landed, pinning comma
lists, case folding, OWS, duplicate fields, fragmented feeds and per-message
reset independence; the store suite pins that touching the newest node keeps
deadline order. Routed Debug **12/12**, fresh opt-in Release **15/15**,
ASan/UBSan **12/12** pass (leak detection disabled as required on this Apple
setup). The parser magnitude guard passes **18/18**; the equivalence streams
stay **68,241,183 byte-identical bytes**, SHA-256
`6a27c498cb9910a9d961ecc4795c9b045de4ee8314a23a8f8dff6dfb4eb2dbac`; allocation
call counts and checksums are unchanged with the disclosed +8 requested bytes
per decoder fixture. Governance **303/303**, repository validation,
clang-format and diff whitespace checks pass.

Reproduction: rebuild the 10-23 lifecycle After tree (archive `08f85e9` plus
the first-round, udata and lifecycle patches above) for this Before; apply the
[short-circuit patch](2026-10-23-head-policy-interest-shortcircuit.patch) for
After. The patch's SHA-256 is
`e6e95ac2b0f31ea5924728482ee335c690be815399c74fe7ec6cbeeb14e3c3b6`; applied to a
fresh Before copy it reproduces every src/tests file hash of the worktree.
Build both with the same Release recipe and run `http_server_bench
--duration-ms 2000`, then each explicit idle case, in five forward/reverse
groups. The [short-circuit evidence JSON](2026-10-23-head-policy-interest-shortcircuit-evidence.json)
retains the fixed plan, source and binary fingerprints, all 100 raw case
samples, the compile-red log, allocation deltas and complete check results.
Disposable builds and reconstruction trees were removed. The two backfilled
ADRs remain **Proposed / Not Started**, awaiting individual approval. No
commit, push, deployment or remote CI run occurred.

## Follow-Up: Event-Driven Connection Management — 2026-10-23

The owner requested the next measured optimization under the existing scoped
waiver. The transport now drains a deduplicated dirty queue for interest changes,
reads the nearest idle deadline from an activity-ordered owning list, expires
only due heads and destroys only retired nodes at batch end. Successful I/O
moves its stable node to the activity tail with O(1) splice. This removes four
full live-connection scans while preserving udata identity through both event
passes, descriptor-reuse rejection, exact idle deadlines and slow-reader recovery.
Management cost follows changed/accepted/expired/closed work; request bytes and
response generation retain their own costs. Production remains single-threaded.

Fresh successful-I/O timestamps and the 64-event harvest capacity remain.
Sharing a round timestamp would alter deadline precision; raising the event
capacity has not been justified by this experiment. Batch-send scheduling was
measured separately below. On this arm64 libc++ build sizeof(Connection) grows
from **80 to 112 bytes**, plus list-node links, while old vector owner storage
disappears. This is a metadata/locality tradeoff, not a memory-saving claim.

### Measurement And Controls

The client now runs in the parent process and the uninstrumented production
server on a worker thread in a child, forked before any worker starts. Boundary
samples report separate client/child RUSAGE_SELF CPU and worker
THREAD_BASIC_INFO user+system CPU. The acknowledged server interval brackets
the client interval and includes small control-boundary work. No clock or CPU
sampling was added per request. Distinct PIDs are checked by the executable CLI
contract; real interruption/invalid-transition tests check explicit failure,
worker stop/join and exactly-once child reaping.

All three variants use byte-identical driver/helper sources, TCP_NODELAY,
-O2 -DNDEBUG without LTO, 20-ms warmup and **2000-ms** issuing intervals. Five
groups alternate poll/before/after and its reverse in one window. The original
eight cases run by default; explicit idle512/2048 cases add one active socket
and the named idle count. Setup and shutdown are excluded; issued work drains.
No compiler, sanitizer suite or other benchmark ran during formal timing.
The plan was fixed before timing. Each cell is a median of five per-run values,
not a pooled latency percentile; CPU/request divides each run's CPU by its
validated completions before taking the median.

**Before** is the frozen udata implementation, with new measurement tooling
only. **Poll** starts from that same source and normalizes only the event
mechanism, retaining its immediate send/direct input/parser/serializer/socket
options. Poll versus Before isolates that backend control; After also includes
connection management changes. Compare only within this section. Earlier
shared-process CPU and different-duration results below have different scopes.

A first attempt was excluded after build-fingerprint review found a compiler
cache reset and a stale After driver binary. Its raw results and reason remain
in the evidence audit. The tables below use the replacement run from a fresh
Release directory, with Release/benchmark-ON cache, production flags, all driver
hashes and binary hashes checked before timing. No excluded sample is pooled.

### Real-Socket Results

| Scenario | Poll req/s ↑ | Before kqueue req/s ↑ | After kqueue req/s ↑ | After/before throughput change ↑ |
| --- | ---: | ---: | ---: | ---: |
| health_c1 | 36,523 | 36,738 | 37,139 | +1.1% |
| health_c8 | 116,572 | 120,442 | 120,239 | -0.2% |
| health_c64 | 123,296 | 127,647 | 126,183 | -1.1% |
| health_idle64 | 28,593 | 36,614 | 37,238 | +1.7% |
| health_pipeline16 | 495,311 | 523,901 | 517,579 | -1.2% |
| echo_1k_c1 | 35,369 | 35,515 | 35,754 | +0.7% |
| echo_1k_c8 | 116,611 | 117,713 | 119,061 | +1.1% |
| echo_1m_c1 | 4,461 | 4,506 | 4,501 | -0.1% |
| health_idle512 | 10,136 | 35,850 | 36,581 | +2.0% |
| health_idle2048 | 2,625 | 31,708 | 35,240 | +11.1% |

| Scenario | Before server worker CPU µs/request ↓ | After worker CPU µs/request ↓ | CPU cost change ↓ | After server process CPU µs/request ↓ | After client CPU µs/request ↓ |
| --- | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 9.55 | 9.50 | -0.6% | 9.50 | 11.87 |
| health_c8 | 7.47 | 7.38 | -1.2% | 7.38 | 7.89 |
| health_c64 | 7.00 | 6.92 | -1.1% | 6.92 | 7.57 |
| health_idle64 | 9.81 | 9.54 | -2.8% | 9.54 | 11.86 |
| health_pipeline16 | 0.78 | 0.78 | +0.3% | 0.78 | 0.75 |
| echo_1k_c1 | 10.01 | 9.99 | -0.2% | 9.99 | 12.08 |
| echo_1k_c8 | 7.60 | 7.53 | -0.9% | 7.53 | 8.01 |
| echo_1m_c1 | 149.44 | 148.87 | -0.4% | 148.88 | 141.88 |
| health_idle512 | 11.91 | 9.59 | -19.5% | 9.59 | 12.25 |
| health_idle2048 | 21.34 | 9.52 | -55.4% | 9.52 | 13.29 |

| Scenario | Before p99 µs ↓ | After p99 µs ↓ | After p50 µs ↓ | After p95 µs ↓ |
| --- | ---: | ---: | ---: | ---: |
| health_c1 | 37.58 | 34.79 | 24.42 | 29.00 |
| health_c8 | 86.54 | 84.17 | 45.62 | 63.50 |
| health_c64 | 550.67 | 558.88 | 239.54 | 405.50 |
| health_idle64 | 42.04 | 38.21 | 24.42 | 29.67 |
| health_pipeline16 | 47.42 | 45.96 | 28.25 | 34.75 |
| echo_1k_c1 | 42.33 | 39.17 | 25.21 | 30.54 |
| echo_1k_c8 | 82.17 | 83.12 | 46.25 | 64.92 |
| echo_1m_c1 | 334.75 | 353.46 | 208.58 | 230.67 |
| health_idle512 | 42.38 | 37.46 | 24.50 | 29.42 |
| health_idle2048 | 44.92 | 42.00 | 25.17 | 30.17 |

| Scenario | Before req/s range ↑ | After req/s range ↑ | Five paired throughput changes ↑ |
| --- | ---: | ---: | --- |
| health_c1 | 35,695–37,485 | 36,472–37,392 | +0.3%, +1.1%, +4.8%, +1.5%, -2.7% |
| health_c8 | 112,311–122,956 | 119,073–120,778 | -0.1%, -1.8%, +5.2%, +6.2%, -2.1% |
| health_c64 | 119,210–131,695 | 123,776–128,877 | -0.9%, -4.2%, -3.0%, +8.1%, -1.0% |
| health_idle64 | 35,872–37,629 | 37,082–38,012 | -0.2%, +1.0%, +1.5%, +3.8%, +2.9% |
| health_pipeline16 | 491,621–529,658 | 505,260–519,163 | -2.5%, -2.3%, -3.6%, +5.6%, +1.4% |
| echo_1k_c1 | 34,954–36,110 | 35,225–36,067 | -0.0%, -0.1%, -0.8%, +2.3%, +1.5% |
| echo_1k_c8 | 108,638–120,747 | 118,645–119,473 | +0.6%, -1.1%, +0.8%, +9.9%, +0.9% |
| echo_1m_c1 | 4,353–4,524 | 4,353–4,539 | +0.4%, +0.3%, -3.4%, +2.2%, +3.2% |
| health_idle512 | 33,394–36,695 | 35,940–36,680 | -0.2%, +1.7%, +2.0%, +9.8%, +2.2% |
| health_idle2048 | 30,884–31,942 | 34,557–35,861 | +12.3%, +11.2%, +10.8%, +12.6%, +11.9% |

**health_idle512**: throughput +2.0%, server-worker CPU/request -19.5%; 4/5 throughput pairs improve.
**health_idle2048**: throughput +11.1%, server-worker CPU/request -55.4%; 5/5 throughput pairs improve.
**health_c64**: throughput -1.1%, server-worker CPU/request -1.1%; 1/5 throughput pairs improve.

With most of 64 connections active, changed-node work approaches the active
connection count. Dirty hooks and activity splices add constant work too; the
source therefore predicts less benefit than in sparse-active cases. This is an
explanation for the limited gain, not a measured attribution of the -1.1% loss.
The health_c64 p99 median is 1.5% higher and echo_1m_c1 p99 is 5.6% higher;
these tail regressions remain visible despite their slightly lower CPU cost.

Five positive pairs with disclosed ranges are the predeclared consistency rule,
not a significance test or confidence interval. Sparse-active cases expose the
scan cost; a small active set can remain limited by socket round trips and
client scheduling even when server CPU/request falls. The ordinary eight-case
changes are disclosed above, including regressions. All pass the 15% retention
guard; that is not proof of no smaller regression. The health_c64 result does
not establish the cause of the earlier apparent 10% loss, or a universal
poll/kqueue ranking. This experiment isolates the combined management change,
not the individual cost of each removed scan.

Across the 15 ten-case reports, **30,927,371 measured responses** validated
with zero socket/wire/deadline errors. Separate client/server peak RSS and all
process CPU samples are retained in JSON. RSS is lifetime high-water and the child inherits allocator pages/history;
optional cases run in separate invocations. These values cannot establish live
per-connection memory savings. Shared-host closed-loop load does not establish
server capacity, an SLO, or long-duration stability.

### Read-Batch Send Experiment

Five separate alternating health_c64 pairs compare immediate send with sending
after all returned read events, before the write pass. The production loop
retains immediate send; a control changing only that scheduling point is saved.

| Scheduling | req/s ↑ | Server worker CPU µs/request ↓ | p99 µs ↓ |
| --- | ---: | ---: | ---: |
| Immediate | 130,147 | 6.83 | 528.50 |
| After read batch | 127,853 | 6.91 | 562.38 |

Five batch/immediate throughput changes ↑: -3.2%, -1.2%, -0.8%, -1.5%, +0.9%. All **2,577,437** responses validated. Mixed pair directions/tail tradeoffs do not justify changing scheduling from
this focused experiment; no general batch-send gain is claimed.

### Full Verification And Reproduction

Test-first evidence includes the previous executable failing the missing
server_process_cpu_seconds contract, explicit idle512 failing before that case
existed, and the missing lifecycle-owner API compile red. The real-owner tests
cover dirty dedup/requeue, queued retirement, descriptor closure, stable retired
identity, exact expiry, activity reordering and live-peer survival. Separate
disposable mutations removing activity splice or retirement dirty unlink each
exit1 with the expected behavioral diagnostic. Tests do not assert prose or
source layout.

Routed Debug **12/12**, fresh opt-in Release **15/15**, ASan/UBSan **12/12** pass;
leak detection remains disabled as required by this Apple sanitizer setup.
Before control passes 14/14, batch control 15/15; poll passes 13/13 applicable
checks. Its kqueue-registration-only test is inapplicable (wire Pass, zero
kqueue registrations), so it is excluded only from that disposable control.
The parser guard passes **18/18**; complete allocation reports are unchanged;
semantic streams remain **68,241,183 byte-identical bytes**, SHA-256
`6a27c498cb9910a9d961ecc4795c9b045de4ee8314a23a8f8dff6dfb4eb2dbac`.
Five fresh parser pairs, all 18 values and ranges are retained; parser source
was not changed, so small timing shifts are not attributed to this transport
optimization. Governance **303/303**, repository validation, clang-format and
diff whitespace checks pass. The structured review covers lifecycle ownership,
all five risks, contract/check traceability, size thresholds, unchanged status
metadata and the explicit owner-waiver boundary.

### Parser Full Rerun

The 18 unchanged parser fixtures were rerun in five alternating pairs with
fresh Release binaries. Values below are medians of per-run median ns/request;
parentheses span the five run medians. Negative time change is better. These
unchanged-source shifts are reported without claiming a transport-caused parser
gain or a resolved noise bound. All raw batch ranges remain in JSON.

| Fixture | Before ns/request, range ↓ | After ns/request, range ↓ | Time change ↓ |
| --- | ---: | ---: | ---: |
| head_long_values_16 | 309.47 (300.10–319.85) | 312.15 (302.14–314.75) | +0.9% |
| head_short_fields_1000 | 7,807.90 (7,760.70–7,914.19) | 7,813.80 (7,744.91–7,828.49) | +0.1% |
| head_ows_7800 | 401.46 (396.91–403.84) | 397.95 (396.88–402.33) | -0.9% |
| head_ows_8_fields | 443.46 (430.24–453.20) | 441.29 (437.66–444.39) | -0.5% |
| head_one_short_field | 52.63 (51.74–53.34) | 52.83 (50.03–53.37) | +0.4% |
| head_minimal | 20.01 (19.63–20.18) | 19.69 (19.59–20.07) | -1.6% |
| head_realistic_434 | 184.39 (183.15–185.87) | 181.91 (177.84–186.67) | -1.3% |
| head_full_8192 | 5,110.60 (5,087.36–5,195.35) | 5,141.32 (4,991.29–5,283.77) | +0.6% |
| head_decoder_whole | 221.85 (217.87–236.42) | 218.46 (215.47–226.43) | -1.5% |
| head_decoder_feed17 | 758.48 (727.95–764.18) | 771.44 (753.39–788.37) | +1.7% |
| echo_chunked_1024 | 175.28 (171.63–176.71) | 171.90 (169.82–176.41) | -1.9% |
| body_1m_chunk1024 | 42,126.62 (41,773.11–42,876.95) | 42,003.58 (41,848.95–42,555.01) | -0.3% |
| body_1m_chunk16384 | 27,746.75 (27,148.27–27,797.52) | 28,107.26 (27,310.38–29,571.94) | +1.3% |
| body_1m_fixed_feed16384 | 16,200.36 (16,107.50–16,710.45) | 16,097.25 (15,941.57–16,390.79) | -0.6% |
| extension_7000 | 1,877.65 (1,866.91–1,892.52) | 1,873.18 (1,862.95–1,886.03) | -0.2% |
| trailer_7000 | 1,863.43 (1,811.44–1,937.33) | 1,855.51 (1,853.00–1,868.94) | -0.4% |
| head_cr_dense | 150.49 (149.49–151.39) | 149.03 (148.48–150.60) | -1.0% |
| respond_echo_chunked_1024 | 51.49 (51.26–51.66) | 51.77 (51.38–52.23) | +0.5% |

### Reproduction Artifacts

Archive `08f85e9`, apply the two earlier first-round/udata patches to obtain the
frozen transport. For After apply the [lifecycle/tooling incremental patch](2026-10-23-kqueue-lifecycle.patch).
For Before apply only the [benchmark control patch](2026-10-23-benchmark-control.patch);
for Poll then apply the [normalized poll patch](2026-10-23-poll-control.patch).
For the scheduling variant apply [batch-send control](2026-10-23-batch-send-control.patch)
to After. All four trees were independently reconstructed from the Git base and
all compiled-source hashes match. Use fresh build directories with the existing
Release recipe, verify cache/production flags before timing, then run
`http_server_bench --duration-ms 2000`, followed by each explicit idle case, in
five forward/reverse groups. The scheduling run selects `--case health_c64`.

The [10-23 evidence JSON](2026-10-23-kqueue-lifecycle-evidence.json) retains the
predeclared plan, binary/source/patch fingerprints, all raw runs, excluded-run
audit, exact commands, complete verification logs and calculation definitions.
Disposable builds, reconstruction trees and binary streams were removed;
preexisting routed build configuration remains. The same two ADRs are backfilled
and remain **Proposed / Not Started**, awaiting individual approval. No commit,
push, deployment or remote CI run occurred.

## Earlier Follow-Up: Direct Kqueue Connection Identity

The owner requested replacing per-event linear descriptor lookup, then another
measured rerun. This is within the previously waived kqueue dispatch scope;
the same Proposed service ADR-0008 now records its third transport subtask.
Neither that request nor the retained test evidence accepts an ADR.

**Implemented**: every connection filter add/enable/disable carries its
Connection pointer in `udata`. Read, write and registration-error events use
that pointer plus a local descriptor check. There is no scan over other
connections in event dispatch. A vector of unique owners keeps pointees stable
through growth and compaction; the nonmovable decoder is stored inside each
Connection. Closed objects remain alive until both event passes finish, so a
same-batch old event cannot use a freed object or target a newly reused fd.
The accepted fd is owned before allocating its Connection, with connection-local
allocation failure cleanup. Interest, deadline, idle and collection scans
remain O(N). This is not a claim that the entire loop is O(ready events).

**Controls**: Before is the first round's After source, freshly rebuilt and
remeasured; After adds only stable storage/direct dispatch and its test/build
registration. The socket driver is byte-identical, with unchanged eight
scenarios, warmup, socket options and Release flags. Five alternating pairs
use **2000 ms** issuing intervals rather than the earlier 500 ms. No compiler,
sanitizer suite or other benchmark ran during timing. Compare within this
new pair of columns; do not splice numbers from the earlier duration.

| Scenario | Before req/s ↑ | Udata req/s ↑ | Udata vs before (throughput) ↑ | Before p99 µs ↓ | Udata p99 µs ↓ |
| --- | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 35,733 | 35,952 | +0.6% | 50.58 | 50.38 |
| health_c8 | 102,921 | 107,669 | +4.6% | 103.04 | 103.46 |
| health_c64 | 106,587 | 119,966 | +12.6% | 677.33 | 651.62 |
| health_idle64 | 35,081 | 35,688 | +1.7% | 53.08 | 50.71 |
| health_pipeline16 | 493,261 | 488,999 | -0.9% | 60.17 | 61.71 |
| echo_1k_c1 | 34,689 | 34,656 | -0.1% | 52.00 | 57.25 |
| echo_1k_c8 | 99,528 | 100,467 | +0.9% | 111.96 | 104.62 |
| echo_1m_c1 | 3,968 | 4,165 | +5.0% | 413.29 | 401.04 |

64-active health shows the largest median increase, +12.6%. Every scenario
still has both positive and negative paired changes and overlapping ranges;
none meets the predeclared five-positive-pairs rule for a repeatable gain.
The structural removal of per-event scans is verified, while these wall-clock
results remain indicative rather than an established speedup. All eight median
changes pass the 15% retention guard; pipeline throughput and some tail values
are worse, so that guard is not evidence of no regression.

| Scenario | Before req/s range ↑ | Udata req/s range ↑ | Five paired throughput changes ↑ |
| --- | ---: | ---: | --- |
| health_c1 | 35,138–36,948 | 35,353–36,646 | +4.3%, -2.7%, +0.1%, -0.9%, +2.0% |
| health_c8 | 98,592–109,827 | 96,701–115,376 | +8.5%, -2.0%, -6.4%, +3.7%, +17.0% |
| health_c64 | 106,057–117,445 | 105,127–122,371 | +8.0%, +15.4%, -5.5%, -0.9%, +14.7% |
| health_idle64 | 33,571–35,554 | 34,972–36,633 | +2.2%, +3.0%, +5.7%, -1.5%, +3.6% |
| health_pipeline16 | 482,855–507,690 | 470,366–509,234 | +1.2%, -1.2%, -3.8%, -4.6%, +5.5% |
| echo_1k_c1 | 31,871–35,386 | 33,946–35,516 | -2.1%, +0.5%, +6.8%, -0.1%, +1.1% |
| echo_1k_c8 | 93,530–105,432 | 94,744–114,590 | -4.1%, +8.7%, +1.3%, -1.7%, +14.9% |
| echo_1m_c1 | 3,339–4,220 | 3,407–4,428 | +19.3%, +5.1%, -14.1%, -3.0%, +28.0% |

| Scenario | Before process CPU µs/request ↓ | Udata process CPU µs/request ↓ | Udata p50 / p95 / p99 µs ↓ |
| --- | ---: | ---: | ---: |
| health_c1 | 21.82 | 21.64 | 24.54 / 31.46 / 50.38 |
| health_c8 | 17.75 | 17.11 | 48.46 / 79.42 / 103.46 |
| health_c64 | 17.09 | 15.36 | 247.42 / 441.71 / 651.62 |
| health_idle64 | 22.36 | 22.09 | 24.67 / 30.96 / 50.71 |
| health_pipeline16 | 1.58 | 1.59 | 29.00 / 37.42 / 61.71 |
| echo_1k_c1 | 22.31 | 22.15 | 25.50 / 33.71 / 57.25 |
| echo_1k_c8 | 18.17 | 18.08 | 50.96 / 83.08 / 104.62 |
| echo_1m_c1 | 316.64 | 308.59 | 218.50 / 296.46 / 401.04 |

Across ten complete runs, **18,457,885 measured responses** validated with
zero socket, wire or deadline errors. Udata process-lifetime peak RSS ranges
from 2,539,520 to 22,675,456 bytes. The longer pipeline run grows the
client latency-sample vector beyond its initial reservation; CPU/RSS include
that bookkeeping and both threads. No per-connection RSS saving, independent
server CPU result, saturation point or long-duration stability claim follows.
The experiment includes the required storage change and does not separately
attribute time to pointer lookup versus memory layout.

**Test-first and full verification**: the new real-kqueue regression first
reported `wire=Pass stable_udata=Fail`, with 161 connection registrations.
After implementation it reports both Pass, including owner-vector growth,
alternating collection, descriptor reuse and 258 currently writable same-batch
hints. The forwarding test harness observes actual registrations and augments
real read events only when a zero-time poll confirms writability; socket I/O
and production dispatch still execute. It is absent from timing executables.
Debug **11/11**, Release **13/13**, ASan/UBSan **11/11** pass, with leak detection
disabled as in the previous round. Governance tests: **303/303**; repository
validator, clang-format and diff whitespace checks pass. Parser magnitude guard: 18/18; complete
ordinary allocation reports are unchanged. Equivalence streams remain
68,241,183 byte-identical bytes with the previously recorded SHA-256. The
source review confirms direct local identity checks at all three dispatch
sites and collection after both passes; it does not assert source text layout.

**Reproduction**: archive `08f85e9`, apply the first-round saved source patch
for this Before, then apply the [incremental udata patch](2026-10-22-kqueue-udata.patch)
for this After. Its SHA-256 is `b0415ce6b0bd77dcbf4316386bd87147ec2a0eff4959628be88af80654940ed9`.
Both patches have been applied independently from the Git base and all final
file hashes match. Use the existing Release recipe and run
`http_server_bench --duration-ms 2000` in five alternating pairs. Commands,
source hashes, all raw samples and complete check results are in the additional
[udata evidence JSON](2026-10-22-kqueue-udata-evidence.json). The earlier report,
raw evidence and patches below remain the first-round snapshot. Task-owned
comparison trees, sanitizer builds and 48 regenerated routed object/library/test
outputs were deleted; the preexisting build directory/configuration is preserved.

## First-Round Scope And Outcomes

This rewrite replaces the earlier 10-22 summary with fresh full parser and
real-socket measurements. The socket benchmark did not exist in the earlier
report; parser-only results never compared poll with kqueue.

Two costs are now removed from the existing kqueue path: receive bytes feed
the incremental decoder directly, with pipeline suffixes advanced as a view;
and newly queued output attempts a nonblocking send before another readiness
wait. The focused tests demonstrate both removals. The measured before/after
throughput changes are mixed and **do not establish a repeatable throughput
improvement**. All eight median changes pass the predeclared 15% regression
guard; that guard is not proof of no smaller regression.

The same-day normalized poll comparison also does not support the claim that
this kqueue implementation is generally weaker on these workloads. Compare
poll with **Before kqueue** to isolate the event mechanism; comparing poll
with After additionally includes the two new path optimizations. No universal
backend ranking, production SLO or saturation capacity follows.

## Authorization And Records

In this task chat the owner requested optimization, a full rerun and a report
rewrite, then explicitly selected “本轮豁免，实施并回补 ADR” for the scoped
preapproval/serialization question. The retrospective records are:

- `docs/adr/ADR-0010-http-server-performance-measurement.md` — measurement
  tooling and its opt-in build/CLI contract.
- `service/docs/adr/ADR-0008-kqueue-response-scheduling.md` — socket request
  delivery and regression checks.

Both are Proposed / Not Started pending individual approval. Historical
exception execution is recorded separately; the waiver is not acceptance.
The existing runtime and hosted-CI records retain their status. Earlier rounds
remain governed by Accepted / Complete service ADR-0005, ADR-0006 and ADR-0007.
No commit, push, deployment, hosted Actions run, dependency, intrinsic, LTO or
production build-flag change occurred.

## Source Identity And Environment

- **October parser anchor**: `34d598738631ece0ef6023c154fbf3ae52520296`.
  It carries the 10-16 parser work and was committed on 10-19; no server source
  exists there. The poll server first appeared at `f7ca39c` on 10-19 and kqueue
  at `5da6348` on 10-20.
- **Pre-task 10-22 / Before kqueue**: `08f85e9`, full identity
  `08f85e9733e91ea5ffaf261f6b1b7a9cc433ca3b`. This already contains fixed-body reserve, static
  rejection wires and append-form response serialization.
- **First-round updated 10-22 / After kqueue**: that base plus the saved
  [source patch](2026-10-22-http-performance-source.patch). SHA-256 of the patch:
  `99bc468255b32b824a623642b5c8a299574041984f2a65617cacc59c90d2a62c`.
  The [evidence JSON](2026-10-22-http-performance-evidence.json) identifies every
  changed implementation file; this is a pre-commit source snapshot, not a
  fictitious commit anchor.
- **Normalized poll control**: pre-task production sources with only the
  event-loop mechanism restored using the poll implementation, preserving
  current TCP_NODELAY, parser, route, serializer and socket options. The exact
  [control patch](2026-10-22-poll-control.patch) applies to the pre-task source.
  Its SHA-256 is `9a3a96dd09e1828962b5b6103b9c4c5e6f8faf5df45af3b7c9c68f3aed8eef69`.
- Apple clang 21.0.0 (clang-2100.3.34.2), arm64, macOS 27.0.1, CMake 4.2.3,
  Release `-O2 -DNDEBUG`, no LTO. Apple M4 Max, 16 physical/logical cores,
  64 GiB RAM (read-only host metadata captured after measurement). No
  cross-machine or cross-day absolute-latency comparison is claimed.

The socket driver is byte-identical in all three controls. The parser driver
and workloads are byte-identical in October/pre-task/updated builds. Production
parser source is unchanged by this new round.

## Measurement Validity

Five sequential groups alternate forward/reverse executable order; no compiler,
sanitizer suite or parallel benchmark ran during the formal timing groups.
Each server scenario warms up for 20 ms, issues closed-loop windows for 500 ms,
and drains every issued request. Connection setup, warmup and shutdown are
excluded. One nonblocking poll client loop and one production server thread
share a process. Both endpoints use TCP_NODELAY. The client checks every
response byte against a wire constructed independently of the serializer.
Across 15 server runs and eight scenarios, **6,543,379 measured responses**
completed with zero socket, wire-validation or deadline errors. Warmup responses
are additional and excluded from that count.

Server p50/p95/p99 are nearest-rank individual response completion latencies
from the first send of their pipeline window. They include client scheduling,
sending, server work and pipeline position. Pipeline latency is not divided by
16. Each table cell is the median of five per-run statistics, not a percentile
pooled across runs. The client can limit reported throughput.

Parser timing retains eleven warmed batch samples of at least 5 ms per run,
five alternating groups, with construction, owned materialization and
destruction included where applicable. These are batch-average parser costs;
they are not network latency distributions. Timing and ordinary allocation
counting remain separate executables.

## Real-Socket Results

| Scenario | Normalized poll req/s ↑ | Before kqueue req/s ↑ | After kqueue req/s ↑ | After vs before (throughput) ↑ | Before p99 µs ↓ | After p99 µs ↓ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 33,167 | 35,354 | 36,524 | +3.3% | 68.00 | 51.29 |
| health_c8 | 87,749 | 98,599 | 109,884 | +11.4% | 112.58 | 126.33 |
| health_c64 | 97,360 | 110,864 | 101,750 | -8.2% | 715.54 | 672.38 |
| health_idle64 | 18,186 | 34,733 | 35,347 | +1.8% | 54.04 | 50.62 |
| health_pipeline16 | 441,563 | 479,456 | 491,207 | +2.5% | 74.46 | 69.04 |
| echo_1k_c1 | 31,825 | 33,601 | 33,344 | -0.8% | 78.67 | 59.58 |
| echo_1k_c8 | 83,121 | 100,366 | 95,856 | -4.5% | 115.75 | 110.04 |
| echo_1m_c1 | 3,856 | 3,921 | 3,988 | +1.7% | 390.33 | 381.29 |

Before/after ranges overlap on every scenario and every scenario has both
positive and negative paired changes. The largest median improvement is
health_c8 (+11.4%); the largest median reduction is health_c64 (-8.2%). Neither
is a resolved repeatable gain or loss under the declared five-pair direction
criterion. The median p99 changes likewise vary; health_c8 p99 rises despite
its higher median throughput. Small-response latency and throughput must be
reported together.

| Scenario | Before req/s range ↑ | After req/s range ↑ | Five paired throughput changes ↑ |
| --- | ---: | ---: | --- |
| health_c1 | 34,478–35,568 | 34,205–39,783 | -1.9%, +12.5%, +5.8%, -3.8%, +6.7% |
| health_c8 | 89,388–101,108 | 96,610–114,202 | +7.6%, -4.4%, +22.9%, +15.2%, +15.1% |
| health_c64 | 102,971–117,047 | 95,330–118,850 | -18.6%, -8.6%, +6.4%, +4.2%, -1.2% |
| health_idle64 | 34,286–35,276 | 33,541–36,122 | +4.0%, -2.2%, +4.5%, +0.2%, -1.4% |
| health_pipeline16 | 470,710–497,411 | 468,790–493,836 | -0.9%, -1.3%, -1.1%, +4.9%, +2.5% |
| echo_1k_c1 | 30,500–34,991 | 32,815–35,244 | -0.8%, +0.7%, -2.6%, +0.6%, +13.8% |
| echo_1k_c8 | 96,376–113,041 | 90,757–111,558 | -16.3%, -18.5%, -0.3%, +11.2%, -2.7% |
| echo_1m_c1 | 3,811–4,152 | 3,499–4,190 | +5.5%, +4.6%, +0.9%, +3.6%, -10.8% |

The normalized poll control is especially slower with 64 idle connections:
its median is about 18.2k requests/s versus 34.7k for pre-task kqueue. That
same-source comparison is evidence for this workload only. At 64 active
connections and for the large echo, run overlap/variation prevents a universal
conclusion about backend superiority.

### Focused health_c64 Cause Check

The first-round median throughput fell **8.2%**, from 110,864 to 101,750
req/s. Process CPU time per request rose from 16.51 to 17.30 µs (+4.8%),
while median p50 rose from 245.25 to 288.00 µs (+17.4%). In contrast, p95
fell from 560.96 to 464.92 µs (-17.1%) and p99 from 715.54 to 672.38 µs
(-6.0%). This is a measured throughput/typical-latency deterioration in that
round, accompanied by better tails; it must not be dismissed merely because
it passed the 15% retention guard. The five paired throughput changes above
have mixed signs, so a persistent regression and its cause were unresolved.

A targeted four-way ablation then reran only health_c64: eight alternating
forward/reverse groups, 2000 ms issuing per variant, the same independent
wire-validating driver and Release flags. The baseline and combined server
sources byte-match the first round's Before and After, respectively. The two
intermediate variants isolate direct input feeding and immediate sending;
they retain the original connection storage and linear event lookup. This
investigation does not include the later udata change or replace either full
eight-scenario experiment. All **6,299,239 measured responses** validated with
zero errors. Each value below is a median of eight per-run statistics.

| Variant | req/s ↑ | Throughput vs baseline ↑ | Process CPU µs/request ↓ | p50 / p95 / p99 µs ↓ |
| --- | ---: | ---: | ---: | ---: |
| Original baseline | 95,997 | — | 19.12 | 332.73 / 609.67 / 832.10 |
| Direct input only | 97,924 | +2.0% | 18.86 | 335.00 / 638.96 / 857.42 |
| Immediate send only | 94,436 | -1.6% | 18.82 | 341.58 / 597.00 / 997.79 |
| Both first-round changes | 105,594 | +10.0% | 17.30 | 293.62 / 520.94 / 767.29 |

The combined variant did not reproduce the earlier median drop: its paired
throughput changes were -8.0%, +7.5%, -3.6%, +7.6%, +22.0%, +4.5%, +3.5%
and +9.6%. All three modified variants have mixed paired signs and overlapping
throughput ranges; this exploratory rerun establishes neither a repeatable
loss nor a repeatable gain. Even the unchanged baseline ranges from 90,523
to 107,659 req/s. Run variation is substantial, but these results do not
identify its host or scheduling cause.

Separate forwarding syscall diagnostics ran three 500 ms samples per variant;
their timing is excluded from the performance comparison. Counts include
setup, warmup and teardown. Every observed server send returned a positive
byte count, with zero send EAGAIN results. Median kevent calls per
successful server send were 0.496 for the baseline and 0.401 for the combined
variant. The immediate-send variants needed no write readiness events in
these samples. Send-side busy retry is therefore not a supported explanation
for the original drop. Counts are normalized by successful send calls, not
timed responses; they do not measure context switches or prove a batching
speedup.

The source confirms a scheduling change: the server now sends after each
read action, allowing client work to interleave before the remaining reads
finish. Its interaction with the single poll client and event batching is a
candidate mechanism, not an established cause of the loss. Parser behavior,
the kqueue backend and linear lookup were shared by both first-round
controls, so their presence alone does not explain that before/after change.
Server-only CPU, client/server scheduling, context-switch and host-contention
profiles were not captured. Those are the next attribution checks; the
specific root cause remains unconfirmed. Raw samples, exact variant diffs,
source hashes, build commands and diagnostic source/counts are retained in
the [focused evidence JSON](2026-10-22-health-c64-analysis-evidence.json).
All task-owned experimental source/build trees and executables were deleted.

## Resources, Stability And Capacity Limits

| Scenario | Before CPU µs/request ↓ | After CPU µs/request ↓ | After p50 / p95 / p99 µs ↓ |
| --- | ---: | ---: | ---: |
| health_c1 | 22.03 | 22.05 | 24.83 / 32.67 / 51.29 |
| health_c8 | 18.29 | 16.60 | 47.04 / 80.04 / 126.33 |
| health_c64 | 16.51 | 17.30 | 288.00 / 464.92 / 672.38 |
| health_idle64 | 22.65 | 22.58 | 25.21 / 34.04 / 50.62 |
| health_pipeline16 | 1.66 | 1.62 | 29.58 / 39.79 / 69.04 |
| echo_1k_c1 | 23.46 | 22.78 | 26.12 / 36.17 / 59.58 |
| echo_1k_c8 | 17.95 | 19.08 | 54.79 / 86.29 / 110.04 |
| echo_1m_c1 | 321.39 | 317.94 | 220.12 / 300.29 / 381.29 |

CPU numbers include both client and server. Updated process lifetime peak RSS
ranges from 2,129,920 to 10,698,752 bytes across the recorded scenarios/runs;
these high-water marks include fixture bodies, latency samples and allocator
retention. They cannot quantify per-connection memory or prove an RSS reduction.
The deterministic input probe separately establishes one staging-band ordinary
allocation before versus zero after for its fixed 1 MiB request.

Warmup and timed intervals are explicit, but 500 ms runs do not establish
long-duration stability. There is no offered-load sweep, independent server CPU
profile, open-loop arrival schedule, memory plateau test or specified server
SLO. Observed throughput leveling between 8 and 64 connections may include
client/scheduling limits; it is not an established server saturation point.
There were zero measured errors, rather than an inferred absence of every
failure mode. Slow-reader recovery and rejection lifecycle are covered by
correctness tests outside timing.

## Full Parser Rerun

| Fixture | October anchor ns ↓ | Pre-task 10-22 ns ↓ | Updated 10-22 ns ↓ | Updated vs pre-task (time) ↓ |
| --- | ---: | ---: | ---: | ---: |
| head_long_values_16 | 306.1 | 319.4 | 317.9 | -0.5% |
| head_short_fields_1000 | 7,956.7 | 7,934.5 | 8,021.4 | +1.1% |
| head_ows_7800 | 411.5 | 409.2 | 411.7 | +0.6% |
| head_ows_8_fields | 448.1 | 453.9 | 456.0 | +0.5% |
| head_one_short_field | 52.4 | 54.1 | 54.7 | +1.0% |
| head_minimal | 20.2 | 20.1 | 20.3 | +1.2% |
| head_realistic_434 | 186.7 | 187.9 | 188.3 | +0.2% |
| head_full_8192 | 5,125.2 | 5,214.6 | 5,228.4 | +0.3% |
| head_decoder_whole | 224.5 | 227.3 | 229.6 | +1.0% |
| head_decoder_feed17 | 812.5 | 787.3 | 781.2 | -0.8% |
| echo_chunked_1024 | 177.2 | 175.5 | 176.3 | +0.4% |
| body_1m_chunk1024 | 43,872.7 | 43,431.6 | 43,234.4 | -0.5% |
| body_1m_chunk16384 | 28,916.3 | 29,034.0 | 28,788.7 | -0.8% |
| body_1m_fixed_feed16384 | 43,307.6 | 16,885.9 | 16,780.4 | -0.6% |
| extension_7000 | 1,961.2 | 1,966.7 | 1,965.3 | -0.1% |
| trailer_7000 | 1,954.7 | 1,969.9 | 1,960.5 | -0.5% |
| head_cr_dense | 156.9 | 154.8 | 155.8 | +0.7% |
| respond_echo_chunked_1024 | N/A | 54.2 | 53.3 | -1.7% |

The fixed-body result remains about 61% below the October anchor, corroborated
by ordinary allocation counts **11 → 4** and requested bytes
**4,173,088 → 1,048,872**. The newly changed server path does not execute in
these fixtures; pre-task/updated parser counts and requested bytes match on
all 18 fixtures. The October anchor has 17 fixtures because it lacks the
response serializer. Every shared allocation checksum agrees.

The October, pre-task and updated equivalence streams are byte-identical:
95,480 semantic cases, seed `0x53544F52`, 68,241,183 bytes, SHA-256
`6a27c498cb9910a9d961ecc4795c9b045de4ee8314a23a8f8dff6dfb4eb2dbac`.
The updated parser magnitude guard passes all 18 fixtures. Small unchanged-path
latency shifts remain unresolved; neither ±3.5% nor a 15% guard is treated as a
measured upper bound on noise.

## Implementation And Test-First Evidence

| Change | Before observation | Updated observation | Scope limit |
| --- | --- | --- | --- |
| Immediate send | KS-1 exact wire Pass; first_send_before_next_wait Fail | Both Pass | A blocked nonblocking send still retains bytes and uses EVFILT_WRITE |
| Direct receive feed | KS-2 body Pass; one ordinary allocation in 8193..65536-byte staging band | Body Pass; zero allocations in that band | Decoder still owns head/body results and allocates them |
| Pipeline advancement | Connection input copied, then erased per complete message | No staging string; string_view advances within the synchronous receive call | No general zero-copy response claim |
| Lifecycle checks | Existing keep-alive, rejection, idle and TCP_NODELAY tests | 512 cross-window requests, blocked-output recovery and active-peer progress also pass | No TLS/browser/runtime-completion claim |

KS-1 observes actual system calls via a test-only forwarding interposer; no
production instrumentation or bypassed socket framing is introduced. KS-2
counts ordinary allocations only on the server thread. Both tests were rerun
against pre-task production and failed for the intended cost, then passed
against the final source. A deliberately corrupted expected health response
caused the benchmark to exit 1 with `response wire mismatch`.

The current decoder returns completed message storage by move. Therefore merely
clearing reset state cannot pool the completed body; that proposal is excluded.
At the end of that first round, linear fd lookup and full connection scans
remained. The follow-up above now removes only the per-event lookup scan;
interest/deadline/collection scans remain. No full-loop complexity reduction
is claimed.

## Historical Rounds Retained As Historical Evidence

These earlier within-round numbers are retained from the previous report;
they are not substituted for the fresh tables above:

| Round | Red / green | Historical target evidence (time and allocation calls ↓) |
| --- | --- | --- |
| Fixed-body reserve, service ADR-0005 | ae95ecc / b419a80, formatting 9e96ef6 | 43,913 → 16,730 ns; 11 → 4 ordinary calls |
| Static rejection wires, service ADR-0006 | 2f53c85 / 1b638ba | Nine byte-equality pins; no latency claim |
| Append serialization, service ADR-0007 | 2be8eaa / 5b404c3 | 91.0 → 52.7 ns; 2 → 1 ordinary calls |

Static views eliminate rejection-wire construction allocations; appending that
view into an insufficiently sized outgoing string can still allocate. Likewise
append serialization removes the temporary wire allocation, while outgoing
capacity growth remains possible. No allocation-free server claim is made.

## Full Verification

- Routed trio: `cmake -S service -B service/build`,
  `cmake --build service/build`, `ctest --test-dir service/build --output-on-failure`:
  **10/10**, including 48 real-socket lifecycle checks and both focused tests.
- Release opt-in build with the documented flags: **12/12**, including both
  benchmark CLI suites; eight real-socket and eighteen parser fixtures.
- Debug ASan/UBSan: **10/10**, with `ASAN_OPTIONS=detect_leaks=0
  UBSAN_OPTIONS=halt_on_error=1`. The Release socket benchmark additionally ran
  all eight quick scenarios under ASan/UBSan. Apple LeakSanitizer is unsupported;
  no leak-detection result is claimed.
- Parser magnitude guard: 18/18; allocation checksums and equivalence as above.
- Governance suite: **303/303**; repository validator, clang-format check mode,
  workflow YAML parse and diff whitespace checks: **Pass**. Source-patch
  reconstruction matches all nine file hashes and both server controls. The
  Release-only benchmark option rejects Debug as required. Complete outputs are
  retained in the evidence JSON. No hosted Actions orchestration or remote
  verification ran.
- Initial sandboxed socket tests failed to bind with EPERM; authorized local
  loopback execution passed. That environment failure is not the TDD red result.

Raw commands, complete verification outputs, all five groups' timing reports,
allocation data, semantic hashes and source identities are retained in
[the evidence JSON](2026-10-22-http-performance-evidence.json). Test-only syscall
observation and allocation counting are absent from timing executables.

## Reproduction And Disposition

The current build recipe is in [service/bench/README.md](../../service/bench/README.md).
To reconstruct this exact uncommitted experiment, create disposable source
copies from the named Git commits; apply the saved source patch to `08f85e9`
for After. To obtain Before with identical drivers, copy that service tree and
restore only `service/src/http_server.cpp` and `.hpp` from `08f85e9`. Apply the
poll-control patch to that Before copy for the normalized control. For the
October parser build, archive `34d5987` and overlay the current `service/bench/`
verbatim; build only parser benchmark/allocation/equivalence targets there.
Do not run the newly added cost-regression tests against Before as passing
correctness checks: they deliberately expose its old costs.

Configure each comparison tree with the same recipe:

```sh
cmake -S <copy>/service -B <disposable-build> -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG' -DORANGE_BUILD_BENCHMARKS=ON
cmake --build <disposable-build> --parallel 3
<disposable-build>/http_server_bench --duration-ms 500
<disposable-build>/http_parser_bench
<disposable-build>/http_parser_allocations
<disposable-build>/http_parser_equivalence > <disposable-stream>
```

For server runs use forward/reverse poll/Before/After order for five groups.
For parser runs use forward/reverse October/Before/After order. Compare binary
semantic streams directly with `cmp`, retain complete JSON, then delete task-owned
build/source copies and streams. A separate sanitizer build uses
`-fsanitize=address,undefined -fno-omit-frame-pointer` and matching linker flags.

Task-owned disposable compiler and comparison output is deleted at completion.
The preexisting `service/build` directory is preserved, with this task's newly
generated objects, library and test executables removed. Source patches and
measurement reports are verification evidence, not retained runtime artifacts.
Formal ADR acceptance remains a separate individual-approval step.
