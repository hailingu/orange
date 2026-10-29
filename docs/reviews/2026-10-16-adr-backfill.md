# HTTP Performance And Storage ADR Backfill

## Request And Authority

On 2026-10-16, owner @hailingu requested “补充文件缺失的 ADR” after the
performance and owned-request-storage experiments. This task adds the missing
decision records and their file references, not another parser optimization.

The owner had explicitly waived ADR acceptance and serialization for the four
performance items (“本次四项也豁免，直接实施并记录证据”) and then for owned head
storage, field-access migration, allocation/lifetime tests, A/B measurements and
evidence (“本次也豁免，直接实施并记录证据”). Those scopes authorize the completed
historical implementations and this retrospective documentation. They do not
supply an exact `Approve` for any new record.

At the time of backfill, each record was **Proposed / Not Started** in the
formal lifecycle, with actual owner-waived implementation and test results
recorded separately. Subsequent individual approval events below update all five
backfilled records. Existing project ADR-0003, ADR-0007 and ADD statuses
are unchanged. No new
standing exception, product-demand candidate, commit, push or deployment is
introduced.

## Individual Approval Follow-Up

### ADR-0008: Benchmark Tooling

At 2026-10-16T09:10:58Z, the exact `Approve` supplied by the previously
self-identified @hailingu was registered for
`docs/adr/ADR-0008-http-parser-benchmarks.md`. The immediately preceding chat
identified this one path. The earlier uppercase batch message was not used as
approval evidence. ADR-0008 became Accepted / In Progress; at that event the
other four backfilled records remained Proposed / Not Started.

This update changes approval/lifecycle metadata and evidence only. The existing
benchmark implementation and acceptance criteria are unchanged. All 22 recorded
source/configuration fingerprints were rechecked and still match. T-1 and the
three acceptance checks have their existing passing evidence; formal completion
still awaits the immutable commit anchor already required by the touchpoint
rows. No commit or new source implementation is part of this registration.
Accepted-stage `npm test --prefix tools/governance-validator` passes 303/303;
`npm run validate --prefix tools/governance-validator` and `git diff --check`
also pass. C++ tests are not rerun for this metadata-only registration because
all 22 source/configuration fingerprints remain identical to their tested snapshots.
The evidence JSON retains its original draft fingerprints and appends this
approval event instead of rewriting that historical snapshot.

### Service ADR-0002: Chunk Metadata Advancement

At 2026-10-16T09:20:11Z, the exact `Approve` supplied by the previously
self-identified @hailingu was registered for
`service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md`. The immediately
preceding chat identified this one path. The preceding uppercase `APPROVE`
was not counted as approval evidence. This record became Accepted / In Progress;
at that event ADR-0008 retained its prior approval, and the other three backfilled
records remained Proposed / Not Started.

Only approval/lifecycle metadata, inactive archival guidance and evidence are
updated. The scope, contracts, risks, checks and implementation are unchanged.
All 22 source/configuration fingerprints and four historical evidence hashes
still match. T-1 and AC-1/AC-2/AC-3 retain passing implementation evidence;
formal completion still awaits the immutable commit anchor already required
by the touchpoint rows. No commit or source implementation is part of this
registration. Structured review confirms the eligible actor, exact one-record
approval, timestamp, requirement levels, status/index agreement and preservation
of the accepted decision. The evidence JSON appends a separate lifecycle event
while retaining the original draft snapshot and earlier approval event.

Accepted-stage `npm test --prefix tools/governance-validator` passes 303/303;
`npm run validate --prefix tools/governance-validator` and `git diff --check`
also pass. C++ tests and A/B timings are not rerun for this documentation-only
registration: source/configuration fingerprints and acceptance conditions are
unchanged, so the previously recorded Release, sanitizer and paired-measurement
results remain the implementation evidence. Task-owned document snapshots and
the disposable governance TAP output are removed after retaining this summary.

### Service ADR-0003: Fragmented Request Heads

At 2026-10-16T09:30:41Z, the exact `Approve` supplied by the previously
self-identified @hailingu was registered for
`service/docs/adr/ADR-0003-fragmented-head-offsets.md`. The immediately preceding
chat identified this one path. This record became Accepted / In Progress;
at that event ADR-0008 and Service ADR-0002 retained their prior approvals,
and the remaining two backfilled records stayed Proposed / Not Started.

