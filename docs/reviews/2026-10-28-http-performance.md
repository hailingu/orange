# HTTP Parser And Loopback Serving Performance — 2026-10-28

**Metric direction**: **↑ higher is better**; **↓ lower is better**, when the
workload and measurement scope are comparable. Throughput (req/s) is ↑;
latency percentiles, processing time (µs/ns), CPU time per request, system
calls per request, context switches per request, allocation calls and requested
allocation bytes are ↓. For percentage changes, a positive throughput change is
better, while a negative time/cost/event-count change is better. Arrows on
ranges describe the measured values, not the width of the range. Peak RSS is ↓
only with comparable process scope and fixture state; the process-lifetime
high-water marks here do not establish server memory savings. These directions
do not establish statistical significance.

## Follow-Up: 64 KiB Receive Window (Round 3) — 2026-10-28

Following Round 2, the owner re-selected the receive-buffer candidate that had
been rejected on 2026-10-23, as the only overturned item (`"只执行
receive-buffer（16→64 KiB）这个优化，其他的不动，然后再做一轮测试，更新benchmark
文件"`). Under `ADR-0008` KS-11, `read_connection`'s per-drain receive window
grows from 16 KiB to 64 KiB (`orange::http::(anonymous namespace)::receive_window`);
nothing else changes — client write sizes, framing, caps, staging, backpressure
and the dirty/interest machinery are untouched.

A new focused suite (`http_server_receive_tests`, interposing `recv` on the
server thread like the KS-1 scheduling pattern) pins the contract first:
**red** at HEAD prints `data_receives=5 window64k=Fail` (a one-write 64 KiB
body needs at least `ceil(65579/16384) = 5` data receives under the 16 KiB
window); **green** with the widened window prints `data_receives=2` with
`wire/body/window64k` all Pass.

### Measurement And Controls (Round 3)

Two server variants compiled with identical Release `-O2 -DNDEBUG` flags and
byte-identical driver sources: **Pre-window** (`4e6c154` archive of HEAD) and
**Windowed** (worktree plus
[`2026-10-28-receive-window.patch`](2026-10-28-receive-window.patch), SHA-256
`dc1c96631ecfeab5f91873fc94cb8ee66455e6650f0597cfe59b8f9d623f1090`, committed
as `cfdce4a`; only `http_server.cpp` differs in the linked library). Five sequential groups
alternated forward and reverse order over all ten scenarios (2000-ms windows).
This round ran on a busier host than Round 2 — absolute throughput baselines
are lower across the board (for example `health_c1` ≈ 29k versus ≈ 32k), so
cross-round absolute comparison is not claimed; the paired same-window design
carries the evidence. Two `health_idle2048` invocations (one per variant) hit
`ECONNRESET` (`errno=54`) during connection-burst setup — the same known
listen-backlog setup race Round 1 recorded for the Poll control — and were
discarded and re-executed cleanly per the established audit practice. Across
the ten complete paired reports, **16,324,499 measured responses** validated
with zero socket, wire, or deadline errors.

### Real-Socket Results (Round 3: Pre-window / Windowed)

| Scenario | Pre-window req/s ↑ | Windowed req/s ↑ | Throughput change ↑ | Pre-window CPU µs/req ↓ | Windowed CPU µs/req ↓ | CPU change ↓ | Pre-window syscalls/req ↓ | Windowed syscalls/req ↓ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 29,416 | 29,505 | +0.3% | 10.25 | 10.19 | -0.6% | 3.000 | 3.000 |
| health_c8 | 80,843 | 83,684 | +3.5% | 8.82 | 8.74 | -0.9% | 2.925 | 2.928 |
| health_c64 | 87,868 | 88,979 | +1.3% | 8.17 | 7.97 | -2.5% | 2.809 | 2.842 |
| health_idle64 | 28,481 | 30,318 | +6.5% | 10.58 | 10.26 | -3.1% | 3.000 | 3.000 |
| health_pipeline16 | 402,133 | 435,039 | +8.2% | 0.82 | 0.80 | -2.7% | 0.188 | 0.188 |
| echo_1k_c1 | 28,565 | 28,463 | -0.4% | 10.49 | 10.26 | -2.2% | 3.000 | 3.000 |
| echo_1k_c8 | 82,618 | 79,774 | -3.4% | 9.05 | 9.06 | +0.1% | 2.934 | 2.927 |
| echo_1m_c1 | 4,038 | 4,477 | **+10.9%** | 154.94 | 137.98 | **-10.9%** | **67.024** | **19.989** |
| health_idle512 | 30,435 | 29,637 | -2.6% | 9.72 | 10.19 | +4.8% | 3.000 | 3.000 |
| health_idle2048 | 28,367 | 27,788 | -2.0% | 10.07 | 10.40 | +3.3% | 3.000 | 3.000 |

| Scenario | Pre-window req/s range ↑ | Windowed req/s range ↑ | Five paired Windowed/Pre-window changes ↑ |
| --- | ---: | ---: | --- |
| health_c1 | 27,510–30,507 | 28,511–31,120 | +4.0%, -3.3%, +8.4%, -3.1%, +4.5% |
| health_c8 | 73,037–87,231 | 72,633–88,384 | +10.3%, -4.1%, -0.1%, +1.5%, -0.6% |
| health_c64 | 78,596–92,173 | 78,158–92,323 | +1.6%, -3.5%, +3.9%, +3.5%, -0.6% |
| health_idle64 | 26,222–30,485 | 28,639–31,303 | +9.9%, +6.2%, -0.5%, +0.0%, +19.0% |
| health_pipeline16 | 369,431–428,229 | 396,972–444,210 | +3.2%, +1.6%, +7.8%, +7.5%, +10.5% |
| echo_1k_c1 | 26,722–29,847 | 27,539–30,888 | +4.8%, +5.9%, -0.4%, +3.1%, -5.6% |
| echo_1k_c8 | 71,928–85,700 | 77,467–84,010 | +1.7%, -4.4%, -5.7%, +1.8%, +7.7% |
| echo_1m_c1 | 3,489–4,188 | 4,162–4,633 | **+10.6%, +10.9%, +9.2%, +8.4%, +19.3%** |
| health_idle512 | 29,374–31,805 | 28,627–32,366 | +1.8%, -2.6%, +1.3%, -7.6%, -3.6% |
| health_idle2048 | 27,239–29,652 | 25,379–29,966 | +5.6%, +4.8%, -4.2%, -6.3%, -6.8% |

