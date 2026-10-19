<!-- Owner-authorized request-storage optimization; see Authorization below. -->
# Owned Request Storage Experiment

## Authorization And Scope

On 2026-10-16, the owner answered the current chat's explicit scope question:

> 本次也豁免，直接实施并记录证据

The question explicitly covered the ADR acceptance and serialization gates for
owned head storage with offset descriptors, field-access API migration,
allocation and lifetime tests, A/B benchmarks and evidence. This authorization
is confined to this experiment. It does not accept or finish ADR-0003 or
ADR-0007, change their index rows, or authorize an arena, pool, architecture
intrinsics, dependency, server integration, commit, push or deployment.

The primary implementation boundary is the HTTP parser's owned request value.
Supporting decoder, framing, tests and benchmark adaptations follow that value's
field-access API. Wire grammar, caps, error precedence, order, duplicate fields,
received case, trimmed values and consumed bytes remain unchanged.

## Design And Retention Criteria

The candidate stores owned characters and compact field offsets. Dense heads
use one bulk copy; text that fits the string's existing inline capacity, or
occupies at most one quarter of the wire head, is packed from validated slices
to avoid retaining discarded padding. Accessors
derive views from the current request, without retaining pointers to caller
input or to the request's previous address. Copies own independent storage;
moves leave the source safely empty. Views expire with the owning request or
when it is assigned/moved. Fragmented input reuses the existing validated-offset
collector. No maximum-sized inline buffer is added to every request.

Before production changes, a focused allocation test must fail on the existing
implementation: 16 long fields must require at most two ordinary heap
allocations for a complete head, including long request-line components.
Incomplete and rejected heads must allocate no result storage. Existing short
heads without fields retain zero allocations on the declared host library.

Correctness gates cover input destruction, copy/move construction and assignment,
self-assignment, decoder reset with outstanding results, field semantics,
every existing parser suite, and ASan/UBSan. Allocation failure at each owned
construction step must return `resource_exhausted` at parser/decoder boundaries.

The allocation reporter is separate from the timing executable so counter
branches do not affect timed allocations. Both share identical fixtures and
result observations. The same driver is compiled separately against the old
and new libraries with Apple clang, C++20, `-O2 -DNDEBUG`, without LTO. Alternate
execution order over five pairs, each using 11 calibrated samples; compare the
median of each side's five run medians. Include the existing eleven cases,
16 long values, 1,000 short fields, long OWS padding, eight padded fields and
one short field. Target at least 15%
less latency for 16 long values; no existing/diagnostic case may exceed 110%
of baseline. Unexpected repeatable regressions require revision or rejection,
not relaxed thresholds. These are local experiment criteria, not portable
latency contracts or a tighter CI smoke gate.

## Execution Evidence

The candidate is retained: all correctness checks and all 16 predeclared
performance criteria pass. This execution does not change any ADR/ADD status.

- Baseline: HEAD `1f876464862c69c5a54424d91fbfd6937983e23c` plus the existing
  dirty working tree at the start of this task, not bare HEAD. The source
  [reverse patch](2026-10-16-owned-request-storage-source.patch) reconstructs
  the four prior parser files byte-for-byte; reconstruction was checked.
- Full [machine-readable evidence](2026-10-16-owned-request-storage-evidence.json)
  retains before/after source SHA-256 fingerprints, byte-identical driver
  fingerprints, all ten timing runs, allocations and environment metadata.
- Host: Apple clang 21.0.0 (`clang-2100.3.34.2`), arm64, macOS 27.0.1,
  C++20, `-O2 -DNDEBUG -Wall -Wextra`, separate compilation, no LTO.
  CPU model is unavailable in the sandbox; no model is inferred. These are
  local hot-cache measurements, not service/network throughput guarantees.
- TDD red: the long-name/value/request-line fixture allocated 35 times and
  failed the at-most-two criterion (exit 1). The direct parser also failed
  both injected `resource_exhausted` outcome checks before translation was
  implemented. Green: the same long fixture allocates twice and all focused
  checks pass. The simpler benchmark with 16 long values goes from 17 to 2.
