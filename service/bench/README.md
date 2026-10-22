<!-- ADR: docs/adr/ADR-0008-http-parser-benchmarks.md -->
<!-- ADR: service/docs/adr/ADR-0004-owned-request-storage.md -->
<!-- Authorization: docs/reviews/2026-10-16-http-performance.md. -->
<!-- Owned storage: docs/reviews/2026-10-16-owned-request-storage.md. -->
# HTTP Parser Benchmarks

From the repository root, allocate and remove your own disposable directory:

```sh
orange_bench_dir=$(mktemp -d)
cmake -S service -B "$orange_bench_dir" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG' \
  -DORANGE_BUILD_BENCHMARKS=ON
cmake --build "$orange_bench_dir"
ctest --test-dir "$orange_bench_dir" --output-on-failure
"$orange_bench_dir/http_parser_bench"
"$orange_bench_dir/http_parser_allocations"
cmake -E rm -rf "$orange_bench_dir"
```

The opt-in target is `http_parser_bench`. Both the library and driver use the
same Release flags. Debug benchmark configurations are rejected. Benchmark
code is compiled separately from production code without IPO on the driver;
do not add whole-program LTO when comparing these results. The JSON checksum
observes each result and is emitted after timing. Fixture construction and
one validation pass occur outside timing. Each measured request includes
decoder construction (where applicable), result materialization, observation
and destruction. `head_*` without `decoder` uses `parse_request`; message and
decoder cases use fresh decoders, not capacity-retaining reset loops.

Seventeen fixtures cover an 18-byte minimal head; a 434-byte, nine-field head;
an 8,192-byte, 409-field head; the same 434-byte head through a whole-input
decoder and 17-byte feeds; a 1 KiB chunked echo; 1 MiB content in 1 KiB and
16 KiB HTTP chunks; 1 MiB fixed Content-Length content in 16 KiB transport
feeds; a 7,000-byte token extension; a 7,000-byte trailer value;
and an 8 KiB malformed CR-dense head. HTTP chunk size and transport feed size
are different variables. Five storage diagnostics add 16 long values, 1,000
short fields, 7,800 OWS octets, eight OWS-padded fields, and one short field.
All successful fixtures fit the existing budgets.

`http_parser_allocations` runs the identical workloads in a separate executable,
reporting ordinary `operator new` calls and total requested bytes per request,
including applicable decoder/body allocations. Input generation and JSON output
are outside counting; timing has no counting replacement. This metric is not
peak live memory, allocator overhead, RSS, or a count of arbitrary aligned/system
allocations. Request `sizeof` is also reported. A decoder is fresh on each run;
steady-state pool reuse is not inferred. Both executables reject invalid fixture
results, and CTest checks their reports agree on fixture identities.

For revision equivalence, compile the byte-identical
`http_parser_equivalence.cpp` and workload header against each revision. Redirect
each executable's **binary stdout** to its own disposable file and run `cmp` on
the two files. Stderr reports the case count and fixed seed. The stream contains
all request tokens, ordered fields, body bytes, errors, consumed bytes and first
feed progress. The corpus includes every capped fixture prefix, seeded
mutations/truncations/suffixes and arbitrary byte strings. It compares observed
protocol semantics; allocation-failure injection is covered separately by
`http_request_storage_tests`. The adapters permit this experiment's old field
members and new read-only accessors without modifying either parser revision.
See the [storage experiment](../../docs/reviews/2026-10-16-owned-request-storage.md)
for the exact source patch, run evidence, tradeoffs and reconstruction procedure.

Calibration warms each path and doubles iterations until a batch takes at
least 5 ms. The report contains 11 repetitions' median/minimum/maximum ns per
request, iteration count, wire/content bytes, compiler, flags and architecture.
`--quick` uses three repetitions and 1 ms batches for CLI verification, not
performance claims. For comparisons, use the same driver and fixtures, record
CPU/OS/compiler/source revisions, alternate old/new executable order, and
compare several runs' medians and ranges with unrelated workloads stopped.
Numbers measure single-threaded hot-cache parsing and owned allocation, not
network/server throughput. They are not portable latency guarantees.

No latency gate runs by default. `--smoke-scale 1` opts into deliberately loose
per-case limits (reported as `smoke_limit_ns`) to detect magnitude regressions.
A positive scale multiplies those limits for a separately calibrated machine;
non-finite/non-positive values or unknown/missing arguments exit 2. A fixture
or requested latency failure exits 1; success exits 0. This CLI contract is
exercised by `http_parser_bench_cli`, including forced gate rejection.

The macOS CI workflow uses Apple clang `-O2`, correctness tests, the loose
magnitude gate, and a separate ASan/UBSan run. JSON is retained in its logs and
job summary; compiler output is discarded. Initial limits include generous
hardware/runner headroom and require calibration from actual runner history;
they do not claim sensitivity to small regressions. Runner-image or compiler
changes require fresh comparison before tightening limits. No precise
cross-machine performance threshold is inferred from a local measurement.
Apple ASan does not support LeakSanitizer here; the workflow explicitly
disables that unsupported option and makes no leak-detection claim.

Measurement guidance: [Google Benchmark user guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md).
Runner specification: [GitHub-hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