**Round 3 Assessment**:

- **The one targeted scenario separates completely.** `echo_1m_c1` drops from
  **67.024 to 19.989 Unix syscalls per request** (-70%) — the exact predicted
  composition of 16 `recv` calls (1 MiB / 64 KiB) plus two `kevent` waits and
  two `send` calls, versus 64 `recv` calls at the 16 KiB window. Worker CPU
  falls **-10.9%** (154.94 → 137.98 µs/req), throughput rises **+10.9%**
  (4,038 → 4,477 req/s) with **5/5 positive paired changes and non-overlapping
  five-run ranges** (3,489–4,188 versus 4,162–4,633). p50 drops 210.7 → 189.5 µs.
- **`health_pipeline16` shows 5/5 positive pairs (+1.6% to +10.5%) with
  unchanged syscall counts (0.188)** — suggestive but with overlapping ranges,
  so no claim is made; the mechanism there is not syscall reduction.
- All other scenarios stay inside the established noise band with mixed paired
  signs (throughput -3.4% to +6.5%), and every median retention ratio is ≥
  0.966 (`echo_1k_c8` 79,774/82,618), far above the 0.85 guard. The 3.000
  syscall floor for unpipelined small exchanges is untouched, as expected —
  those requests never fill a 16 KiB window.
- The window costs no allocation (stack buffer) and no behavior change: every
  routed suite, sanitizer run and the byte-exact wire checks stay green.

### Verification (Round 3)

- Red/green: `ctest -R '^http_server_receive_tests$'` — red at HEAD
  (`data_receives=5 window64k=Fail`, wire/body Pass), green after the change
  (`data_receives=2`, all Pass).
- Routed Debug CTest: **13/13 Passed** (including the new suite).
- Release opt-in CTest (worktree, benchmarks ON): **16/16 Passed**, including
  `http_server_bench_cli` floors; Pre-window archive tree: **15/15 Passed**.
- Debug ASan/UBSan CTest with
  `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`: **13/13 Passed**.
- `clang-format --dry-run --Werror` on the changed sources passes.
- Raw run JSON for both variants, the discarded-invocation audit, and binary
  SHA-256 fingerprints are retained in
  [`2026-10-28-http-performance-evidence.json`](2026-10-28-http-performance-evidence.json)
  under `receive_window_r3`.

## Follow-Up: Header-Length Pre-Filter, Write-Pass Index, And Drain-Once Activity Clock — 2026-10-28

Following the initial 2026-10-28 SB-6 baseline rerun below, the owner requested
applying and measuring the three remaining internal micro-tunings within the
existing `ADR-0008` transport/decoder boundaries (`"按照你的建议微调更新一次，并再次测试一轮"`):

1. **Header-name length pre-filter in `detail::head_policy`**:
   `head_policy` switches on `field.name.size()` (`10` for `"connection"`, `14`
   for `"content-length"`, `17` for `"transfer-encoding"`) before running
   case-insensitive `equal_ascii` byte comparisons, skipping character-by-character
   folding on every unrelated header field.
2. **First-write batch index in `LoopbackServer::serve`**:
   The read pass records the index `first_write` of the first `EVFILT_WRITE`
   event in the harvested `kevent` batch (defaulting to `ready` when none
   occurred). In the common immediate-send steady state where no write filter
   fires, the second loop (`for (int i = first_write; i < ready; ++i)`) skips
   scanning the `events[0..ready)` array entirely while preserving exact
   read-before-write batch ordering when write events are present.
3. **Once-per-drain activity timestamp in `read_connection` / `write_connection`**:
   Instead of invoking `std::chrono::steady_clock::now()` after every 16 KiB
   `::recv` or partial `::send` iteration inside a single readiness drain, each
   helper records `progressed = true` on positive I/O and samples
   `steady_clock::now()` once when the nonblocking drain completes, saving 63
   redundant clock reads per 1 MiB request body while keeping fresh monotonic
   per-readiness timestamps.

### Measurement And Controls (Round 2)

Four server variants (`Poll`, `08f85e9` Before, `17e571e` Pre-tuned, and
`Tuned` worktree with [`2026-10-28-micro-tuning.patch`](2026-10-28-micro-tuning.patch),
SHA-256 `a7c2ed437e12ba99efe287eeb296e44504408a4c8252c45e7a33c40814c4db0f`)
and four parser variants (`October`, `08f85e9`, `17e571e` Pre-tuned, and
`Tuned`) were compiled with identical Release `-O2 -DNDEBUG` flags and
byte-identical benchmark drivers. Five sequential groups alternated forward and
reverse order over all ten server scenarios (2000-ms windows) and all 18 parser
fixtures. Across the 20 ten-case server reports in Round 2, **35,928,259
measured responses** (**18,435,285** across Pre-tuned and Tuned) validated with
zero socket, wire, or deadline errors.

### Real-Socket Results (Round 2: Poll / `08f85e9` / Pre-Tuned `17e571e` / Tuned)

| Scenario | Poll req/s ↑ | `08f85e9` req/s ↑ | Pre-tuned (`17e571e`) req/s ↑ | Tuned req/s ↑ | Tuned/Pre-tuned change ↑ | Tuned/`08f85e9` change ↑ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 33,480 | 32,597 | 32,574 | 32,074 | -1.5% | -1.6% |
| health_c8 | 97,215 | 101,485 | 95,439 | 99,451 | +4.2% | -2.0% |
| health_c64 | 105,388 | 106,614 | 109,119 | 102,493 | -6.1% | -3.9% |
| health_idle64 | 25,460 | 32,586 | 32,904 | 33,461 | +1.7% | +2.7% |
| health_pipeline16 | 445,820 | 453,806 | 465,070 | 452,868 | -2.6% | -0.2% |
| echo_1k_c1 | 32,129 | 31,535 | 32,573 | 30,854 | -5.3% | -2.2% |
| echo_1k_c8 | 98,433 | 95,980 | 94,875 | 92,749 | -2.2% | -3.4% |
| echo_1m_c1 | 3,961 | 3,852 | 4,068 | 3,985 | -2.0% | +3.5% |
| health_idle512 | 9,319 | 29,121 | 33,207 | 31,999 | -3.6% | +9.9% |
| health_idle2048 | 2,390 | 18,951 | 31,334 | 30,806 | -1.7% | +62.6% |