- The initial whole-head-copy candidate regressed the 7,800-OWS case by
  21.1%. It was rejected under the original threshold. A focused test then
  failed its at-most-one-allocation OWS requirement; selective packing fixes
  both the allocation test and latency regression without relaxing budgets.

The final five-pair median-of-medians results are below. Requested bytes are
cumulative allocation requests, not peak live memory or allocator overhead.

| Fixture | Before → after ns | Change | Allocation calls | Requested bytes |
| --- | --- | --- | --- | --- |
| `head_long_values_16` | 550.6 → 291.6 | -47.0% | 17 → 2 | 2,944 → 2,312 |
| `head_short_fields_1000` | 11,411.2 → 7,650.4 | -33.0% | 1 → 2 | 48,000 → 16,024 |
| `head_ows_7800` | 380.0 → 386.1 | +1.6% | 1 → 1 | 48 → 8 |
| `head_ows_8_fields` | 393.2 → 422.5 | +7.4% | 1 → 2 | 384 → 96 |
| `head_one_short_field` | 48.2 → 50.3 | +4.2% | 1 → 1 | 48 → 8 |
| `head_minimal` | 23.0 → 18.3 | -20.3% | 0 → 0 | 0 → 0 |
| `head_realistic_434` | 229.7 → 178.1 | -22.5% | 2 → 2 | 632 → 512 |
| `head_full_8192` | 6,768.7 → 4,900.4 | -27.6% | 2 → 2 | 20,056 → 11,472 |
| `head_decoder_whole` | 272.2 → 212.4 | -22.0% | 3 → 3 | 896 → 744 |
| `head_decoder_feed17` | 784.3 → 724.3 | -7.7% | 9 → 9 | 2,512 → 2,360 |
| `echo_chunked_1024` | 161.3 → 166.9 | +3.5% | 3 → 4 | 1,344 → 1,320 |
| `body_1m_chunk1024` | 41,288.1 → 41,343.3 | +0.1% | 13 → 14 | 2,112,816 → 2,112,792 |
| `body_1m_chunk16384` | 26,757.6 → 27,237.3 | +1.8% | 9 → 10 | 2,082,096 → 2,082,072 |
| `extension_7000` | 1,772.3 → 1,783.2 | +0.6% | 2 → 3 | 312 → 288 |
| `trailer_7000` | 1,751.7 → 1,770.4 | +1.1% | 2 → 3 | 312 → 288 |
| `head_cr_dense` | 144.3 → 144.6 | +0.2% | 0 → 0 | 0 → 0 |

`sizeof(Request)` decreases from 96 to 64 bytes on this host. Persistent field
descriptors occupy eight bytes each instead of two 24-byte strings. The
complete parser's existing bounded stack scratch remains 16 KiB. There is no
new per-connection maximum-sized array, arena or pool. The fragmented collector
still has its existing bounded, growing descriptor vector and head buffer;
its allocations are included in the corresponding decoder fixture.

The 434-byte repository fixture already allocated twice before this change;
it is not the third party's seven-allocation fixture. It still allocates twice,
but requests fewer bytes and avoids per-field string construction. Conversely,
1,000 short fields now allocate twice instead of once, while latency decreases
33.0% and requested bytes fall from 48,000 to 16,024. Chunked-message cases add
one small character allocation; the 1 KiB echo median increases 3.5%. Eight
OWS-padded fields increase 7.4%, the largest observed regression, within the
predeclared 10% budget. No universal zero-allocation or 20% speedup is claimed.

## Contract And Risk Review

