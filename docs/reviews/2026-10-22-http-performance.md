# HTTP Transport Performance Work — 2026-10-22

## Measured Revisions

Every number in this report belongs to an explicit commit, unlike the
2026-10-16 report, which predates that convention:

- **Before (the 2026-10-16 version)**: `34d598738631ece0ef6023c154fbf3ae52520296`
  — the anchor commit carrying the October parser work.
- **After (the 2026-10-22 version)**: `ee42431`'s source tree — production
  sources byte-identical to the last optimization commit `5b404c3`; the bench
  driver additionally carries the cross-revision adaptation that lets one
  byte-identical driver compile against both revisions.
- Environment: Apple clang 21.0.0 (clang-2100.3.34.2), arm64, macOS 27.0.1,
  CMake 4.2.3, Release `-O2 -DNDEBUG`, no LTO, benchmark driver without IPO.

## Authorization And Scope

The repository owner authorized three optimization rounds in the performance
task chat on 2026-10-22: “先挑选一个执行优化，然后和现有的bench mark对比。在优化前，先将未提交的内容提交”,
then “补充这个ADR后，进行计划内的下一轮优化”, then a repeat of the first
instruction for the third round. These extend the scoped exception pattern
recorded in `docs/adr/INDEX.md` (2026-10-15/2026-10-16): implementation
precedes individual record approval, and every round is backfilled as a
retrospective Full ADR. The governing records are
`service/docs/adr/ADR-0005-fixed-body-reserve.md`,
`service/docs/adr/ADR-0006-static-rejection-wires.md`, and
`service/docs/adr/ADR-0007-append-form-response-serialization.md`, all
Accepted / Complete on the same day.

All three rounds preserve the HTTP grammar, error precedence, owned results,
pipeline boundaries, sticky failures, the independent 8 KiB head / 1 MiB
content / 8 KiB chunk-metadata limits, and the close-after-rejection
lifecycle. No dependency, target CPU intrinsic, build-flag, or LTO change was
introduced. Server socket throughput remains outside measurement.

## Extended Coverage Vs The 2026-10-16 Report

The October report measured parser latency over sixteen fixtures. The code
has since gained the response layer (the serializer first appears in
`87548eb`, 2026-10-19 — after the October anchor), so this report extends the
measured dimensions:

1. **Latency per fixture** (unchanged method): median of five per-run
   medians, alternating old/new executable order.
2. **Allocation calls and requested bytes per fixture** (headline evidence):
   `http_parser_allocations` runs the identical workloads in a separate
   executable.
3. **Response serialization** (new `respond` mode): covers the outbound half
   of the per-request path. This dimension has no October counterpart — the
   serializer did not exist at the anchor — so the fixture reports the
   current revision only.
4. **Transport feed size** (new `Case::feed`): fragmented delivery was fixed
   at 17 bytes; it is now parameterized, and 16 KiB mirrors the event loop's
   recv buffer. The fixed-body fixture uses it.
5. **Per-request path accounting** (analysis, not measurement): user-space
   body copies and heap allocations per echo request.

The fixture set grew sixteen to eighteen: `body_1m_fixed_feed16384` and
`respond_echo_chunked_1024`; the CLI contract count was pinned at each step.
The driver detects the linked revision (`__has_include` plus dependent
lookup), so the October build reports seventeen fixtures and the current
build eighteen from one byte-identical driver source.

## Results — 2026-10-16 vs 2026-10-22

Same-day paired comparison of the two measured revisions, five alternating
run pairs, median of per-run medians:

| Fixture | 10-16 (`34d5987`) | 10-22 (`ee42431`) | Change |
| --- | ---: | ---: | ---: |
| head_long_values_16 | 315.9 ns | 315.6 ns | -0.1% |
| head_short_fields_1000 | 7,954.6 ns | 7,981.2 ns | +0.3% |
| head_ows_7800 | 408.0 ns | 407.8 ns | -0.1% |
| head_ows_8_fields | 446.3 ns | 448.4 ns | +0.5% |
| head_one_short_field | 53.6 ns | 54.2 ns | +1.2% |
| head_minimal | 19.7 ns | 20.4 ns | +3.5% |
| head_realistic_434 | 187.7 ns | 188.6 ns | +0.5% |
| head_full_8192 | 5,127.4 ns | 5,245.9 ns | +2.3% |
| head_decoder_whole | 234.2 ns | 227.9 ns | -2.7% |
| head_decoder_feed17 | 794.4 ns | 782.3 ns | -1.5% |
| echo_chunked_1024 | 180.0 ns | 176.9 ns | -1.7% |
| body_1m_chunk1024 | 44,247.4 ns | 43,552.4 ns | -1.6% |
| body_1m_chunk16384 | 28,874.0 ns | 29,760.9 ns | +3.1% |
| **body_1m_fixed_feed16384** | **44,628.6 ns** | **17,512.8 ns** | **-60.8%** |
| extension_7000 | 1,978.1 ns | 1,964.6 ns | -0.7% |
| trailer_7000 | 1,957.4 ns | 1,962.5 ns | +0.3% |
| head_cr_dense | 153.8 ns | 154.2 ns | +0.2% |
| respond_echo_chunked_1024 | N/A — no serializer at the anchor | 53.9 ns | new dimension |

Per-run medians for the target fixture do not overlap: October
44,280–46,324 ns, current 16,553–18,767 ns. Every October fixture that the
rounds did not target stays within ±3.5% — noise and link-layout level, far
inside the ±15% retention guard; the fixtures' code paths are untouched by
the rounds, which is exactly why their drift bounds the measurement noise.

Allocation measurement on the same revisions: `body_1m_fixed_feed16384`
drops 11 to 4 ordinary calls and 4,173,088 to 1,048,872 requested bytes; all
other shared fixtures' counts and bytes are unchanged; result checksums agree
on every shared fixture.

Parser-equivalence streams are byte-identical across the two revisions:
95,480 semantic cases, fixed seed `0x53544F52`.

## Per-Round Attribution

Each round's own paired measurement (same-driver, against its immediate
parent state) attributes the cross-version totals:

| Round | Commits (red / green) | Change | Round-local target evidence |
| --- | --- | --- | --- |
| 1 — reserve the declared fixed body (ADR-0005) | `ae95ecc` / `b419a80` (+ format `9e96ef6`) | One reservation replaces geometric regrowth for fixed framing | 43,913 → 16,730 ns (-61.9%); 11 → 4 calls / 4,173,088 → 1,048,872 bytes |
| 2 — precomputed static rejection wires (ADR-0006) | `2f53c85` / `1b638ba` | Rejection path allocates nothing; six compile-time wires | Cold path, no latency claim; nine byte-equality pins against the serializer |
| 3 — append-form response serialization (ADR-0007) | `2be8eaa` / `5b404c3` | The per-response temporary string and its full-wire copy removed | 91.0 → 52.7 ns (-42.1%); 2 → 1 calls / 2,176 → 1,088 bytes |

Round 2 is invisible to the latency benchmark by design (rejections are a
cold path); its evidence is the byte-equality pin and the eliminated
allocation. Round 3's target fixture measures the exact per-request
serialization pattern the event loop performs.

## Per-Request Path Accounting (Analysis)

For one echo request through the event loop, user-space body copies and heap
allocations, derived from the source — not separately measured. "Before the
rounds" is the source state at `0c776ca`; the anchor revision predates the
response layer entirely.

| Stage | Before the rounds | After the rounds |
| --- | --- | --- |
| recv buffer → `incoming` | 1 copy | 1 copy (unchanged; a direct-feed path remains future work) |
| `incoming` → decoded body | 1 copy, geometric regrowth (~8 allocs per 1 MiB in 16 KiB feeds) | 1 copy, one reserved allocation for fixed framing |
| body → response wire | 2 copies (temporary string, then backlog append), 1 alloc | 1 copy straight into the backlog, 0 extra allocs |
| terminal rejection | validate + serialize + allocate | static wire, 0 allocs |

## Correctness And Risk Review

