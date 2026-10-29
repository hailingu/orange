# Decision Record Index

This is the repository-wide index of project and service ADRs and OCRs,
including archived records, under the existing AGENTS.md index contract.
The repository-relative path is the record identity. Rows are retained when
status or storage changes; ADR and OCR number sequences remain separate.

## Task-Scoped Serialization Authorization

On 2026-10-15, the repository owner explicitly authorized a temporary
serialization waiver in the current quality-repair chat to prepare separate
records for findings 1, 2, 4 and 5 alongside the unfinished runtime record.
The authorization retains individual record approval before implementation.
It applies only to the four repair paths added below; it neither accepts
them nor changes the general AGENTS.md policy. Original user instruction:

> 允许本次暂时豁免 ADR 串行限制，继续为第 1、2、4、5 点逐项准备修复 ADR，仍逐份审批后实现。

On 2026-10-16, the same owner explicitly extended the acceptance and
serialization exception to the four performance changes (“本次四项也豁免，直接实施并记录证据”)
and then to the scoped owned-request-storage experiment (“本次也豁免，直接实施并记录证据”).
The later request “补充文件缺失的 ADR” asks to backfill those decisions. The five
new records below cover exactly those earlier scopes, with separate outcomes
for benchmarks, metadata advancement, fragmented heads, CI and owned storage.
They were drafted as retrospective Proposed / Not Started records, with
historical exception execution recorded separately. On 2026-10-16, @hailingu
supplied the exact `Approve` in the sole context of
`docs/adr/ADR-0008-http-parser-benchmarks.md`; that record is now Accepted /
In Progress pending its declared immutable commit anchor. The same owner later
supplied exact `Approve` in the sole context of
`service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md`; that record is also
Accepted / In Progress pending its declared immutable commit anchor. The owner
then supplied exact `Approve` in the sole context of
`service/docs/adr/ADR-0003-fragmented-head-offsets.md`; that record is now also
Accepted / In Progress pending its declared immutable commit anchor. The owner
then supplied exact `Approve` in the sole context of
`docs/adr/ADR-0009-http-parser-verification-ci.md`; it is now Accepted / In Progress
with hosted verification and its declared immutable commit anchor outstanding.
The owner finally supplied exact `Approve` in the sole context of
`service/docs/adr/ADR-0004-owned-request-storage.md`; it is now Accepted /
In Progress pending its declared immutable commit anchor. All five backfilled
records have received individual approval. This scoped backfill
does not extend a standing waiver, change the runtime/fixture records, or
claim remote CI execution. Authorization, evidence
and file coverage are collected in `docs/reviews/2026-10-16-adr-backfill.md`.

On 2026-10-21, the pre-declared immutable commit anchor for all five backfilled
records was bound to `34d598738631ece0ef6023c154fbf3ae52520296`, the commit that
carries the implemented sources, their record markers, and the five records.
`docs/adr/ADR-0008-http-parser-benchmarks.md`,
`service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md`,
`service/docs/adr/ADR-0003-fragmented-head-offsets.md`, and
`service/docs/adr/ADR-0004-owned-request-storage.md` moved to `Complete` with
fresh HEAD verification recorded in each record.
`docs/adr/ADR-0009-http-parser-verification-ci.md` also received its anchor but
remains `In Progress`: its hosted AC-3 verification and the five applicable
hosted risk observations require an authorized remote Actions run that has not
occurred.