| Contract | Exact requirement | Passing evidence |
| --- | --- | --- |
| OR-1 | Result bytes survive input overwrite/destruction and decoder reset | `check_copies`, `check_owned_fields`, `check_reset`, ASan/UBSan |
| OR-2 | Copy/move construction and assignment preserve all slices; moved-from values are empty; self-assignment is safe | `check_copies`, `check_moves`, `check_copy_failure` |
| OR-3 | Complete head materialization uses at most two ordinary allocations, independent of long field count; unfinished/rejected input allocates no result | `check_allocations`, existing `check_parse_allocations`, separate allocation report |
| OR-4 | Grammar, caps, order, duplicates, case, OWS, body and consumed bytes stay identical for the same input/feed schedule | 95,479 input/feed records with byte-identical complete semantic streams; existing suites |
| OR-5 | Parser/decoder allocation failures return `resource_exhausted`; decoder errors remain terminal | `check_failures` injects both head-storage steps and all three whole-decoder construction steps |
| OR-6 | Long-value median ≤85% of baseline; every other diagnostic/existing median ≤110% | 16/16 final A/B comparisons pass |
| OR-7 | Field views borrow from the owning Request, never caller input; iterators also require their range object | `Request` / `HeaderField` API comments and lifetime tests; no stored views in the owning representation |

| Baseline risk dimension | Applicability and owner | Verification and result |
| --- | --- | --- |
| Concurrency and ordering | Field order and successive decoder lifecycles apply; request value / decoder own these. Threads are not introduced. | OR-1/2/4 pass, including outstanding results during reset and duplicate fields |
| Timeout and deadline | N/A — synchronous bounded in-memory parser has no timer, wait, I/O or deadline policy | Existing head/body/metadata caps and semantics remain covered by OR-4 |
| Cancellation and interruption | Decoder reset and interrupted/incomplete input apply; decoder owns the transition. No new async cancellation API. | OR-1/3/4 pass for reset, all prefixes and fragment splits |
| Resource bounds and backpressure | Bounded head storage and allocation failures apply; request and decoder own memory. Transport backpressure is unchanged. | OR-3/5 pass; 8 KiB head limit bounds offsets, fields and character allocation; body/metadata limits unchanged |
| Framework or trust-boundary rejection | Untrusted HTTP octets apply; parser/framing own rejection. No new framework. | OR-4/5 pass for errors/caps and resource exhaustion |

All 95,479 corpus records compare stateless head parsing, complete-message
parsing and two-feed incremental progress. Binary streams include every token,
field byte, body byte, error and consumed count (66,143,697 bytes per revision).
Both optimized streams and the sanitized candidate stream are identical:
SHA-256 `88e1a83e8a654a57f0b3657cc9097f60c82a694965ca28a5c2a1551e8d855f63`.
The seed is `0x53544F52`. Records include repeated fixtures and prefixes; they
are not claimed to be distinct random inputs. Allocation-failure outcomes are
verified separately, since the direct parser previously let `bad_alloc` escape.

A disposable mutation replaced strong copy assignment with sequential member
assignment. `check_copy_failure` then failed exactly its complete-value
preservation check (exit 1), demonstrating that the test detects mismatched
character storage and offsets. The mutation never touched the working tree.
Ordinary C++ value copies retain their prior standard-container allocation
failure behavior; parse entry points translate allocation failure to error
values, and copy assignment preserves the destination on failure.

Queueing, locks, connection pools, downstream services, network I/O and GC are
N/A to this single-threaded parser experiment. No server or traffic shape was
invented to infer an end-to-end bottleneck or throughput improvement.

## API Migration And Structured Review

`Request` now exposes read-only `method()`, `target()`, `version()` and
`headers()` accessors. `headers()` is sized, indexable and iterable; each
`HeaderField` contains views into the request's owned buffer. Saving a field
independently of the request now requires an explicit string copy. Copying a
Request still deep-copies its owned storage. The source API changes, while the
HTTP wire contract does not. All repository consumers in framing, tests and
benchmarks have been adapted. No input or connection arena controls returned
request lifetime.

Stable implementation touchpoints are `orange::http::Request` (private storage,
validated construction, accessors and value operations), `parse_request`,
`detail::HeadFields::finish`, and `detail::body_framing`. The existing compact
`HeaderFieldOffsets` / `RequestLineOffsets` declarations move into the request
header so both complete and incremental construction use the same types.
No second grammar implementation or public unchecked constructor is added.

