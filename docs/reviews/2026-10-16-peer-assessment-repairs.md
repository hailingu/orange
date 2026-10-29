# Peer Assessment: Findings And Authorized Repairs

## Scope And Authorization

This report records an implementation review and repairs to the existing HTTP
decoder and governance validator. It is factual task evidence, not an ADR and
not a replacement for the repository's decision-record policy.

The supplied assessment was read from the owner's attachment `Pasted text.txt`
(attachment ID `12bb8e90-745a-4c4c-bd44-a3620c7194e2`). Findings were checked
against the workspace based on commit
`1f876464862c69c5a54424d91fbfd6937983e23c`, including the earlier approved
repairs already present as uncommitted changes. The independently reproduced
pre-repair CLI outcomes and HTTP probes were retained as
`orange-peer-assessment-review.md` and
`orange-peer-assessment-cli-results.json` in this chat's visualization directory.

On 2026-10-16, owner @hailingu explicitly authorized this task with:

> 本次明确豁免 ADR 前置审批与串行限制，直接修复已证实问题

This one-task exception covers the confirmed repairs below. Existing ADR
approval metadata and historical evidence were preserved. In particular,
runtime ADR-0003 and fixture-lifecycle ADR-0007 remain Proposed/Not Started;
this exception does not mark either Accepted or complete their broader work.
No commit, push, deployment, runtime listener or reusable build was produced.

## Assessment Corrections

The assessment identified real validator gaps and an unbounded decoder
metadata workload, but some conclusions were overstated:

- There are no independently maintained request-line/header grammars:
  `service/src/http_messages.cpp::detail::is_valid_request_line` delegates to
  `validate_request_line`, also used by `parse_request`; the header wrapper
  likewise delegates to `validate_field_line`.
- An arbitrary index substring was insufficient to bypass the whole CLI:
  the global record-membership check already rejected unindexed records.
  The local supersession check was nevertheless made structural.
- ADR-0006's 240-line allowance was an explicit accepted engineering exception
  with owner @hailingu and a removal/review trigger, not an agent granting
  itself permission. The affected factory is now removed. Its historical
  acceptance evidence was not rewritten to a different threshold.
- The common engineering standard already applies to JavaScript. Missing
  JavaScript-specific style tooling is a separate improvement, not absence of
  any applicable guardrails. Adding lint/format dependencies or a new standard
  was not necessary for the confirmed repairs and was left out of this task.
- The peer's benchmark and oracle programs were not supplied, so their
  400,000-case fuzz result and nanosecond figures were not independently
  certified. Also, 43 ns corresponds to about 23.26 million requests/second,
  and 282 microseconds divided by 16,384 chunks is about 17.2 ns/chunk, not
  per metadata byte. One MiB divided by 3.29 ms is about 3.14 ns/byte.

The input's overall numeric grade therefore is not treated as a measured
project-wide quality result. Performance rewrites, a benchmark subsystem and
product completion remain outside this repair's scope.

## Reproduced Findings And Repairs

The CLI regressions below are exercised through the real process in
[`peer-review-regressions.test.mjs`](../../tools/governance-validator/test/peer-review-regressions.test.mjs).
Fixture transformations operate on controlled Markdown grammar, not opaque
repository-source assertions. Each new fixture is removed with `t.after`.

| Finding and pre-repair evidence | Owning implementation and result | Regression coverage |
| --- | --- | --- |
| ADR moved into `docs/adr/ocr/` with a synchronized index returned exit 0 | `lib/record-identity.mjs::validRecordLocation` and CLI dispatch reject prefix/directory mismatch with `RECORD_LOCATION_INVALID`, including ADD misplacement | Misplaced ADR, ADD and OCR; existing valid service/archive index suites |
| Invalid Mermaid in an ADR, OCR or ordinary document returned exit 0 | `lib/mermaid-validation.mjs::validateMermaidSyntax` runs for instantiated Markdown; ADD completeness remains separate | Invalid diagrams in all three paths; valid ordinary diagram; nested example fences; authoring-template control |
| A source marker kept the old path after archival and returned exit 0 | `lib/provenance-validation.mjs::validateProvenance` resolves explicit leading comment markers using the existing repository-containment reader; stale paths emit `PROVENANCE_RECORD_MISSING` | Stale C++ marker rejected, archived replacement marker accepted; multiline source example ignored |
| Deferred candidate without a reason returned exit 0 | `lib/relationship-validation.mjs::candidateTable` retains a reason/evidence value; Deferred checks reject blanks and placeholders | Missing reason rejected; explicit reason accepted; legacy reasoned unused-path cells retained |
| A valid risk matrix followed by a legend produced three false dimension errors | `lib/accepted-risk.mjs::validateRiskMatrixDimensions` reads the structured matrix's rows | Valid matrix plus independent status legend accepted; existing missing/duplicate/extra dimension cases retained |
| Fenced template example produced an unresolved-variable error | CLI checks active instantiated text outside comments/fences; raw template declarations still undergo their dedicated check | Inactive example accepted, active placeholder rejected, declared authoring variable retained |
| Prepending an OCR checklist item produced six false eligibility errors | `lib/accepted-ocr.mjs::validateOcrEligibility` matches each required semantic assertion independent of position | Additional item accepted; unchecked required assertion and unchecked duplicate rejected |
| Markdown-link replacement path was treated as missing, and the CLI dropped service prefixes | `lib/record-identity.mjs::recordPathsFromValue` is shared by index, relationship, supersession and provenance readers | Linked replacement with a complete service root and linked reciprocal/index paths accepted; existing containment checks retained |
| Wrong-directory ADD produced ADR lifecycle diagnostics | Filename classification is centralized in `recordKind`; invalid residence stops lifecycle dispatch | Exact location diagnostic rather than treating the ADD as an ADR |
| Local supersession used whole-index substring membership | CLI `validateSupersession` compares parsed Path cells, ignoring unrelated text | Existing unindexed/non-record/reciprocity controls plus the linked service replacement case |