| Scenario | Poll worker CPU µs/req ↓ | `08f85e9` worker CPU µs/req ↓ | Pre-tuned worker CPU µs/req ↓ | Tuned worker CPU µs/req ↓ | Tuned/Pre-tuned CPU ↓ | Tuned/`08f85e9` CPU ↓ | Tuned syscalls/req (`08f`→Tuned) ↓ | Tuned csw/req ↓ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 11.16 | 10.37 | 9.90 | 9.93 | +0.4% | -4.2% | 4.000 → 3.000 | 1.023 |
| health_c8 | 9.06 | 8.46 | 8.40 | 8.30 | -1.2% | -1.9% | 3.408 → 2.808 | 0.184 |
| health_c64 | 8.67 | 8.18 | 7.57 | 7.82 | +3.3% | -4.4% | 2.557 → 2.401 | 0.058 |
| health_idle64 | 27.07 | 10.81 | 9.89 | 10.03 | +1.4% | -7.2% | 4.000 → 3.000 | 1.039 |
| health_pipeline16 | 0.92 | 0.84 | 0.80 | 0.81 | +1.4% | -3.0% | 0.250 → 0.188 | 0.064 |
| echo_1k_c1 | 11.79 | 10.85 | 10.28 | 10.48 | +2.0% | -3.4% | 4.000 → 3.000 | 1.041 |
| echo_1k_c8 | 9.12 | 8.89 | 8.65 | 8.78 | +1.5% | -1.2% | 3.360 → 2.800 | 0.171 |
| echo_1m_c1 | 158.88 | 164.61 | 152.95 | 156.14 | +2.1% | -5.1% | 68.007 → 67.028 | 1.229 |
| health_idle512 | 96.99 | 14.46 | 9.87 | 9.99 | +1.2% | -30.9% | 4.000 → 3.000 | 1.033 |
| health_idle2048 | 375.72 | 35.17 | 9.79 | 9.83 | +0.5% | -72.0% | 4.000 → 3.000 | 1.027 |

| Scenario | Pre-tuned req/s range ↑ | Tuned req/s range ↑ | Five paired Tuned/Pre-tuned changes ↑ | Tuned p50 / p95 / p99 µs ↓ |
| --- | ---: | ---: | --- | ---: |
| health_c1 | 32,264–33,818 | 31,633–34,055 | -5.3%, -1.5%, +3.1%, -2.0%, +4.8% | 24.79 / 32.04 / 61.79 |
| health_c8 | 93,910–104,745 | 92,217–105,019 | -6.7%, -7.6%, +8.7%, +5.9%, +11.4% | 49.79 / 79.79 / 122.04 |
| health_c64 | 98,351–111,428 | 100,924–111,755 | -7.1%, +2.6%, +7.0%, -5.0%, -8.0% | 273.92 / 445.83 / 724.58 |
| health_idle64 | 32,013–34,098 | 31,347–33,824 | -4.7%, +2.8%, +2.5%, -1.9%, -0.1% | 24.88 / 34.29 / 66.71 |
| health_pipeline16 | 445,334–470,593 | 434,550–462,118 | -0.8%, +0.3%, -7.7%, -2.6%, -0.6% | 29.50 / 38.88 / 62.96 |
| echo_1k_c1 | 32,431–33,088 | 29,102–33,291 | -12.0%, -8.1%, -6.6%, -2.8%, +2.7% | 26.00 / 35.92 / 61.29 |
| echo_1k_c8 | 92,620–101,491 | 92,054–97,261 | -3.0%, +0.1%, -8.7%, -4.2%, +3.1% | 52.88 / 82.33 / 117.88 |
| echo_1m_c1 | 3,756–4,173 | 3,948–4,005 | +5.1%, -2.4%, -0.9%, -4.8%, -1.6% | 214.00 / 304.42 / 485.08 |
| health_idle512 | 32,498–33,449 | 31,524–33,440 | -1.5%, -0.5%, -3.1%, -4.5%, +0.6% | 24.92 / 33.38 / 60.92 |
| health_idle2048 | 27,052–31,753 | 30,292–32,295 | +13.7%, -0.3%, -4.6%, -2.6%, +3.8% | 25.58 / 32.17 / 64.00 |

### Parser Results (Round 2: October / `08f85e9` / Pre-Tuned `17e571e` / Tuned)