The four changed production files have 458, 193, 31 and 184 physical lines.
The largest existing affected test file has 719 lines; the new lifetime suite
has 209. The longest affected production function (`parse_request`) has 43
non-comment code lines; the largest new benchmark function (`fixtures`) has
48. Executable nesting stays below the baseline cap. Complexity is
`N/A — no configured complexity tool`; span and nesting were manually reviewed.
No engineering size exception is required. Descriptor counts and offsets stay
bounded by the same 8 KiB wire cap. Ownership uses standard RAII containers.

## Verification And Reproduction

The existing `service/build` belongs to prior work and was preserved. All
verification used owned disposable directories under
`/private/tmp/orange-owned-head-3YdwHv` with the same CMake/CTest command forms.
Compiler outputs, reconstructed sources, mutation executables and binary
comparison streams are deleted before completion; only reports and the source
patch remain.

- Optimized: `cmake -S service -B <dir> -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_CXX_COMPILER=clang++ '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG'
  -DORANGE_BUILD_BENCHMARKS=ON`, `cmake --build <dir>`,
  `ctest --test-dir <dir> --output-on-failure`: 6/6 pass.
- Sanitizers: configure a separate Debug directory with
  `'-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'`,
  build, then `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
  ctest --test-dir <dir> --output-on-failure`: 5/5 pass. Apple LeakSanitizer
  is unsupported and was explicitly disabled; no leak-detection claim is made.
- Differential: both `http_parser_equivalence` binaries exit 0 and `cmp`
  reports equality. A separate `clang++ -std=c++20 -O1 -g -Wall -Wextra
  -fsanitize=address,undefined -fno-omit-frame-pointer -Iservice/src` compile
  of `http_parser_equivalence.cpp` plus the four production translation units
  also exits 0; its binary stream compares equal without sanitizer findings.
- `npm test --prefix tools/governance-validator`: 303/303 pass.
  `npm run validate --prefix tools/governance-validator`: pass.
- `clang-format --dry-run --Werror` on every changed/new C++ source and
  `git diff --check`: pass.

To reconstruct the old library without changing the checkout, copy only source,
tests, benchmarks and CMake configuration into a fresh disposable directory:

```sh
orange_repo_dir=$PWD
orange_compare_dir=$(mktemp -d)
mkdir "$orange_compare_dir/before"
cp -R service/src service/tests service/bench "$orange_compare_dir/before/"
cp service/CMakeLists.txt "$orange_compare_dir/before/"
(cd "$orange_compare_dir/before" && git apply --reverse -p2 \
  "$orange_repo_dir/docs/reviews/2026-10-16-owned-request-storage-source.patch")
cmake -S "$orange_compare_dir/before" -B "$orange_compare_dir/before-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG' -DORANGE_BUILD_BENCHMARKS=ON
cmake --build "$orange_compare_dir/before-build" --target \
  http_parser_bench http_parser_allocations http_parser_equivalence
cmake -S service -B "$orange_compare_dir/after-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG' -DORANGE_BUILD_BENCHMARKS=ON
cmake --build "$orange_compare_dir/after-build"
ctest --test-dir "$orange_compare_dir/after-build" --output-on-failure
```

The restored library uses the old API, so build only the three comparison
targets there; current correctness tests intentionally use the new API. Run the
two timing executables alternately five times and compare each case's median
of run medians. Run both allocation reporters. Redirect both equivalence
targets' stdout to separate files and compare with `cmp`. Finally remove
`$orange_compare_dir` with `cmake -E rm -rf`.
The patch is tied to the source fingerprints above; later parser changes
require deliberately selecting a new baseline rather than forcing it to apply.

Remote CI orchestration was not run. No commit, push, deployment, dependency,
HTTP limit, arena or pool was introduced; unrelated changes were preserved.