On 2026-10-22, the same owner instructed in the performance task chat
“先挑选一个执行优化，然后和现有的bench mark对比。在优化前，先将未提交的内容提交”,
and then “补充这个ADR后，进行计划内的下一轮优化”. Under the same exception
pattern as above, the fixed-body reservation was implemented directly
(red `ae95ecc`, green `b419a80`, formatting `9e96ef6`) and the follow-up
optimization round was authorized. This authorization permits drafting the
retrospective records alongside non-terminal ADRs; it neither accepts them —
individual approval is still required — nor changes the general policy.
The reservation record is `service/docs/adr/ADR-0005-fixed-body-reserve.md`.
The follow-up round served terminal rejections from precomputed static wires
(red `2f53c85`, green `1b638ba`); its record is
`service/docs/adr/ADR-0006-static-rejection-wires.md`.
On 2026-10-22, @hailingu supplied the exact `Approve` in the task chat and, in
the required one-record clarification of that exchange, designated
`service/docs/adr/ADR-0005-fixed-body-reserve.md` as the approval target; that
record is now Accepted / Complete with fresh approval-HEAD verification
recorded. `service/docs/adr/ADR-0006-static-rejection-wires.md` then received
its own exact `Approve` from @hailingu in an exchange whose sole approval
context was that record; it is likewise Accepted / Complete with fresh
approval-HEAD verification recorded. Later the same day, the owner instructed
“挑选一个执行优化，然后和现有的bench mark对比。在优化前，先将未提交的内容提交”,
authorizing the append-form response-serialization round under the same pattern
(red `2be8eaa`, green `5b404c3`); its record is
`service/docs/adr/ADR-0007-append-form-response-serialization.md`. The owner
then supplied the exact `Approve` in an exchange whose sole approval context
was that record; it is now Accepted / Complete with fresh approval-HEAD
verification recorded.

On 2026-10-22, in the current performance rerun chat, the owner explicitly
selected “本轮豁免，实施并回补 ADR” for real-socket measurement and measured
kqueue dispatch/write/input changes. This waives preapproval and serialization
only for that scope and permits the two retrospective records below. It grants
no standing waiver or individual acceptance. Both remain Proposed / Not Started
until individually approved; historical exception execution is kept separately.

On 2026-10-28, @hailingu supplied the exact `Approve` in two one-record
approval exchanges, each naming exactly one repository-relative path:
`docs/adr/ADR-0010-http-server-performance-measurement.md` and
`service/docs/adr/ADR-0008-kqueue-response-scheduling.md`. The measurement
record completed the same day with fresh HEAD verification at anchor
`8ef4e2b` (Debug 15/15, Release benchmark tree 18/18, Release+ASan/UBSan
bench suites 2/2, process tests 30/30) and is Accepted / Complete. The
kqueue scheduling record's approval was invalidated in the same change: its
Implementation Plan carried four subtasks against the at-most-three-subtask
cap for Accepted records, so its former T-3/T-4 rows merged with all
evidence preserved, returning it to Proposed / Not Started pending
reapproval of the merged plan. The scoped 2026-10-22 waiver is discharged
by the individual approvals; `docs/adr/ADR-0009-http-parser-verification-ci.md`
remains Accepted / In Progress awaiting its authorized hosted Actions run,
which no waiver or approval in this section provides.

## Active Records