| Fixture | October (`34d5987`) ns/req ↓ | `08f85e9` ns/req ↓ | Pre-tuned (`17e571e`) ns/req ↓ | Tuned ns/req ↓ | Tuned/Pre-tuned change ↓ | Tuned/`08f85e9` change ↓ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| head_long_values_16 | 303.71 | 313.71 | 303.87 (296.47–430.78) | 299.34 (292.89–339.15) | -1.5% | -4.6% |
| head_short_fields_1000 | 7,859.21 | 7,816.12 | 7,824.83 (7,772.87–7,918.21) | 7,772.99 (7,744.14–10,087.44) | -0.7% | -0.6% |
| head_ows_7800 | 399.72 | 403.38 | 406.73 (406.14–416.19) | 408.32 (404.02–414.94) | +0.4% | +1.2% |
| head_ows_8_fields | 439.33 | 440.82 | 443.61 (432.68–525.49) | 444.04 (441.72–444.62) | +0.1% | +0.7% |
| head_one_short_field | 51.35 | 53.32 | 51.64 (50.85–53.46) | 51.24 (49.31–51.47) | -0.8% | -3.9% |
| head_minimal | 19.43 | 19.92 | 19.11 (18.91–29.79) | 19.30 (19.11–21.29) | +1.0% | -3.1% |
| head_realistic_434 | 184.87 | 185.75 | 187.83 (181.39–189.54) | 182.93 (179.87–186.41) | -2.6% | -1.5% |
| head_full_8192 | 5,136.88 | 5,125.41 | 5,212.16 (5,012.41–5,282.06) | 5,081.87 (5,002.48–5,122.03) | -2.5% | -0.8% |
| head_decoder_whole | 219.56 | 224.88 | 231.05 (223.90–237.10) | 229.39 (226.04–234.09) | -0.7% | +2.0% |
| head_decoder_feed17 | 763.97 | 761.49 | 769.70 (758.45–782.77) | 781.29 (773.40–807.60) | +1.5% | +2.6% |
| echo_chunked_1024 | 169.89 | 173.92 | 176.53 (175.15–178.26) | 176.29 (173.57–179.36) | -0.1% | +1.4% |
| body_1m_chunk1024 | 42,054.03 | 42,494.14 | 42,206.06 (41,840.82–43,120.12) | 42,869.79 (41,823.90–45,954.44) | +1.6% | +0.9% |
| body_1m_chunk16384 | 28,520.18 | 28,459.63 | 28,472.82 (27,782.56–29,805.83) | 28,179.52 (27,335.61–33,937.50) | -1.0% | -1.0% |
| body_1m_fixed_feed16384 | 41,931.96 | 16,543.38 | 16,367.11 (16,212.24–17,200.52) | 16,437.58 (16,369.87–23,529.30) | +0.4% | -0.6% |
| extension_7000 | 1,924.90 | 1,914.14 | 1,916.76 (1,913.65–1,991.43) | 1,933.97 (1,907.51–1,965.48) | +0.9% | +1.0% |
| trailer_7000 | 1,925.59 | 1,924.23 | 2,022.43 (1,909.68–2,176.73) | 1,976.97 (1,906.30–2,152.72) | -2.2% | +2.7% |
| head_cr_dense | 150.92 | 152.63 | 153.02 (152.24–157.19) | 153.57 (151.73–160.77) | +0.4% | +0.6% |
| respond_echo_chunked_1024 | N/A | 52.24 | 52.21 (51.19–53.53) | 53.83 (52.56–54.17) | +3.1% | +3.0% |

**Round 2 Assessment**:
- On the parser benchmarks, `head_policy` length pre-filtering lowers the
  multi-field medians (`head_realistic_434` **187.83 → 182.93 ns**, `-2.6%`;
  `head_full_8192` **5,212.16 → 5,081.87 ns**, `-2.5%`; `head_short_fields_1000`
  **7,824.83 → 7,772.99 ns**, `-0.7%`; `head_decoder_whole` **231.05 → 229.39 ns**,
  `-0.7%`), bringing `head_realistic_434` and `head_full_8192` slightly below
  both `17e571e` and `08f85e9`, though still with overlapping five-run ranges.
- On the real-socket server benchmarks, `Tuned` vs `Pre-tuned` (`17e571e`) shows
  mixed paired signs across all ten scenarios (worker CPU within `-1.2%` to
  `+3.3%`, throughput within `-6.1%` to `+4.2%` with overlapping ranges),
  confirming the prediction that tens-of-nanoseconds user-space shavings are
  submerged by the ~9.6 µs kernel socket I/O cost and shared-host scheduling
  variance. Against `08f85e9`, `Tuned` preserves the exact **4.000 → 3.000**
  syscall reduction, lower worker CPU across all ten scenarios (`-1.2%` to
  `-72.0%`), and 5/5 positive paired throughput gains on `echo_1m_c1` (`+3.5%`),
  `health_idle512` (`+9.9%`), and `health_idle2048` (`+62.6%`).
- In the Round 2 Read-Batch Send control (`health_c64`, **2,163,925** validated
  responses), `Immediate` (`111,091 req/s`, `7.51 µs/req`, `2.382 syscalls/req`)
  outperformed `Batch` (`106,613 req/s`, `7.88 µs/req`, `2.377 syscalls/req`)
  across all five pairs (`-4.9%, -8.5%, -0.3%, -5.2%, -2.4%` for Batch vs
  Immediate).

## Initial Rerun With Deterministic Server Event Counts — 2026-10-28

This report re-executes the full real-socket server and HTTP parser benchmark
program from [`2026-10-22-http-performance.md`](2026-10-22-http-performance.md)
against the committed `HEAD` revision (`17e571e`), which adds deterministic
child-process event deltas (`server_unix_syscalls` and `server_context_switches`
under `ADR-0010` SB-6) to the split-process benchmark harness while keeping the
production kqueue loop (`ADR-0008` KS-1 through KS-10) uninstrumented.

Across the four cumulative transport rounds landed between `08f85e9` and
`17e571e`:

1. **Direct receive feeding and immediate nonblocking send (KS-1, KS-2)**:
   `recv` windows feed `RequestDecoder::feed` directly and advance pipeline
   suffixes via `std::string_view` without a per-connection staging string or
   prefix erasure; responses queued during a read attempt a nonblocking `send`
   before the next `kevent` wait instead of arming `EVFILT_WRITE` first.
2. **Direct `udata` connection dispatch (KS-5, KS-6)**: every connection filter
   carries its stable `Connection *` in `kevent.udata`, replacing per-event
   linear descriptor scans while retaining retired objects until both event
   passes finish.
3. **Event-driven connection management (KS-7)**: `ConnectionStore` drains an
   intrusive dirty queue for interest transitions, reads the nearest idle
   deadline from the head of an activity-ordered `std::list<Connection>`,
   retires only due heads, and destroys retired nodes at batch end, removing
   four full live-connection scans.
4. **Interest-transition guard, tail-splice skip, and single-pass head policy
   (KS-8, KS-9, KS-10)**: a serviced connection enters the dirty queue only
   when `wanted_interest` differs from tracked kernel state; `touch` skips the
   identity splice when the connection is already at the activity tail; and
   `detail::head_policy` resolves body framing plus `Connection: close` (RFC
   9110 7.6.1) in one pass over validated headers (`ParsedMessage::close`),
   removing the response-time header scan.

### Source Identity And Environment