Only approval/lifecycle metadata, inactive archival guidance and evidence are
updated. All 22 source/configuration fingerprints and four historical evidence
hashes still match. The scope, contracts, risks, acceptance checks and source
implementation are unchanged; T-1 and AC-1/AC-2/AC-3 retain their recorded
passing implementation evidence. Formal completion still requires the immutable
commit anchor declared by the touchpoint rows. No commit is part of this
registration. Structured review confirms the eligible actor, exact one-record
approval, timestamp, requirement levels, status/index agreement and preservation
of the decision. The evidence JSON appends this event while retaining the
original draft snapshot and previous approval events.

Accepted-stage `npm test --prefix tools/governance-validator` passes 303/303;
`npm run validate --prefix tools/governance-validator` and `git diff --check`
also pass. C++ tests and A/B timings are not rerun for this documentation-only
registration because source/configuration fingerprints and acceptance conditions
are unchanged. Prior Release, sanitizer and paired-measurement results remain
the implementation evidence. Task-owned document snapshots and disposable TAP
output are removed after retaining this summary.

### ADR-0009: HTTP Parser Verification CI

At 2026-10-16T09:36:20Z, the exact `Approve` supplied by the previously
self-identified @hailingu was registered for
`docs/adr/ADR-0009-http-parser-verification-ci.md`. The immediately preceding
chat identified this one path. This record became Accepted / In Progress;
at that event the prior three approvals were unchanged, and only Service ADR-0004
among the five backfilled records remained Proposed / Not Started.

This registration updates approval/lifecycle metadata, inactive archival
guidance and evidence only. All 22 source/configuration fingerprints and four
historical evidence hashes still match. The scope, contracts, risks, acceptance
checks, workflow and AGENTS routing are unchanged. T-1 and local AC-1/AC-2
retain their passing evidence. Hosted AC-3 and all five applicable hosted risk
observations remain Not Started, and formal completion also requires the
declared immutable commit anchor. No push, dispatch or remote run is part of
this registration; approval does not authorize the actions excluded by Scope.
Structured review confirms the eligible actor, exact one-record approval,
timestamp, requirement levels, index agreement and truthful local/hosted evidence.
The evidence JSON appends this event without rewriting earlier snapshots.

Accepted-stage `npm test --prefix tools/governance-validator` passes 303/303;
`npm run validate --prefix tools/governance-validator` and `git diff --check`
also pass. YAML, C++, sanitizer and timing checks are not rerun for this
documentation-only registration because the workflow, source/configuration
fingerprints and acceptance conditions are unchanged. Prior local results remain
local implementation evidence; no hosted outcome is inferred. Task-owned document
snapshots and disposable TAP output are removed after retaining this summary.

### Service ADR-0004: Owned Request Storage

At 2026-10-16T09:39:23Z, the exact `Approve` supplied by the previously
self-identified @hailingu was registered for
`service/docs/adr/ADR-0004-owned-request-storage.md`. The immediately preceding
chat identified this one path. This record, including its existing narrow
ordinary Request value-copy exception, is now Accepted / In Progress. All five
backfilled records have now received individual approval; their earlier events
and unrelated record statuses are preserved.

This registration updates approval/lifecycle metadata, inactive archival
guidance and evidence only. All 22 source/configuration fingerprints and four
historical evidence hashes still match. The exception's scope, owner, controls
and review condition are unchanged; its lifecycle wording now reflects acceptance.
All other scope, contracts, risks, checks and implementation remain unchanged.
T-1/T-2 and AC-1 through AC-5 retain passing implementation evidence; formal
completion still requires the declared immutable commit anchor. No commit or
source change is part of this registration. Structured review confirms the
eligible actor, exact one-record approval, timestamp, requirement levels,
exception acceptance and status/index agreement. The evidence JSON appends this
event without rewriting the original draft snapshot or previous approval events.

Accepted-stage `npm test --prefix tools/governance-validator` passes 303/303;
`npm run validate --prefix tools/governance-validator` and `git diff --check`
also pass. C++, sanitizer, allocation/equivalence and A/B timing checks are not
rerun for this documentation-only registration because the implementation,
source/configuration fingerprints and acceptance conditions are unchanged.
Prior results remain the implementation evidence. Task-owned document snapshots
and disposable TAP output are removed after retaining this summary.

