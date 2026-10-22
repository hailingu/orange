# HTTP Transport Performance Work — 2026-10-22

## Authorization And Scope

The repository owner authorized three optimization rounds in the performance
task chat on 2026-10-22: “先挑选一个执行优化，然后和现有的bench mark对比。在优化前，先将未提交的内容提交”,
then “补充这个ADR后，进行计划内的下一轮优化”, then a repeat of the first
instruction for the third round. These extend the scoped exception pattern
recorded in `docs/adr/INDEX.md` (2026-10-15/2026-10-16): implementation
precedes individual record approval, and every round is backfilled as a
retrospective Full ADR. The governing records are
`service/docs/adr/ADR-0005-fixed-body-reserve.md` (Accepted / Complete),
`service/docs/adr/ADR-0006-static-rejection-wires.md` (Accepted / Complete),
and `service/docs/adr/ADR-0007-append-form-response-serialization.md`
(Proposed at this writing; its numbers are the historical exception execution
cited there).

All three rounds preserve the HTTP grammar, error precedence, owned results,
pipeline boundaries, sticky failures, the independent 8 KiB head / 1 MiB
content / 8 KiB chunk-metadata limits, and the close-after-rejection
lifecycle. No dependency, target CPU intrinsic, build-flag, or LTO change was
introduced. Server socket throughput remains outside measurement.

## Extended Measurement Dimensions

The 2026-10-16 report measured parser latency only. This round set extends
coverage because the work added capabilities:

1. **Latency per fixture** (unchanged method): median of five per-run medians,
   alternating old/new executable order.
2. **Allocation calls and requested bytes per fixture** (now headline
   evidence): `http_parser_allocations` runs the identical workloads in a
   separate executable; both reservation rounds are allocation-motivated, so
   counts are reported beside latency.
3. **Response serialization** (new `respond` mode): the benchmark now covers
   the outbound half of the per-request path, not just parsing; the driver
   adapts to the linked library revision through a dependent-lookup template,
   keeping one byte-identical driver source for both revisions.
4. **Transport feed size** (new `Case::feed`): fragmented delivery was fixed
   at 17 bytes; it is now parameterized, and the 16 KiB value mirrors the
   event loop's recv buffer.
5. **Per-request path accounting** (analysis, not measurement): user-space
   body copies and heap allocations per echo request, before and after the
   three rounds.

The fixture set grew sixteen to eighteen: `body_1m_fixed_feed16384` (round 1)
and `respond_echo_chunked_1024` (round 3); the CLI contract count was pinned
at each step.

## Predetermined Verification

- Every new or changed behavior lands test-first: the failing check is
  observed before its implementation (red commits below).
- Retention rule inherited from the October work: keep an optimization only
  with a measured improvement beyond run-to-run noise on the target fixture
  and no repeatable >15% regression on any pre-existing fixture.
- Rejection wires must stay byte-identical to the serializer's output for
  every `ParseError` value.
- Append-form serialization must equal the owning form byte for byte,
  preserve existing destination content, and leave the destination unchanged
  on validation or allocation failure.
- Correctness suites, sanitizer suites, governance validation, and formatting
  checks all pass; disposable build trees are deleted after retaining results.

## Results

### Round 1 — reserve the declared fixed body length (ADR-0005)

`start_body` now reserves the exact, cap-bounded Content-Length once instead
of regrowing geometrically across transport fragments. Apple clang 21.0.0
(clang-2100.3.34.2), arm64, macOS 27.0.1, `-O2 -DNDEBUG`, no LTO; five
alternating pairs; values are the median of the five per-run medians.

| Fixture | Before | After | Change |
| --- | ---: | ---: | ---: |
| head_long_values_16 | 310.7 ns | 313.3 ns | +0.8% |
| head_short_fields_1000 | 7,940.9 ns | 7,911.4 ns | -0.4% |
| head_ows_7800 | 407.7 ns | 406.5 ns | -0.3% |
| head_ows_8_fields | 445.5 ns | 450.7 ns | +1.2% |
| head_one_short_field | 52.8 ns | 52.7 ns | -0.2% |
| head_minimal | 19.3 ns | 19.4 ns | +0.4% |
| head_realistic_434 | 187.7 ns | 183.1 ns | -2.4% |
| head_full_8192 | 5,173.9 ns | 5,067.9 ns | -2.0% |
| head_decoder_whole | 223.7 ns | 222.6 ns | -0.5% |
| head_decoder_feed17 | 795.1 ns | 787.0 ns | -1.0% |
| echo_chunked_1024 | 178.8 ns | 177.6 ns | -0.7% |
| body_1m_chunk1024 | 44,065.8 ns | 43,752.6 ns | -0.7% |
| body_1m_chunk16384 | 29,165.4 ns | 29,460.4 ns | +1.0% |
| **body_1m_fixed_feed16384** | **43,913.4 ns** | **16,729.9 ns** | **-61.9%** |
| extension_7000 | 1,943.2 ns | 1,955.8 ns | +0.6% |
| trailer_7000 | 1,957.7 ns | 1,941.9 ns | -0.8% |
| head_cr_dense | 154.5 ns | 151.9 ns | -1.7% |