- **Current production and harness (`After` / `HEAD`)**:
  `17e571e2873793830ae1791339142f26b5032d23` (`feat(bench): report server event counts`),
  which includes production transport commit `d577443ef30631bd778d44c20042cb84805c2775`
  and split-process benchmark commit `13f835123f3492effc26ed4c767b744a931bbe95`.
- **Pre-optimization kqueue baseline (`Before` / `08f85e9`)**:
  `08f85e9733e91ea5ffaf261f6b1b7a9cc433ca3b` (`docs(reviews): rewrite the 2026-10-22 report against the October anchor`),
  paired with the byte-identical `17e571e` split-process benchmark driver so
  both revisions report identical CPU, RSS, syscall, and context-switch fields.
- **Normalized `poll` control (`Poll`)**: the `udata` transport snapshot
  (`08f85e9` + [`2026-10-22-http-performance-source.patch`](2026-10-22-http-performance-source.patch) +
  [`2026-10-22-kqueue-udata.patch`](2026-10-22-kqueue-udata.patch)) with
  [`2026-10-23-poll-control.patch`](2026-10-23-poll-control.patch) (SHA-256
  `fa7b194127b10fb19cf89b2d3c2330990184de83dfca2bb48a4663decdba7c15`) applied
  and paired with the byte-identical `17e571e` driver. It retains direct input
  feeding, immediate send, `TCP_NODELAY`, and identical parser/serializer code,
  normalizing only the `poll(2)` readiness mechanism.
- **Read-batch send control (`After read batch`)**: `17e571e` with
  [`2026-10-23-batch-send-control.patch`](2026-10-23-batch-send-control.patch)
  (SHA-256 `b6de25e0f418357d6a51a6c300f23b165da823482480e740b5ff3814db454a02`),
  deferring the nonblocking send until all read events in the harvested batch
  have been processed.
- **October parser anchor (`October`)**:
  `34d598738631ece0ef6023c154fbf3ae52520296`, compiled against the
  byte-identical `17e571e` parser benchmark, allocation, and equivalence
  drivers.
- **Toolchain and host**: Apple clang 21.0.0 (`clang-2100.3.34.2`), `arm64`,
  macOS 27.0.1 (`26B92`), CMake 4.2.3, Release `-O2 -DNDEBUG` without LTO.

### Measurement And Controls

The client runs in the parent process and the uninstrumented production server
runs on a worker thread in a forked child process, created before any worker
thread starts. Boundary samples report parent `RUSAGE_SELF` CPU, child
`RUSAGE_SELF` CPU, worker `THREAD_BASIC_INFO` user+system CPU, and whole-child
`TASK_EVENTS_INFO` deltas (`syscalls_unix` and `csw`, read first at each
acknowledged boundary and subtracted modulo $2^{32}$). No clock or counter
queries execute on the request path.

All server variants use byte-identical driver sources, `TCP_NODELAY`,
`-O2 -DNDEBUG` without LTO, a 20-ms warmup, and **2000-ms** closed-loop issuing
intervals. Five sequential groups alternate forward (`Poll` → `Before` →
`After`) and reverse (`After` → `Before` → `Poll`) execution order within one
window; each group runs the eight default scenarios followed by explicit
`--case health_idle512` and `--case health_idle2048`. Each summary cell is the
median of five per-run values, not a pooled percentile. Per-request CPU,
syscalls, and context switches divide each run's delta by its validated
completions before taking the median.

During Group 4's first attempt on `Poll` (`health_idle2048`), one client socket
hit `ECONNRESET` (`errno=54`) during the 2,049-connection burst setup before
warmup because `Poll` rebuilds and scans its connection vector while draining
the 16-entry listen backlog. That incomplete invocation was discarded and Group
4 `Poll` was re-executed cleanly; the audit entry is recorded in
[`2026-10-28-http-performance-evidence.json`](2026-10-28-http-performance-evidence.json).
Across the 15 complete ten-case server reports, **26,705,208 measured
responses** (**18,264,199** across the ten `Before` and `After` reports)
validated byte-for-byte with zero socket, wire, or deadline errors.

### Real-Socket Results

| Scenario | Poll req/s ↑ | Before (`08f85e9`) req/s ↑ | After (`HEAD`) req/s ↑ | After/Before throughput change ↑ | After/Poll throughput change ↑ |
| --- | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 33,359 | 32,551 | 34,741 | +6.7% | +4.1% |
| health_c8 | 96,980 | 89,614 | 95,784 | +6.9% | -1.2% |
| health_c64 | 104,816 | 97,590 | 107,397 | +10.0% | +2.5% |
| health_idle64 | 25,124 | 33,206 | 33,961 | +2.3% | +35.2% |
| health_pipeline16 | 449,921 | 454,027 | 475,998 | +4.8% | +5.8% |
| echo_1k_c1 | 32,547 | 32,237 | 33,598 | +4.2% | +3.2% |
| echo_1k_c8 | 93,926 | 91,975 | 94,715 | +3.0% | +0.8% |
| echo_1m_c1 | 3,863 | 3,668 | 3,906 | +6.5% | +1.1% |
| health_idle512 | 9,020 | 27,397 | 33,513 | +22.3% | +271.6% |
| health_idle2048 | 2,247 | 18,889 | 29,745 | +57.5% | +1,223.5% |

| Scenario | Poll worker CPU µs/req ↓ | Before worker CPU µs/req ↓ | After worker CPU µs/req ↓ | After/Before worker CPU change ↓ | After server process CPU µs/req ↓ | After client CPU µs/req ↓ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 11.04 | 10.22 | 9.58 | -6.2% | 9.58 | 11.47 |
| health_c8 | 8.99 | 9.27 | 8.53 | -8.0% | 8.53 | 9.29 |
| health_c64 | 8.62 | 8.77 | 7.70 | -12.2% | 7.70 | 8.58 |
| health_idle64 | 27.33 | 10.59 | 9.70 | -8.4% | 9.70 | 11.99 |
| health_pipeline16 | 0.90 | 0.84 | 0.79 | -5.5% | 0.79 | 0.77 |
| echo_1k_c1 | 11.55 | 10.51 | 10.19 | -3.0% | 10.19 | 12.24 |
| echo_1k_c8 | 9.31 | 9.29 | 8.68 | -6.5% | 8.68 | 9.41 |
| echo_1m_c1 | 160.43 | 173.94 | 158.39 | -8.9% | 158.39 | 154.08 |
| health_idle512 | 96.96 | 14.76 | 9.67 | -34.5% | 9.67 | 12.21 |
| health_idle2048 | 390.07 | 34.86 | 9.86 | -71.7% | 9.86 | 13.32 |