## Added Records

| Record | Complete outcome | Primary boundary | Routing |
| --- | --- | --- | --- |
| [ADR-0008](../adr/ADR-0008-http-parser-benchmarks.md) | Reproducible Release benchmark and diagnostics with tested CLI/status semantics | Parser verification tooling and build/CLI interface | Project: opt-in build configuration |
| [Service ADR-0002](../../service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md) | Bounded chunk/trailer runs with equivalent grammar, quotas and cursor | Decoder metadata advancement | Service internal |
| [Service ADR-0003](../../service/docs/adr/ADR-0003-fragmented-head-offsets.md) | Fragmented-head completion from validated offsets | Decoder head validation/completion | Service internal |
| [ADR-0009](../adr/ADR-0009-http-parser-verification-ci.md) | Disposable macOS correctness, magnitude and sanitizer pipeline | Repository verification orchestration | Project: CI and scope routing |
| [Service ADR-0004](../../service/docs/adr/ADR-0004-owned-request-storage.md) | Independently owned Request values with bounded complete-head allocations | Parser owned Request value | Service internal; all API consumers are inside service |

Benchmarks and CI are separate records because they can be reviewed and
reverted independently. Metadata advancement, head collection and Request
representation likewise have separate outcomes. Full ADRs make the build,
pipeline, internal state/memory and source API decisions explicit. No new
record duplicates the still-unfinished broader runtime task.

The storage ADR records a narrow engineering exception for ordinary
Request value copies throwing `bad_alloc`, with strong assignment preservation.
Parser entry points retain coded `resource_exhausted` errors. This describes
the implemented standard-container copy behavior and its tests. The backfill
originally proposed the exception; the individual Service ADR-0004 approval
recorded above now accepts it. The header's introductory error-policy comment
distinguishes ordinary value copies from coded parser allocation errors.

## File Coverage And Source Identity

Twenty-two existing source, test, build, workflow and guide files gain leading
ADR references. All changes in those files are confined to their leading
comments. Existing repair/report references remain in place. Their executable
and configuration bodies are byte-identical to the pre-backfill snapshot.
The central index gains one row per record and the scoped authorization note.

[Machine-readable evidence](2026-10-16-adr-backfill-evidence.json) enumerates all
22 paths, before/after SHA-256 identities and comment-only verification. The
new ADRs list their implementation touchpoints, affected paths, contract/check
mapping, the five baseline risks and acceptance/completion conditions. This
report and its JSON are evidence, not substitutes for those decision records.
Unrelated work, including existing editor files and governance implementations,
was preserved.

Historical measurement JSON and the two parser source patches are unchanged.
Their fingerprints describe the measured source snapshots, not these later
comment edits. The separate
[provenance patch](2026-10-16-adr-provenance.patch) captures only this task's
leading-comment changes, including the clarified Request error-policy comment.
In a disposable copy containing the listed files, reconstruct earlier parser
sources in this order, from that copy's root:

```sh
git apply --reverse docs/reviews/2026-10-16-adr-provenance.patch
git apply --reverse docs/reviews/2026-10-16-owned-request-storage-source.patch
# Apply the next reversal only when comparing the earlier parser optimization.
git apply --reverse docs/reviews/2026-10-16-http-performance-source.patch
```

The first reversal restores all 22 pre-backfill files byte-for-byte. The second
restores all ten parser fingerprints recorded for the storage baseline. The
third restores all nine parser Git blob IDs recorded before the earlier
performance optimization. These reconstructions were actually executed in a
disposable copy, without touching the working sources. Hashes identify content;
they do not claim the old dirty workspace exists in HEAD or as stored Git objects.

For a new A/B measurement, compile one byte-identical driver against each
chosen parser snapshot and compare the same workload identities. The earlier
performance report used eleven cases; the current shared driver includes those
shapes plus five storage diagnostics. Historical driver fingerprints and raw
samples remain historical evidence; fresh timings are fresh measurements, not
an assertion that the old timing sample stream or earlier driver revision has
been recreated. Source reconstruction above concerns parser contents.