Per-run medians for the target fixture do not overlap: baseline 42,553–47,129
ns, optimized 16,257–16,981 ns. Allocation measurement on the same pair:
`body_1m_fixed_feed16384` drops 11 to 4 ordinary calls and 4,173,088 to
1,048,872 requested bytes; all other fixture counts are unchanged and
checksums agree on all seventeen fixtures.

Memory-profile disclosure: a declared length is now committed up front, still
bounded by the existing 1 MiB body cap per message.

### Round 2 — precomputed static rejection wires (ADR-0006)

Terminal rejections are answered from six `constexpr` wires via `error_wire`,
and the event loop's rejection, handler-failure, and serializer-fallback
paths append them directly. This is a cold path by design, so no latency
fixture claims a win; the evidence is structural: nine byte-equality pins
against `write_response(error_response(...))` (one per `ParseError` value),
the elimination of a file-local duplicate 500 wire, and a rejection path that
cannot allocate beyond the outgoing buffer's own growth.

### Round 3 — append-form response serialization (ADR-0007)

`write_response_into` serializes straight into the caller's buffer;
`write_response` delegates to it, and `queue_response` no longer builds a
per-response temporary or copies it wholesale. Same machine, flags, and
pairing method as round 1.

| Fixture | Before | After | Change |
| --- | ---: | ---: | ---: |
| head_long_values_16 | 307.3 ns | 314.1 ns | +2.2% |
| head_short_fields_1000 | 7,953.0 ns | 7,931.2 ns | -0.3% |
| head_ows_7800 | 406.3 ns | 410.0 ns | +0.9% |
| head_ows_8_fields | 446.4 ns | 449.0 ns | +0.6% |
| head_one_short_field | 53.6 ns | 54.2 ns | +1.1% |
| head_minimal | 19.2 ns | 19.6 ns | +1.7% |
| head_realistic_434 | 183.4 ns | 183.0 ns | -0.2% |
| head_full_8192 | 5,069.8 ns | 5,090.9 ns | +0.4% |
| head_decoder_whole | 221.9 ns | 223.4 ns | +0.7% |
| head_decoder_feed17 | 756.3 ns | 768.0 ns | +1.5% |
| echo_chunked_1024 | 176.5 ns | 176.4 ns | -0.0% |
| **respond_echo_chunked_1024** | **91.0 ns** | **52.7 ns** | **-42.1%** |
| body_1m_chunk1024 | 43,461.9 ns | 43,217.8 ns | -0.6% |
| body_1m_chunk16384 | 28,407.9 ns | 28,621.3 ns | +0.8% |
| body_1m_fixed_feed16384 | 16,678.8 ns | 16,570.7 ns | -0.6% |
| extension_7000 | 1,939.2 ns | 1,947.8 ns | +0.4% |
| trailer_7000 | 1,937.8 ns | 1,942.7 ns | +0.2% |
| head_cr_dense | 153.3 ns | 153.7 ns | +0.2% |

Per-run medians for the target fixture do not overlap: baseline 88.9–98.6 ns,
optimized 52.4–53.1 ns. Allocation measurement: `respond_echo_chunked_1024`
drops 2 to 1 calls and 2,176 to 1,088 requested bytes; all other counts
unchanged; checksums agree on all eighteen fixtures.

### Per-request path accounting (analysis)

For one echo request through the event loop, user-space body copies and heap
allocations, derived from the source — not separately measured:

| Stage | Before the three rounds | After |
| --- | --- | --- |
| recv buffer → `incoming` | 1 copy | 1 copy (unchanged; a direct-feed path remains future work) |
| `incoming` → decoded body | 1 copy, geometric regrowth (~8 allocs per 1 MiB in 16 KiB feeds) | 1 copy, one reserved allocation for fixed framing |
| body → response wire | 2 copies (temporary string, then backlog append), 1 alloc | 1 copy straight into the backlog, 0 extra allocs |
| terminal rejection | validate + serialize + allocate | static wire, 0 allocs |

## Correctness And Risk Review

| Preserved contract | Executed evidence | Result |
| --- | --- | --- |
| Decoded bytes, consumed accounting, error precedence, pipeline suffix, reset | `http_parser_equivalence` streams byte-identical over 95,480 semantic cases (round 1 pair) | Pass |
| Fixed-body allocation bound and failure channel | Red `ae95ecc` (11 allocations observed failing the bound), green `b419a80` (4 allocations); fail-at-every-site injection returns `resource_exhausted` | Pass |
| Rejection wires byte-identical per `ParseError` | Red `2f53c85` (missing-API build failure), green `1b638ba` (60 checks, 0 failures) | Pass |
| Append-form equality, prefix preservation, unchanged-on-failure, fault injection | Red `2be8eaa` (missing-API build failure), green `5b404c3` (72 checks, 0 failures) | Pass |
| Socket-level serving lifecycle | Real-socket `http_server_tests` green at every green commit | Pass |
| Benchmark CLI contract | Seventeen-, then eighteen-fixture contract passes in the Release trees | Pass |

| Baseline risk dimension | Assessment and owning boundary | Result |
| --- | --- | --- |
| Concurrency and ordering | Single-threaded, connection-local buffers and decoders; ordering covered by pipeline and keep-alive cases in the routed suites | Pass |
| Timeout and deadline | N/A — no I/O, clock, or deadline behavior changed in any round | N/A |
| Cancellation and interruption | Sticky failure after `bad_alloc` verified at every allocation site of the reserved path; close-after-flush unchanged on all rejection paths | Pass |
| Resource bounds and backpressure | Reservation bounded by the pre-validated 1 MiB body cap; rejection path allocation-free; respond fixture allocations halved | Pass |
| Framework or trust-boundary rejection | Over-cap and malformed inputs keep their classifications (equivalence corpus, decoder suites); serializer rejections unchanged (response suite) | Pass |

## Verification And Reproduction

- Environment: Apple clang 21.0.0 (clang-2100.3.34.2), arm64, macOS 27.0.1,
  CMake 4.2.3; Release `-O2 -DNDEBUG`; benchmark driver without IPO.
- Method: for each measured round, a disposable copy of the final source tree
  with exactly that round's production patch reversed served as baseline; the
  benchmark driver source was byte-identical across the pair (revision
  adaptation via dependent lookup); five alternating run pairs; medians of
  per-run medians; unrelated load minimized.
- Routed Debug trio at every green commit: 8/8 suites, zero warnings
  (`b419a80`, `1b638ba`, `5b404c3`). Release tree for round 3: 9/9 including
  the eighteen-fixture CLI contract.
- ASan/UBSan at HEAD `5b404c3`: 8/8 with `ASAN_OPTIONS=detect_leaks=0
  UBSAN_OPTIONS=halt_on_error=1` (Apple ASan does not support LeakSanitizer
  here; no leak-detection claim).
- clang-format `--dry-run --Werror` clean on every touched C++ file;
  `git diff --check 501f4d1..HEAD` clean; governance validation exit 0 and
  validator suite 303/303 at each record change.
- Commit ledger: red `ae95ecc` / green `b419a80` / format `9e96ef6` (round 1);
  red `2f53c85` / green `1b638ba` (round 2); red `2be8eaa` / green `5b404c3`
  (round 3); records `a9720e2`, `e93f0cd`, `4b3aa17`; approvals `c0493fe`,
  `3dd3bcb`, `acb1a50`.
- All disposable build and comparison trees were deleted after retaining
  these results. Reproduce any round per `service/bench/README.md`.

## Known Limitations

- Numbers are single-machine, same-day, hot-cache comparisons; each round's
  baseline is its own parent state, so cross-round and cross-day absolute
  latencies are not comparable.
- Response serialization is measured at the unit level (the exact per-request
  pattern), not through sockets; server throughput remains unmeasured.
- Round 2 has no latency claim by design.
- The October report's absolute numbers predate these changes and describe
  that day's machine state; use today's README procedure for fresh numbers.
