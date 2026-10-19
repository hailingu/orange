# HTTP Parser Performance Work

## Authorization And Scope

The repository owner authorized the sequence benchmark, metadata optimization,
conditional fragmented-head optimization, and CI. In this chat the owner then
explicitly extended the exception: “本次四项也豁免，直接实施并记录证据”. This
waives ADR preapproval and serialization for these four changes only. It does
not accept or finish the existing runtime or fixture-lifecycle proposals.
The existing authorized task branch and unrelated work are preserved.

This work preserves HTTP grammar, error precedence, owned results, pipeline
boundaries, sticky failures, and the independent 8 KiB head / 1 MiB content /
8 KiB aggregate chunk-metadata limits. It adds no application/library dependency
or server behavior; CI uses a pinned checkout action.
The parser remains single-threaded and synchronous; deadlines, cancellation,
socket backpressure, TLS, and server throughput are outside this measurement.

## Predetermined Verification

- The optional Release benchmark validates every fixture before measuring,
  reports compiler/options, iterations, repetitions, median and range, and
  exits nonzero for an invalid invocation, fixture, or requested smoke limit.
  Minimal/realistic/full heads, decoder fragmentation, echo, 1 MiB chunked
  content, long extensions/trailers, and malformed CR-dense input are covered.
- Span chunk-line validation must match the existing byte API's result,
  consumed boundary and decoded size across splits, malformed inputs, and
  quota boundaries. Existing public decoder tests must continue to pass.
- Retain metadata optimization only with a measured improvement exceeding
  run-to-run noise on relevant fixtures and no repeatable >15% regression on
  minimal/realistic complete heads or large-content decoding.
- Consider fragmented-head changes only after measuring their baseline cost.
  Preserve field case/order/duplicates, leading empty lines, trimmed values,
  transient-input ownership, exact head-cap errors and allocation handling.
  Retain only if fragmented-head cost improves beyond run-to-run noise.
- Run optimized tests, ASan/UBSan, clang-format checks, governance validation
  and diff checks. Delete task-owned compiler output after retaining results.
- CI uses Apple clang on macOS, runs correctness before timing, and applies
  deliberately loose magnitude checks. Reported latency is a parser-only hot
  cache measurement, not network requests per second or an SLO.

## Results

The optional `http_parser_bench` target now has eleven validated fixtures and a
CTest CLI contract check. `ChunkLine::feed(string_view)` scans runs in one
grammar state; delimiters still use the existing byte transitions and numeric
accumulation rule. `RequestDecoder::Impl::read_metadata` clips input to the
remaining metadata budget before inspecting it. Trailers scan token/value
runs, and a complete data CRLF advances together when both bytes are present.

`detail::HeadFields` stores compact validated offsets for fragmented heads.
`finish` materializes owned results without reparsing the complete buffered
head. Both paths reuse `validate_request_line`, `validate_field_line` and
`copy_header_block`; no second grammar implementation or borrowed public view
was introduced. The complete-first-fragment fast path remains direct. The
initial speculative incomplete parse and exceptional cap-classification parse
remain; this is not a claim that every head byte is always visited only once.

The final comparison rebuilt byte-identical benchmark driver source against
both libraries, using Apple clang 21.0.0, arm64, macOS 27.0.1, `-O2 -DNDEBUG`,
without LTO. Baseline means the original working sources including earlier
uncommitted repairs, not HEAD alone. Five pairs alternated executable order;
each run contains eleven warmed samples per fixture, at least 5 ms per batch.
Values below are the median of the five per-run medians. CPU brand lookup was
denied by the local sandbox; no claim about another machine follows.

| Fixture | Before | After | Latency change |
| --- | ---: | ---: | ---: |
| Minimal 18-byte head | 23.8 ns | 24.4 ns | +2.7% |
| 434-byte / nine-field head | 252.8 ns | 252.0 ns | -0.3% |
| 8 KiB / 409-field head | 7.167 µs | 7.257 µs | +1.3% |
| Whole head through decoder | 289.1 ns | 294.6 ns | +1.9% |
| Same head in 17-byte feeds | 936.2 ns | 838.8 ns | -10.4% |
| 1 KiB chunked echo | 181.4 ns | 167.4 ns | -7.7% |
| 1 MiB / 1 KiB HTTP chunks | 52.653 µs | 44.051 µs | -16.3% |
| 1 MiB / 16 KiB HTTP chunks | 29.193 µs | 29.061 µs | -0.5% |
| 7,000-byte token extension | 24.888 µs | 1.958 µs | -92.1% |
| 7,000-byte trailer value | 17.562 µs | 1.952 µs | -88.9% |
| Malformed CR-dense 8 KiB head | 155.0 ns | 154.9 ns | -0.1% |

Raw repetitions' min/median/max, iterations, summaries, source hashes and exact
command templates are retained in
[`2026-10-16-http-performance-evidence.json`](2026-10-16-http-performance-evidence.json).
Small changes near zero are not treated as proven gains. Metadata improvement
and fragmented-head improvement exceed observed run variation; there is no
repeatable >15% regression in the protected fixtures. The optimization is
retained. The chunk-size gap is reduced, not eliminated: chunk count still
changes copying/append bookkeeping as well as metadata work.