| Scenario | Poll syscalls/req ↓ | Before syscalls/req ↓ | After syscalls/req ↓ | Syscall change (After/Before) ↓ | Poll csw/req ↓ | Before csw/req ↓ | After csw/req ↓ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| health_c1 | 3.000 | 4.000 | 3.000 | -25.0% | 1.028 | 1.055 | 1.043 |
| health_c8 | 2.571 | 3.384 | 2.813 | -16.9% | 0.019 | 0.156 | 0.190 |
| health_c64 | 2.079 | 2.525 | 2.398 | -5.0% | 0.009 | 0.040 | 0.056 |
| health_idle64 | 3.000 | 4.000 | 3.000 | -25.0% | 1.046 | 1.060 | 1.029 |
| health_pipeline16 | 0.188 | 0.250 | 0.188 | -25.0% | 0.067 | 0.068 | 0.066 |
| echo_1k_c1 | 3.000 | 4.000 | 3.000 | -25.0% | 1.050 | 1.044 | 1.027 |
| echo_1k_c8 | 2.534 | 3.351 | 2.801 | -16.4% | 0.025 | 0.145 | 0.178 |
| echo_1m_c1 | 67.025 | 68.029 | 67.017 | -1.5% | 1.381 | 1.326 | 1.261 |
| health_idle512 | 3.000 | 4.000 | 3.000 | -25.0% | 0.085 | 1.046 | 1.047 |
| health_idle2048 | 3.001 | 4.000 | 3.000 | -25.0% | 0.182 | 1.047 | 1.049 |

| Scenario | Poll p99 µs ↓ | Before p50 / p95 / p99 µs ↓ | After p50 / p95 / p99 µs ↓ |
| --- | ---: | ---: | ---: |
| health_c1 | 53.67 | 25.04 / 36.62 / 58.12 | 23.88 / 34.88 / 54.96 |
| health_c8 | 109.54 | 59.29 / 89.50 / 154.96 | 51.92 / 80.83 / 111.08 |
| health_c64 | 968.58 | 315.54 / 603.88 / 989.88 | 267.75 / 460.12 / 739.38 |
| health_idle64 | 80.67 | 25.71 / 38.04 / 63.88 | 24.71 / 32.67 / 55.25 |
| health_pipeline16 | 72.04 | 29.58 / 44.67 / 81.83 | 28.46 / 37.96 / 67.67 |
| echo_1k_c1 | 63.00 | 25.96 / 38.54 / 61.75 | 25.50 / 33.75 / 55.79 |
| echo_1k_c8 | 129.25 | 58.71 / 90.50 / 144.29 | 52.88 / 83.08 / 120.83 |
| echo_1m_c1 | 515.83 | 226.04 / 342.54 / 592.04 | 210.67 / 318.88 / 478.04 |
| health_idle512 | 283.83 | 27.92 / 40.46 / 73.88 | 24.50 / 36.04 / 61.04 |
| health_idle2048 | 1,305.92 | 44.46 / 53.00 / 90.04 | 25.21 / 37.08 / 64.38 |

| Scenario | Poll req/s range ↑ | Before req/s range ↑ | After req/s range ↑ | Five paired After/Before throughput changes ↑ |
| --- | ---: | ---: | ---: | --- |
| health_c1 | 32,586–34,349 | 30,598–35,760 | 33,760–36,558 | +1.2%, +12.3%, -2.9%, +14.2%, +9.8% |
| health_c8 | 89,536–101,990 | 84,880–104,434 | 92,728–105,490 | +1.0%, +1.7%, +4.0%, +5.3%, +12.8% |
| health_c64 | 99,406–112,373 | 94,778–111,032 | 97,656–114,076 | +2.7%, +2.1%, +0.1%, +3.0%, +12.1% |
| health_idle64 | 23,744–27,355 | 31,183–34,068 | 33,184–36,630 | +1.6%, +3.5%, +10.3%, +13.4%, -0.9% |
| health_pipeline16 | 412,252–460,193 | 406,839–463,711 | 449,630–489,314 | +2.6%, +0.1%, +7.8%, +18.0%, +1.9% |
| echo_1k_c1 | 29,457–33,275 | 29,390–33,527 | 32,336–34,668 | +0.2%, +0.3%, +6.2%, +18.0%, +0.3% |
| echo_1k_c8 | 82,763–102,519 | 78,094–101,170 | 87,493–102,939 | +1.7%, +3.8%, +0.5%, +12.0%, +7.7% |
| echo_1m_c1 | 3,531–4,152 | 3,043–3,997 | 3,709–4,214 | +5.4%, +3.7%, +6.5%, +26.1%, +8.3% |
| health_idle512 | 8,874–9,678 | 25,392–29,675 | 32,166–36,012 | +17.3%, +17.4%, +21.4%, +25.7%, +29.1% |
| health_idle2048 | 2,016–2,564 | 18,665–20,727 | 29,745–32,301 | +66.5%, +59.4%, +55.5%, +56.7%, +57.5% |

#### Deterministic Event Count And Scaling Findings

1. **Exact one-syscall-per-exchange reduction on immediate send**:
   In `Before` (`08f85e9`), a single-connection unpipelined exchange (`health_c1`,
   `health_idle64`, `echo_1k_c1`, `health_idle512`, `health_idle2048`) executes
   exactly **4.000 Unix system calls per request**: one `kevent` read harvest,
   one `recv`, one `kevent` arming `EVFILT_WRITE` and harvesting write
   readiness, and one `send` (plus the second `kevent` disabling `EVFILT_WRITE`
   folded into the next read wait). In `After` (`HEAD`), immediate nonblocking
   send (KS-1) and the interest-transition short-circuit (KS-8) reduce this to
   **3.000 Unix system calls per request** (-25.0%): one `kevent` wait, one
   `recv`, and one immediate `send`, with zero `EVFILT_WRITE` registration or
   changelist traffic.