### HTTP Resource Boundary And API Clarity

Before repair, a one-MiB extension and 2,000 trailers were accepted, with wire
counts 1,048,645 and 150,060 respectively, despite tiny decoded content. The
state machines used constant metadata storage but accepted unbounded work.

`service/src/http_request_decoder.hpp::chunk_metadata_cap` now declares an
independent 8,192-byte per-message budget. `RequestDecoder::Impl::read_metadata`
counts all chunk size lines, extensions, data CRLF delimiters, the last-chunk
line and discarded trailers. Head and decoded-body bytes use their existing
separate caps. Byte 8,193 fails with `ParseError::chunk_metadata_too_long`;
the caller's documented HTTP mapping is 413 followed by connection closure.
The current implementation is a parser library, so this task does not claim
to have exercised an HTTP server response.

This is a project-selected aggregate budget. [RFC 9112 section 7.1.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1.1)
recommends bounding received extensions but does not prescribe 8 KiB.

`service/tests/http_request_decoder_tests.cpp::check_metadata_limits` and
`check_aggregate_metadata` cover exact cap, cap plus one, extension/trailer
paths, many small chunks/trailers, one-byte/17-byte/whole-message transport
fragments, pipeline exclusion, sticky failure and reset to an independent
input stream. Existing tests still accept a full one-MiB decoded body.

Error `consumed` values differed between contiguous and fragmented invalid
heads. They are discard accounting, not a valid next-message boundary. The
public `DecodeResult` and `RequestDecoder::feed` documentation now explicitly
requires discarding the connection input on any error. Successful message
boundaries were unchanged; no fake precision was added to the failure path.

`service/src/http_body_framing.cpp` now directly includes `<algorithm>` for
`std::all_of` rather than relying on `http_syntax.hpp` to provide it.

## Decomposition Review

The large accepted-stage, relationship, terminal and Mermaid factories were
removed. Module functions take explicit context and error collections; no
forwarding factory was introduced. Cross-module APIs retain only validators
consumed by other capabilities or CLI dispatch. Private row helpers remain
with their owning tables. Shared reasoned-N/A parsing has multiple consumers.

Accepted-stage plan/check tables, risk/traceability tables and OCR runbooks now
have separate modules and independently failing contracts. Dependency flow is
from CLI to validators and from validators to shared Markdown/path/value
readers; no validator imports the CLI and there are no new module cycles.

The final content IDs and point-in-time physical/function measurements are in
[`2026-10-16-peer-assessment-repairs-evidence.json`](2026-10-16-peer-assessment-repairs-evidence.json).
Function measurements include blanks and exclude comment lines. After the
split, the largest maintained JavaScript function is 51 code lines
(`validateSupersession`), below the 60-line review threshold and 80-line cap.
The 763-line CLI remains above its file-review threshold but below the 800-line
cap: it owns configured policy constants, filesystem traversal, lifecycle
dispatch and diagnostics; extracting a thin entry wrapper would not remove
that responsibility. The previously oversized accepted-stage file is now
155 physical lines; the new capability modules are all below 600.

The existing 1,163/1,211-line lifecycle/structure test files were not materially
changed by this repair. They remain below the 1,800-line cap; their observed
decomposition-review debt is recorded without expanding this task into an
unrelated test rewrite. The new CLI regression file remains below 1,000 lines.
Cyclomatic complexity: N/A — no configured complexity tool. Function spans
and manual nesting review were used instead; affected executable nesting
stays at or below four levels, with no new loops deeper than three levels.

## Verification Evidence

The retained adjacent JSON records exact commands, results, toolchain,
test-first failures, final source content IDs and temporary-output cleanup.
Configured CMake commands used a task-owned temporary build directory instead
of `service/build` to preserve the existing checkout's output. All compiler,
fixture and sanitizer output was disposable; only text evidence was retained.

The initial 15 CLI regressions failed 11 cases for the intended defects before
production changes; four controls passed. The initial metadata tests failed
14 runtime checks before production changes. Their production fixes and
subsequent refactoring were then covered by the final checks in the JSON.

## Follow-Up Clarification, 2026-10-16

The follow-up in
[`2026-10-16-peer-assessment-followup.md`](2026-10-16-peer-assessment-followup.md)
fixes the old fixture lifecycle and active Markdown reference gap, synchronizes
ADR-0003 with the metadata policy, and makes the joint-budget tradeoff explicit.
The one-MiB acceptance above is an existence example, not a guarantee for every
chunk encoding: without extensions/trailers, 1,024-byte chunks spend 7,173
metadata bytes and pass, while 512-byte chunks would spend 14,341 and fail.
Transport feed sizes do not alter that outcome. Historical source hashes and
verification results in this report remain evidence of the earlier revision.