Memory was inspected with clang record-layout output: `RequestDecoder::Impl`
grows from 232 to 264 bytes. Each descriptor is eight bytes; fragmented heads
initially reserve sixteen descriptors (128 bytes), growing with received fields
up to 2,048 slots / 16 KiB under the existing head limit. Complete first
fragments allocate no descriptor storage. No maximum-sized per-connection
array is added. This is a bounded memory tradeoff for removing repeated scans,
not a claim of zero-cost fragmentation or zero decoder allocations.

## Correctness And Risk Review

| Preserved contract | Executed evidence | Result |
| --- | --- | --- |
| Head grammar, leading empty lines, case/order/duplicates, OWS and ownership | Existing head tests, `check_head_boundaries`, new `check_owned_fields`, `check_rejections`, `check_many_fields` | Pass |
| Chunk extension grammar, quoted escapes, numeric quota and exact stopping cursor | New byte/span comparisons over every octet in extension states and multiple fragment sizes; independent literal hex/quota cases; existing `check_metadata_octets` and `check_chunked` | Pass; 96,889 span assertions |
| Independent 8 KiB head / 1 MiB content / 8 KiB metadata limits and rejection classification | Existing `check_head_boundaries`, `check_metadata_limits`, `check_aggregate_metadata`, `check_joint_chunk_budgets`; new offset-budget regression | Pass |
| Pipeline suffix, transient input, sticky failures and reset | Existing `check_fragments`, `check_decoder_lifecycle`, `check_allocation_failure`, seeded binary roundtrips | Pass |
| Benchmark reproducibility and failure reporting | CLI JSON checks, invalid CLI exit 2, forced smoke rejection exit 1, eleven real smoke cases, Debug benchmark configuration rejection | Pass |

| Baseline risk dimension | Assessment and owning boundary | Result |
| --- | --- | --- |
| Concurrency and ordering | Single-threaded decoder remains connection-local; interleaved instances, fragmented grammar transitions and pipeline cursor tests execute it | Pass |
| Timeout and deadline | N/A — no production I/O, clock, wait or deadline changes; CI job has a ten-minute limit | N/A |
| Cancellation and interruption | No new cancellation mechanism; existing sticky error/reset and allocation-failure tests cover interrupted decoding | Pass |
| Resource bounds and backpressure | Parser bounds are checked before span inspection; exact/over-cap and joint-budget tests pass. No socket backpressure changes | Pass |
| Framework or trust-boundary rejection | Actual parser APIs reject invalid octets, framing ambiguity and malformed line endings; no test bypasses the wire grammar | Pass |

New public/internal interfaces have intent-bearing documentation. Production
files remain below the 600-line review threshold, new executable units below
60 code lines, and inspected new nesting at most four levels. Cyclomatic
complexity: N/A — no configured complexity tool. No application/library dependency
or external HTTP contract changes, target CPU intrinsics, unbounded reads, or unrelated
refactoring were introduced. The only AGENTS.md change adds the mandatory
routing row for the new CI platform and its owner-authorization reference.

## Verification And Reproduction

- Optimized CMake/CTest: 5/5; zero compiler warnings.
- ASan/UBSan: 4/4. `detect_leaks=1` initially aborted because Apple ASan does
  not support LeakSanitizer on this platform; the supported checks pass with
  `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`. No leak-detection
  result is claimed.
- Governance suite: 303/303; repository governance validation exit 0.
- clang-format check, workflow YAML parse and `git diff --check`: exit 0.
- Test-first evidence: missing benchmark CLI, missing span/head APIs, and the
  over-cap offset regression were observed failing before their implementations.
  Span behavior passed before its run-scanning refactor was integrated.

Current benchmark reproduction commands and CLI semantics are in
[`service/bench/README.md`](../../service/bench/README.md).
To reconstruct the measured baseline, use a disposable copy of this final
source tree and reverse
[`2026-10-16-http-performance-source.patch`](2026-10-16-http-performance-source.patch)
with `git apply --reverse`. Configure Release with the same flags and build
only `--target http_parser_bench`; new API tests belong to the final
implementation and do not compile against the old API. The reverse patch was
checked and applied to a disposable copy: every baseline source reconstructed
byte-for-byte. Hashes identify contents; they do not claim that earlier
uncommitted sources are present in HEAD or stored Git objects.

The new macOS Actions workflow runs optimized correctness, the loose magnitude
guard and sanitizers. It uses read-only repository permission, an immutable
checkout action, no persisted checkout credentials and disposable build output.
Timing JSON goes to logs and the job summary. The runner label and action
revision were checked against [GitHub's runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)
and [checkout release](https://github.com/actions/checkout/releases/tag/v7.0.1).
Its YAML and equivalent local verification commands passed. Actual remote
Actions orchestration has not run; there was no commit, push or deployment.

All task-owned compiler output is deleted after retaining these reports. The
preexisting `service/build` and unrelated worktree changes are preserved.