2. **Pipeline amortization**:
   On `health_pipeline16`, `Before` executes **0.250** syscalls/request
   ($4 / 16$: two `kevent` waits, one `recv`, one `send` per 16-request window),
   while `After` executes **0.188** syscalls/request ($3 / 16$, -25.0%).On
   `echo_1m_c1` (64×16 KiB `recv` calls plus `send` and `kevent`), syscalls
   drop by exactly one wait per 1 MiB message (**68.029 → 67.017**).
3. **Idle-connection scaling (`O(1)` management vs `O(N)` scans)**:
   In `After`, server worker CPU per request remains flat across idle counts:
   **9.58 µs** (`health_c1`), **9.70 µs** (`health_idle64`), **9.67 µs**
   (`health_idle512`), and **9.86 µs** (`health_idle2048`). In `Before`
   (`08f85e9`), scanning the live connection vector on every wakeup raises
   worker CPU to **14.76 µs** at 512 idle sockets (`After` is **-34.5%** CPU,
   **+22.3%** throughput, 5/5 pairs positive with non-overlapping ranges) and
   **34.86 µs** at 2,048 idle sockets (`After` is **-71.7%** CPU, **+57.5%**
   throughput, 5/5 pairs positive with non-overlapping ranges `18,665–20,727` vs
   `29,531–32,301`). In the normalized `Poll` control, rebuilding and kernel-copying
   the `pollfd` array drives worker CPU to **96.96 µs/req** at 512 idle
   connections (9,020 req/s) and **390.07 µs/req** at 2,048 idle connections
   (2,247 req/s); because each `poll` wakeup takes ~400 µs at 2,048 sockets, the
   client often deposits the next request before the server re-enters `poll`,
   lowering `Poll` context switches per request (`0.182`) only because the
   server is CPU-bound in user/kernel poll scans.
4. **Active-connection pairs and noise boundaries**:
   Eight of ten scenarios (`health_c8`, `health_c64`, `health_pipeline16`,
   `echo_1k_c1`, `echo_1k_c8`, `echo_1m_c1`, `health_idle512`,
   `health_idle2048`) show 5/5 positive paired throughput changes for `After`
   over `Before`, and all ten show lower median worker CPU (-3.0% to -71.7%) and
   lower median p50/p95/p99 latencies. However, on the eight active/small-idle
   cases (`health_c1` through `echo_1m_c1`), the 5-run throughput ranges still
   overlap (`health_c1` and `health_idle64` have 4/5 positive pairs). Only
   `health_idle512` and `health_idle2048` separate completely without range
   overlap, while the deterministic syscall reduction (4.000 → 3.000) holds
   invariant across every run.

### Read-Batch Send Experiment

Five alternating `health_c64` pairs (2000-ms issuing windows) compare the
production immediate-send loop (`HEAD`) against the disposable control that
defers sending until all returned read events in the batch have been processed:

| Scheduling | req/s ↑ | req/s range ↑ | Server worker CPU µs/req ↓ | Syscalls/req ↓ | Context switches/req ↓ | p50 / p95 / p99 µs ↓ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Immediate (`HEAD`) | 101,342 | 100,206–104,441 | 7.94 | 2.394 | 0.064 | 287.83 / 502.38 / 783.62 |
| After read batch | 103,249 | 92,440–104,687 | 7.96 | 2.389 | 0.062 | 282.71 / 489.79 / 760.38 |

Five paired batch/immediate throughput changes ↑: **-1.1%, -7.8%, +3.3%, +1.2%, +1.0%**.
All **2,038,015** measured responses validated with zero errors. Syscall counts
(2.394 vs 2.389 per request) and worker CPU (7.94 vs 7.96 µs/request) are
virtually identical, and paired throughput directions remain mixed with wide
range overlap. Deferring sends to the end of the read batch still shows no
repeatable advantage, so production retains immediate send.

## Full Parser Rerun

The 18 parser fixtures were re-executed in five alternating forward/reverse
groups (`October` → `08f85e9` → `HEAD` and reverse) using byte-identical
drivers and Release `-O2 -DNDEBUG` flags. Each table cell reports the median of
five per-run medians (each run takes the median of 11 warmed batches of at least
5 ms), followed by the five-run median range in parentheses:

| Fixture | October anchor (`34d5987`) ns/req ↓ | Pre-task (`08f85e9`) ns/req ↓ | Current (`HEAD`) ns/req ↓ | `HEAD` vs `08f85e9` time change ↓ | `HEAD` vs October time change ↓ |
| --- | ---: | ---: | ---: | ---: | ---: |
| head_long_values_16 | 315.36 (308.98–421.28) | 328.80 (321.52–347.98) | 312.62 (309.13–318.73) | -4.9% | -0.9% |
| head_short_fields_1000 | 7,970.13 (7,931.15–8,022.75) | 7,958.54 (7,909.63–7,982.87) | 7,995.73 (7,925.46–8,337.89) | +0.5% | +0.3% |
| head_ows_7800 | 408.01 (405.22–415.50) | 412.41 (408.34–413.66) | 416.83 (412.37–427.56) | +1.1% | +2.2% |
| head_ows_8_fields | 446.71 (436.18–450.54) | 459.10 (444.07–461.47) | 451.75 (446.76–459.16) | -1.6% | +1.1% |
| head_one_short_field | 52.40 (51.45–55.49) | 54.44 (53.17–56.32) | 54.09 (52.96–54.82) | -0.6% | +3.2% |
| head_minimal | 19.61 (19.14–20.08) | 20.88 (19.97–21.41) | 19.94 (19.76–20.05) | -4.5% | +1.7% |
| head_realistic_434 | 187.63 (185.62–190.70) | 189.91 (187.57–192.48) | 187.64 (186.69–189.64) | -1.2% | +0.0% |
| head_full_8192 | 5,148.48 (5,060.99–5,444.42) | 5,244.30 (5,175.05–5,385.70) | 5,183.35 (5,137.78–5,218.55) | -1.2% | +0.7% |
| head_decoder_whole | 224.06 (221.16–232.86) | 228.40 (225.91–233.27) | 235.19 (234.94–239.54) | +3.0% | +5.0% |
| head_decoder_feed17 | 821.92 (784.16–833.37) | 784.49 (780.78–801.47) | 792.54 (769.14–853.05) | +1.0% | -3.6% |
| echo_chunked_1024 | 175.89 (172.81–177.59) | 178.33 (177.05–179.52) | 177.62 (177.29–203.29) | -0.4% | +1.0% |
| body_1m_chunk1024 | 44,115.23 (43,236.00–45,360.02) | 44,218.42 (43,626.30–45,437.50) | 44,236.97 (43,445.31–46,010.38) | +0.0% | +0.3% |
| body_1m_chunk16384 | 30,925.62 (29,782.23–34,182.62) | 29,813.64 (29,078.12–30,320.15) | 29,303.71 (29,095.70–31,089.84) | -1.7% | -5.2% |
| body_1m_fixed_feed16384 | 45,104.49 (43,813.81–48,014.32) | 16,670.82 (16,636.15–16,891.44) | 16,910.24 (16,730.31–18,624.68) | +1.4% | -62.5% |
| extension_7000 | 1,959.65 (1,956.79–1,975.46) | 2,002.11 (1,975.94–2,028.61) | 1,971.78 (1,947.98–1,987.79) | -1.5% | +0.6% |
| trailer_7000 | 1,947.87 (1,929.64–1,957.69) | 1,963.68 (1,942.52–1,974.91) | 1,971.40 (1,954.40–2,170.86) | +0.4% | +1.2% |
| head_cr_dense | 155.88 (153.61–157.13) | 155.60 (153.52–156.93) | 156.65 (153.41–159.44) | +0.7% | +0.5% |
| respond_echo_chunked_1024 | N/A | 55.07 (53.65–55.48) | 54.21 (53.48–54.38) | -1.6% | N/A |

- **Fixed-body reserve (`ADR-0005`) remains intact**: `body_1m_fixed_feed16384`
  completes in **16.91 µs** (`HEAD`) and **16.67 µs** (`08f85e9`) versus
  **45.10 µs** on the October anchor (**-62.5%**), with ordinary allocations
  reduced from **11 calls (4,173,088 bytes)** to **4 calls (1,048,880 bytes)**.
- **Single-pass head policy (`KS-10`) impact on parser**: folding `Connection:
  close` into `detail::head_policy` adds no ordinary allocation call to any of
  the 18 fixtures. Each of the eight `RequestDecoder` fixtures (`head_decoder_whole`,
  `head_decoder_feed17`, `echo_chunked_1024`, `body_1m_chunk1024`,
  `body_1m_chunk16384`, `body_1m_fixed_feed16384`, `extension_7000`,
  `trailer_7000`) requests exactly **+8 bytes** once for `RequestDecoder::Impl`
  due to the padding-aligned `bool close` on `ParsedMessage`, with identical
  call counts and checksums across all 18 cases. Timing differences between
  `08f85e9` and `HEAD` stay within `-4.9%` to `+3.0%` with overlapping ranges.
- **Semantic equivalence**: the binary equivalence streams across `October`
  (`34d5987`), `08f85e9`, and `HEAD` (`17e571e`) are byte-for-byte identical
  (`cmp` exit 0): **95,480 semantic cases**, seed `0x53544F52`, **68,241,183
  bytes**, SHA-256
  `6a27c498cb9910a9d961ecc4795c9b045de4ee8314a23a8f8dff6dfb4eb2dbac`.

## Full Verification And Reproduction

- **Routed Debug CTest**: `cmake -S service -B service/build && cmake --build service/build && ctest --test-dir service/build --output-on-failure` — **12/12 Passed**.
- **Release Opt-In CTest (`HEAD`)**: **15/15 Passed**, including
  `http_bench_process_tests` (verifying distinct child PID, `clean_stop`
  idle-window 2..16 syscall and 1..16 context-switch bounds, `interrupted_child`,
  `invalid_transition`, and `ECHILD` single reaping) and `http_server_bench_cli`
  (verifying all eight default cases, `health_c1` $\ge 2$ syscalls/request floor,
  `health_pipeline16 < health_c1` per-request syscall amortization, explicit
  `health_idle512`/`health_idle2048` scaling fixtures, and exit-code 2 rejection
  matrix).
- **Control CTest Suites**: Release `Batch` control passes **14/14** applicable
  suites (excluding `http_server_scheduling_tests`, which pins immediate send);
  Release `Poll` control passes **13/13** applicable suites (excluding
  `http_server_dispatch_tests`, which asserts kqueue registration counts);
  Release `08f85e9` passes **11/11** suites.
- **Debug ASan/UBSan CTest**: **12/12 Passed** with
  `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1` (LeakSanitizer is
  unsupported on this Apple platform; no leak-detection claim is made).
- **Parser Smoke Gate & Debug Guard**: `http_parser_bench --smoke-scale 1`
  passes **18/18**; configuring `-DCMAKE_BUILD_TYPE=Debug -DORANGE_BUILD_BENCHMARKS=ON`
  fails fast as required.
- **Formatting, Workflow, And Governance**: `clang-format --dry-run --Werror`,
  `ruby -e 'require "yaml"; YAML.load_file(".github/workflows/http-performance.yml")'`,
  `npm test --prefix tools/governance-validator` (**303/303**), and
  `npm run validate --prefix tools/governance-validator` all pass.

All raw run JSON objects, SHA-256 binary and patch fingerprints, excluded-run
audit metadata, allocation tables, and verification logs are retained in
[`2026-10-28-http-performance-evidence.json`](2026-10-28-http-performance-evidence.json).
All disposable source reconstructions, temporary build directories, and binary
equivalence streams in `/tmp` and `service/build` were deleted before task
completion. Both retrospective ADRs (`docs/adr/ADR-0010-http-server-performance-measurement.md`
and `service/docs/adr/ADR-0008-kqueue-response-scheduling.md`) remain
`Proposed` / `Not Started` pending individual approval.