| Type | ID | Title | Decision Status | Implementation Status | Scope | Architecture Source | Path | Superseded By |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Full ADR | ADR-0001 | Self-contained orange issue governance | Accepted | Complete | Repository-wide issue creation, classification, evidence, and closure | N/A — process governance, not product-demand implementation | `docs/adr/ADR-0001-orange-issue-governance.md` | None — first issue-governance proposal |
| Full ADR | ADR-0002 | Adopt and localize the governance validator | Accepted | Complete | Project | N/A — blocker-resolution governance tooling under the AGENTS.md serialization exception | `docs/adr/ADR-0002-governance-validator-adoption.md` | None — not superseded |
| Full ADR | ADR-0003 | One documented local runtime path with a loopback health endpoint | Accepted | Complete | Project | docs/architecture/ADD-0001-orange-v1-baseline.md — CAND-0001 | `docs/adr/ADR-0003-local-runtime-path.md` | None — not superseded |
| Full ADR | ADR-0004 | Engineering standards preceding implementation | Accepted | Complete | Project | N/A — blocker-resolution governance documentation under the AGENTS.md serialization exception, not product-demand implementation | `docs/adr/ADR-0004-engineering-standards.md` | None — not superseded |
| Full ADR | ADR-0005 | Match concrete required approvers during approval validation | Accepted | Verified | Project | N/A — corrective governance-tool maintenance, not product-demand implementation | `docs/adr/ADR-0005-required-approver-match.md` | None — not superseded |
| Full ADR | ADR-0006 | Validate every table in decision and architecture indexes | Accepted | Verified | Project | N/A — corrective governance-tool maintenance, not product-demand implementation | `docs/adr/ADR-0006-complete-record-index-validation.md` | None — not superseded |
| Lightweight ADR | ADR-0001 | Decompose the HTTP request-head test entry point | Accepted | Verified | Service internal — service | N/A — behavior-preserving maintenance of existing service tests, not a new product-demand slice | `service/docs/adr/ADR-0001-http-message-test-decomposition.md` | None — not superseded |
| Lightweight ADR | ADR-0007 | Clean up governance test fixtures through their owning tests | Accepted | Complete | Project | N/A — test-resource lifecycle correction in governance tooling, not product-demand implementation | `docs/adr/ADR-0007-governance-fixture-lifecycle.md` | None — not superseded |
| Full ADR | ADR-0008 | Reproducible HTTP parser benchmarks | Accepted | Complete | Project | N/A — corrective performance maintenance of an existing parser and its verification, not a new product-demand capability | `docs/adr/ADR-0008-http-parser-benchmarks.md` | None — not superseded |
| Full ADR | ADR-0002 | Advance chunk metadata by bounded spans | Accepted | Complete | Service internal — service | N/A — corrective performance maintenance of an existing parser and its verification, not a new product-demand capability | `service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md` | None — not superseded |
| Full ADR | ADR-0003 | Retain validated offsets for fragmented request heads | Accepted | Complete | Service internal — service | N/A — corrective performance maintenance of an existing parser and its verification, not a new product-demand capability | `service/docs/adr/ADR-0003-fragmented-head-offsets.md` | None — not superseded |
| Full ADR | ADR-0009 | Disposable HTTP parser verification in CI | Accepted | Complete | Project | N/A — corrective performance maintenance of an existing parser and its verification, not a new product-demand capability | `docs/adr/ADR-0009-http-parser-verification-ci.md` | None — not superseded |
| Full ADR | ADR-0011 | Two-layer quality gates — hosted CI and a local SonarQube gate | Proposed | Not Started | Project | N/A — CI and quality-gate tooling, not product-demand implementation | `docs/adr/ADR-0011-ci-quality-gates.md` | None — not superseded |
| Full ADR | ADR-0004 | Own request text with offset descriptors | Accepted | Complete | Service internal — service | N/A — corrective performance maintenance of an existing parser and its verification, not a new product-demand capability | `service/docs/adr/ADR-0004-owned-request-storage.md` | None — not superseded |
| Full ADR | ADR-0005 | Reserve the declared fixed body length once | Accepted | Complete | Service internal — service | N/A — corrective performance maintenance of an existing parser and its verification, not a new product-demand capability | `service/docs/adr/ADR-0005-fixed-body-reserve.md` | None — not superseded |
| Full ADR | ADR-0006 | Answer rejections with precomputed static wires | Accepted | Complete | Service internal — service | N/A — corrective performance maintenance of an existing serializer and its verification, not a new product-demand capability | `service/docs/adr/ADR-0006-static-rejection-wires.md` | None — not superseded |
| Full ADR | ADR-0007 | Serialize responses straight into the outgoing backlog | Accepted | Complete | Service internal — service | N/A — corrective performance maintenance of an existing serializer and its verification, not a new product-demand capability | `service/docs/adr/ADR-0007-append-form-response-serialization.md` | None — not superseded |
| Full ADR | ADR-0010 | Measure real loopback HTTP serving separately from parsing | Accepted | Complete | Project | N/A — corrective performance verification, not product-demand implementation | `docs/adr/ADR-0010-http-server-performance-measurement.md` | None — not superseded |
| Full ADR | ADR-0008 | Use stable kqueue connection identities and avoid redundant request-path work | Accepted | Complete | Service internal — service | N/A — corrective performance maintenance, not product-demand implementation | `service/docs/adr/ADR-0008-kqueue-response-scheduling.md` | None — not superseded |

## Archived Records

None — no decision record has been retired or archived.