| Preserved contract | Executed evidence | Result |
| --- | --- | --- |
| Decoded bytes, consumed accounting, error precedence, pipeline suffix, reset | Equivalence streams byte-identical over 95,480 semantic cases across `34d5987` and `ee42431` | Pass |
| Fixed-body allocation bound and failure channel | Red `ae95ecc` (11 allocations observed failing the bound), green `b419a80` (4 allocations); fail-at-every-site injection returns `resource_exhausted` | Pass |
| Rejection wires byte-identical per `ParseError` | Red `2f53c85` (missing-API build failure), green `1b638ba` (60 checks, 0 failures) | Pass |
| Append-form equality, prefix preservation, unchanged-on-failure, fault injection | Red `2be8eaa` (missing-API build failure), green `5b404c3` (72 checks, 0 failures) | Pass |
| Socket-level serving lifecycle | Real-socket `http_server_tests` green at every green commit | Pass |
| Benchmark CLI contract | Seventeen-, then eighteen-fixture contract passes in the Release trees; Release ctest 9/9 at the After revision | Pass |

| Baseline risk dimension | Assessment and owning boundary | Result |
| --- | --- | --- |
| Concurrency and ordering | Single-threaded, connection-local buffers and decoders; ordering covered by pipeline and keep-alive cases in the routed suites | Pass |
| Timeout and deadline | N/A — no I/O, clock, or deadline behavior changed in any round | N/A |
| Cancellation and interruption | Sticky failure after `bad_alloc` verified at every allocation site of the reserved path; close-after-flush unchanged on all rejection paths | Pass |
| Resource bounds and backpressure | Reservation bounded by the pre-validated 1 MiB body cap; rejection path allocation-free; respond fixture allocations halved | Pass |
| Framework or trust-boundary rejection | Over-cap and malformed inputs keep their classifications (cross-revision equivalence corpus, decoder suites); serializer rejections unchanged (response suite) | Pass |

## Verification And Reproduction

- Method: the October revision was checked out as a detached worktree at
  `34d5987` and overlaid with the byte-identical current bench driver (the
  driver adapts to the revision through `__has_include` and dependent
  lookup); both trees built Release with the documented flags; five
  alternating run pairs; medians of per-run medians; unrelated load
  minimized. Per-round measurements used the same pairing against each
  round's parent state with the production patch reversed.
- Routed Debug trio at every green commit: 8/8 suites, zero warnings
  (`b419a80`, `1b638ba`, `5b404c3`). Release tree at the After revision:
  9/9 including the eighteen-fixture CLI contract.
- ASan/UBSan at the After revision: 8/8 with `ASAN_OPTIONS=detect_leaks=0
  UBSAN_OPTIONS=halt_on_error=1` (Apple ASan does not support LeakSanitizer
  here; no leak-detection claim).
- clang-format `--dry-run --Werror` clean on every touched C++ file;
  `git diff --check 501f4d1..HEAD` clean; governance validation exit 0 and
  validator suite 303/303 at each record change.
- Commit ledger: red `ae95ecc` / green `b419a80` / format `9e96ef6` (round 1);
  red `2f53c85` / green `1b638ba` (round 2); red `2be8eaa` / green `5b404c3`
  (round 3); driver adaptation `ee42431`; records `a9720e2`, `e93f0cd`,
  `4b3aa17`; approvals `c0493fe`, `3dd3bcb`, `acb1a50`, `5a16465`.
- All disposable build and comparison trees were deleted after retaining
  these results. Reproduce per `service/bench/README.md`; reconstruct the
  October baseline with a detached worktree at `34d5987` plus the current
  `service/bench/` overlaid verbatim.

## Known Limitations

- Each table is a same-day paired comparison; absolute latencies are not
  comparable across days or machines. The October report's own numbers were
  workspace-measured and not commit-anchored; this report anchors both ends,
  and future reports are expected to do the same.
- `respond_echo_chunked_1024` measures the serialization unit pattern, not a
  socket round trip; server throughput remains unmeasured.
- Round 2 has no latency claim by design.
- The ±3.5% drift on untouched fixtures bounds this machine's noise level;
  smaller effects are not resolvable with this method.