## Structured Review

| Review dimension | Result and evidence |
| --- | --- |
| Classification, numbering and routing | Five Full ADR identities are unique in their own roots; project records own build/CI, service records own internal parser decisions; central index rows match metadata |
| One complete task and implementation boundary | Each record has one primary boundary and one or two coupled subtasks; independently reversible outcomes are split |
| Requirement levels and lifecycle | All required sections retained; conditional fields have an explicit trigger assessment; no approval, retirement, rejection or completion is fabricated |
| Authority and scope | Exact earlier owner waivers and current backfill request are recorded; they are not promoted into a general governance rule or exact Approve |
| Acceptance checks | Every check maps to one declared subtask and names concrete input, executable/deterministic method, exact result and evidence; historical experiment criteria are distinguished from retrospective ADR drafting |
| Contracts and risks | Every Decision clause has explicit check coverage; all five baseline dimensions assessed; CI hosted risks stay Not Started rather than claiming local tests prove Actions behavior |
| Ownership, failure and API | Storage record covers Request-owned text, borrowed accessor/range lifetime, copy/move/reset, strong failed-copy preservation, coded parser allocation errors and the narrow value-copy exception, now individually accepted with Service ADR-0004 |
| Performance interpretation | Local paired measurements and per-case retention budgets are retained without network-throughput claims; fragmented-head ranges refer to per-run medians, whose ranges separate, while noisy individual samples can overlap |
| Evidence and reconstruction | Original raw JSON and source patches retained; new comment fingerprints and reversal patch restore both measured parser baselines |
| Implementation changes in this task | Only leading comments changed in existing code/config/guide files; no production behavior or build/workflow body changed; therefore no artificial test-first defect was introduced |

## Executed Verification

All commands ran from the repository root. Disposable CMake directories were
allocated beneath the task-owned `/private/tmp/orange-adr-backfill-*` root;
pre-existing `service/build` was not modified.

- `npm test --prefix tools/governance-validator`: **303/303 pass**.
- `npm run validate --prefix tools/governance-validator`: **pass**, including
  new record metadata, template sections, status/index and referenced ADR paths.
- Release: `cmake -S service -B "$orange_adr_dir/release"
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
  '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG' -DORANGE_BUILD_BENCHMARKS=ON`,
  `cmake --build "$orange_adr_dir/release" --parallel 3`,
  `ctest --test-dir "$orange_adr_dir/release" --output-on-failure`:
  **6/6 pass**, no compiler warnings.
- Debug with `-fsanitize=address,undefined -fno-omit-frame-pointer` and matching
  linker sanitizer flags; build/CTest with `ASAN_OPTIONS=detect_leaks=0
  UBSAN_OPTIONS=halt_on_error=1`: **5/5 pass**. Apple LeakSanitizer remains
  unsupported; no leak-detection result is inferred.
- Separate default-OFF and rejected-Debug-ON CMake configurations: default
  omits benchmark targets; Debug ON rejects with the declared diagnostic.
- Real CLI probes: unknown option, missing scale and scales `0`, `-1`, `nan`,
  `inf` all exit **2**; the CTest forced tiny gate exits **1**.
- `http_parser_bench --smoke-scale 1`: **16/16 pass**, eleven samples each;
  allocation reporter also exits 0. This checks magnitude, not small gains.
- Current `http_parser_equivalence`: **95,479 records**, **66,143,697 bytes**,
  SHA-256 `88e1a83e8a654a57f0b3657cc9097f60c82a694965ca28a5c2a1551e8d855f63`,
  identical to the previously retained old/new semantic stream fingerprint.
- `ruby -e 'require "yaml"; YAML.load_file(".github/workflows/http-performance.yml")'`:
  exit 0. `clang-format --dry-run --Werror` on all 17 affected C++ files and
  `git diff --check`: pass.
- Actual **remote Actions orchestration was not run**. ADR-0009 AC-3 and its
  hosted risk observations remain Not Started, with exact future evidence
  requirements. No timing A/B experiment was repeated for comment-only edits.

All task-owned compiler/test output, comparison streams and temporary source
copies are deleted after retaining these evidence summaries and fresh smoke /
allocation JSON. No reusable binary is retained.
