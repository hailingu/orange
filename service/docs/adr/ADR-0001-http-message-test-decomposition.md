# Lightweight ADR-0001: Decompose the HTTP request-head test entry point

## Metadata [Required]

- **Decision Status**: Accepted
- **Implementation Status**: Verified
- **Date**: 2026-10-15
- **Author**: @codex
- **Decision Owner**: @hailingu
- **Required Approver**: @hailingu
- **Record Scope**: Service internal — service
- **Approver [Conditionally Required — Decision Status is or has been `Accepted`]**: @hailingu
- **Approval Time [Conditionally Required — Decision Status is or has been `Accepted`]**: 2026-10-15T11:24:20Z
- **Approval Evidence [Conditionally Required — Decision Status is or has been `Accepted`]**: Approve
- **Rejector [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Time [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Evidence [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Rejection Reason [Conditionally Required — Decision Status is `Rejected`]**: N/A — not rejected
- **Retired By [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Time [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Evidence [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Retirement Reason [Conditionally Required — Decision Status is `Deprecated` or `Superseded`]**: N/A — not retired
- **Blocked From [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker And Evidence [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Owner [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Blocker Exit Or Recheck Criterion [Conditionally Required — Implementation Status is `Blocked`]**: N/A — not blocked
- **Related [Optional]**: docs/adr/ADR-0003-local-runtime-path.md; docs/adr/ADR-0004-engineering-standards.md
- **Architecture Source [Conditionally Required — product demand]**: N/A — behavior-preserving maintenance of existing service tests, not a new product-demand slice
- **Supersedes [Conditionally Required — this ADR replaces another]**: None
- **Superseded By [Conditionally Required — this ADR is replaced]**: None — not superseded

## Requirement Level Legend [Required]

- **`[Required]`**: The section or field always applies and MUST remain present
  with complete, verifiable content. Use `None — <reason>` only when the
  template explicitly permits an empty result; never leave it blank.
- **`[Conditionally Required — <trigger>]`**: The section or field MUST be
  completed when its stated trigger applies. When the trigger does not apply,
  retain `N/A — <reason>` unless the template explicitly instructs removal or
  retention as inactive future-lifecycle guidance. A missing trigger assessment
  is incomplete content.
- **`[Optional]`**: The section may be removed without affecting acceptance,
  execution, completion, or verification. If retained, it MUST be accurate and
  complete; optional content MUST NOT substitute for required evidence.

Unlabeled fields inside a `[Required]` section are required.

## Context [Required]

Quality-review finding 4 identifies `service/tests/http_message_tests.cpp::main`
at source revision `1f876464862c69c5a54424d91fbfd6937983e23c`: 329
physical lines and 307 code lines including blanks and excluding comment
lines. This exceeds the common 80-code-line cap. The current runtime proposal
already records its temporary exception and requires removal before T-1
completion.

The surrounding file has 665 physical lines. Six existing `check_*` helpers,
the shared `CheckReporter` and scoped `AllocationCounter` already demonstrate
a local decomposition style. The quality review ran both CTest suites
successfully. This record covers one behavior-preserving test-maintenance
slice in one pull request; it does not select or replace the runtime ADD
candidate, whose existing reciprocal link remains in place.

Routing is service-internal because only the service's private test code
changes and no externally consumed contract changes. Factual removal of the
obsolete runtime-proposal exception is supporting evidence maintenance.

**Task-scoped drafting authorization**: In this chat on 2026-10-15, the
repository owner explicitly permitted temporarily waiving ADR serialization
to prepare separate repair records for quality-review findings 1, 2, 4 and 5,
while retaining individual approval before implementation. This direct user
instruction permits these proposals to coexist with the unfinished runtime
record; it is not the Accepted-and-Blocked exception and does not change the
general repository policy. Each repair still requires its own exact
`Approve` under `AGENTS.md` Approval and Status.

## Decision [Required]

Extract the inline cases into named, responsibility-specific functions in
the same anonymous namespace, each taking `CheckReporter&`. Keep one executable
and one CTest registration. Use the existing helper style and standard
library; add no framework, new production seam or new public header.

- **HT-1**: Preserve every existing wire input, expected parse result, consumed-byte check, field-order/ownership check and allocation assertion. The existing request-line, request-target, field-line, cap, incomplete-input and six helper families all remain exercised.
- **HT-2**: `main` constructs one reporter, dispatches each family once in the existing relative order, and returns `reporter.exit_status()`. The reporter's failure reporting and allocation counter's scoped lifetime remain unchanged.
- **HT-3**: Every new or modified executable unit is at most 60 code lines; `main` is at most 30. The test file remains within 1,000 physical lines, so no new size exception is needed. These are structured-review measurements, not source-text regression tests.
- **HT-4**: After the extraction passes verification, record the measured removal of the old main exception in the existing runtime proposal without accepting that proposal or claiming network/browser acceptance.

## Scope [Required]

In scope:

- `service/tests/http_message_tests.cpp`: local helper extraction, intent comments, and the governing-record marker for this maintenance slice.
- An input/outcome inventory and before/after verification evidence in this record.
- Evidence-only maintenance of the obsolete head-test-main exception in `docs/adr/ADR-0003-local-runtime-path.md`.

Out of scope:

- Production parser/decoder code, their API or HTTP behavior, CMake configuration, dependencies and test framework changes.
- Body-decoder test reorganization, runtime implementation and product capabilities.

## Lightweight Eligibility Check [Required]

- [x] Change is limited to the paths in Scope and can be fully reverted without migration or retained partial behavior.
- [x] Change modifies only localized test source; the runtime-proposal update records measured exception removal and changes no normative decision.
- [x] No public API, schema, protocol, service boundary, auth, security, data model, migration, dependency, build, CI, or deployment behavior changes.
- [x] No new technology, framework, storage, queue, runtime, provider, or design pattern.
- [x] No material product or technical question remains unresolved.
- [x] Existing CTest behavior and a predetermined structured size/case review provide exact observable results.

## Implementation Plan [Required]

**Complete task outcome**: The HTTP request-head suite retains its complete
behavior coverage and reporting contract with a small dispatch-only entry
point and cohesive local case functions.

**Primary implementation boundary**: Service request-head test organization.

Allowed subtask statuses: `Not Started`, `In Progress`, `Blocked`, `Complete`,
or `N/A — <specific reason>`.

| ID | Objective or deliverable | Included scope | Status | Actual implementation evidence |
| --- | --- | --- | --- | --- |
| T-1 | Extract cohesive case families while retaining the complete test behavior | Existing test file, before/after case inventory, verification and measured exception-removal evidence | Complete | Fourteen local functions replace the inline cases; main dispatches those and the six unchanged helpers once in original order. Verified Implementation Evidence records source content IDs, complete case inventory, size measurements, before/after CTest results and runtime-metadata preservation |

This is a pure refactor. Run the existing suite green as characterization
before extraction, then keep it green after each extraction; do not manufacture
a failing behavior test for source layout. Any discovered parser defect is
outside this slice and requires its own failing regression and authorization.

### Stable Implementation Touchpoints [Required]

| Path | Stable symbol or contract anchor | Key code excerpt, when needed | Purpose | Source revision |
| --- | --- | --- | --- | --- |
| service/tests/http_message_tests.cpp | main | N/A — stable symbol is sufficient | Replace inline cases with calls to cohesive local case functions | 1f876464862c69c5a54424d91fbfd6937983e23c |
| service/tests/http_message_tests.cpp | anonymous namespace::CheckReporter; anonymous namespace::AllocationCounter; check_parse_allocations | N/A — stable symbols are sufficient | Retain failure reporting, test exit status and allocation ownership | 1f876464862c69c5a54424d91fbfd6937983e23c |
| service/CMakeLists.txt | http_message_tests; http_request_decoder_tests | N/A — stable target names are sufficient | Existing compilation and CTest boundaries; inspect and execute without editing | 1f876464862c69c5a54424d91fbfd6937983e23c |

**Migration and rollback**: No data migration. Revert only the extraction and
its factual exception-removal note to restore the prior suite organization.
Restore the prior exception note if the over-cap main returns.

### Engineering Exceptions [Conditionally Required — an engineering rule is exceeded or waived]

N/A — this slice removes the recorded main cap exception. The completed result
must satisfy HT-3; the original measurement remains historical evidence.

### Verified Implementation Evidence [Optional]

The baseline remains commit
`1f876464862c69c5a54424d91fbfd6937983e23c`. Before editing, the request-head
test content ID was `a2f2c65d3e6c191b3a5c356224da3fe191713bb8`.
The resulting `service/tests/http_message_tests.cpp` content ID is
`4f576fd1bbb42b3d2130ab971d7ceda71720977f`. IDs were computed with
`git hash-object` over the source bytes; no commit or push was created.

**AC-2 semantic inventory**: The following rows follow the original execution
order. All 84 former inline `reporter.check` call sites retain their inputs,
conditions and diagnostic descriptions; the six existing helper bodies retain
their 19 additional call sites and all loop-generated cases. Counts describe
reviewed assertion sites, not executed-case totals or a maintained layout test.
Every named function is dispatched exactly once with the same reporter.

| Destination in anonymous namespace | Retained inputs and observable expectations | Check call sites | Code lines including blanks, excluding comments |
| --- | --- | --- | --- |
| check_incomplete_request_lines | Empty input, unterminated GET line, trailing CR, 64 method bytes and CRLF alone all return incomplete | 5 | 16 |
| check_request_line_limits | cap method bytes return method_too_long; cap-sized target and cut `%2` return target_too_long; version overflow blames the longer method; request line ending at cap returns headers_too_long, one byte past returns target_too_long | 6 | 26 |
| check_malformed_request_line_overflow | `GE(T`, bare CR in target, `banana-banana-banana` version and CRLF-only exhaustion remain malformed despite overflow | 4 | 20 |
| check_request_line_fields | POST /api/echo retains method/target/version, consumed 27 and no fields; HTTP/2.0 and !CUSTOM! parse; a leading CRLF is skipped and included in consumed 20 | 10 | 29 |
| check_malformed_request_lines | One/two tokens, doubled/leading space, extra token, bare LF, tokens crossing CRLF, CR within version, invalid method, banana version and lowercase http all return malformed | 11 | 35 |
| check_rejected_target_octets | NUL fixture remains 19 bytes with NUL at offset 6; NUL, VT, HTAB, DEL, raw UTF-8, fragment, angle brackets, `%G1` and truncated `%2` all return malformed | 10 | 34 |
| check_request_target_forms | OPTIONS `*`, absolute `http://example.com:8080/a?b=c` and CONNECT `example.com:443` preserve the complete target | 3 | 12 |
| check_header_fields | POST with Host 127.0.0.1:8080 and Content-Length 5 yields two owned fields in order and consumed 68 | 5 | 17 |
| check_header_value_trimming | X-Pad, X-Tab, Empty and Only-OWS yield four fields, trimmed spaced/value strings and two empty values | 4 | 17 |
| check_repeated_header_fields | x-a:1, X-B:2, x-a:3 retain three entries, original names/case/order and values without combining | 4 | 17 |
| check_header_value_content | User-Agent keeps internal spaces; X-Note keeps the original UTF-8 obs-text bytes through string_view literals | 2 | 14 |
| check_header_limits | X-Pad overflow and head at cap+1 return headers_too_long; exact-cap head consumes cap; trailing CR stays headers_too_long; DEL and bare CR in overflow stay malformed | 6 | 28 |
| check_incomplete_headers | Request line alone, partial Host value and complete Host line without final empty line all return incomplete | 3 | 11 |
| check_malformed_header_lines | Missing colon, empty name, whitespace in name, SP/HTAB obs-fold, bare LF/CR and NUL/VT/DEL values return malformed; NUL fixture stays 28 bytes with NUL at offset 22 | 11 | 39 |
| check_parse_allocations — unchanged | Empty head, incomplete/malformed/oversized 16-field heads, complete head with unread body and long incomplete fields retain zero-allocation or at-most-one-allocation checks; complete consumed 146 and 16 fields | 6 | 41 |
| check_deferred_field_copies — unchanged | Reusing the source leaves request-line and empty/trimmed/duplicate/obs-text fields owned; 2,043 shortest fields fit consumed 8,192 with at most one allocation | 3 | 31 |
| check_field_value_octets — unchanged | All 256 bytes at positions 1/7/15/16/31/32/47/62 retain RFC-derived allowed-value or malformed outcomes | 2 | 22 |
| check_field_value_boundaries — unchanged | The existing 16 lengths, 16 HTAB paddings and every DEL position retain trimming, consumed/body-boundary and rejection assertions | 2 | 25 |
| check_ows_trimming — unchanged | Existing 17 lengths and three OWS patterns retain interior content, empty-value and allocation checks; exact-cap all-HTAB field consumes 8,192 | 3 | 36 |
| check_ows_edge_octets — unchanged | Existing nine lengths and all 256 edge bytes distinguish SP/HTAB trimming, preserved VCHAR/obs-text and rejected control bytes | 3 | 27 |

**Ownership and cohesion review**: `CheckReporter`, `AllocationCounter`,
`parse_counted`, global allocation replacements and the six original helper
bodies are unchanged. `main` owns one reporter and returns its exit status.
The three cap-using groups each bind the same existing header_block_cap
constant locally. The three groups needing embedded-NUL or obs-text view
literals retain the string_view literal namespace in their own scope.
Fixture construction and diagnostics remain outside each allocation counter.
No production, CMake, dependency, API or assertion change appears in the diff.

**AC-3 engineering review**: At the resulting content ID, main is 24 code /
physical lines, down from 307 code / 329 physical lines. New functions range
from 11 to 39 code lines and each documents its distinct case responsibility
before its body; the file is 718 physical lines, up from 665. New executable
nesting is at most one level (the existing empty-line fixture loop).
Cyclomatic complexity: N/A — no configured complexity tool; unit span,
cohesion and nesting were inspected instead. No threshold or cap is exceeded.

**AC-1 commands and results**, from the repository root, on 2026-10-15:

```text
cmake -S service -B /private/tmp/orange-head-tests-w8qYMn/cpp-build
cmake --build /private/tmp/orange-head-tests-w8qYMn/cpp-build
ctest --test-dir /private/tmp/orange-head-tests-w8qYMn/cpp-build --output-on-failure
clang-format --dry-run --Werror service/tests/http_message_tests.cpp
git diff --check
```

Configure/build/CTest/format/diff checks exit 0. Both existing CTest targets
pass before extraction (2/2, 0 failures, 0.93 seconds) and after extraction
(2/2, 0 failures, 1.67 seconds); both builds emit no compiler warnings.
Toolchain: Apple clang 21.0.0 (clang-2100.3.34.2),
arm64-apple-darwin27.0.0; CMake 4.2.3; clang-format 23.1.3.
The task-owned directory substitutes for service/build solely to preserve
the user's existing build output. This is the same routed CMake/CTest
verification, with disposable output removed before task completion.

**AC-3 governance commands**: With TMPDIR set to the task-owned
`/private/tmp/orange-head-tests-w8qYMn/fixtures`,
`npm test --prefix tools/governance-validator` exits 0: 271/271 pass,
0 failures/cancellations/skips, 26.539 seconds.
`npm run validate --prefix tools/governance-validator` and `git diff --check`
exit 0. Temporary verification output and fixture directories are removed;
no reusable build artifact is retained.

**AC-3 runtime metadata and linkage**: Before this slice, the runtime proposal
was captured separately. The configured metadata parser confirms the same
values after exception removal: Decision Status Proposed; Implementation
Status Not Started; Approver, Approval Time and Approval Evidence all
`Pending — reapproval required`; Architecture Source
`docs/architecture/ADD-0001-orange-v1-baseline.md — CAND-0001`.
The complete ADD bytes remain unchanged by this slice (content ID
`199590b373a851a72f43ca751672f693c6e3b979`); its parsed CAND-0001 row remains
Selected with the reciprocal runtime path. The runtime proposal's resulting
content ID is `24c0543d7dbdb92da69b6dd6e79816a31c404630`.
Only its obsolete exception and supporting change-log evidence changed in
this slice. No runtime code, network check or browser check was run or claimed.

## Contract-To-Check Traceability [Required]

| Clause ID | Authoritative contract path and heading | Exact normative requirement | Acceptance check or deterministic test IDs | Explicit coverage method |
| --- | --- | --- | --- | --- |
| HT-1 | docs/development/software-engineering-standard.md › Testing And Change Design; this record › Decision › HT-1 | Refactoring retains all observable parser assertions and case families | AC-1, AC-2 | Run the same CTest targets before/after and review every original wire input/assertion against its destination |
| HT-2 | service/tests/http_message_tests.cpp › CheckReporter and AllocationCounter; this record › Decision › HT-2 | One reporter aggregates failures; every family executes once; allocation measurement retains its scoped lifetime | AC-1, AC-2 | Compiler/CTest execution plus case-call and ownership review |
| HT-3 | docs/development/software-engineering-standard.md › Size And Complexity Guardrails; this record › Decision › HT-3 | Changed units at most 60 code lines, main at most 30 and file at most 1,000 physical lines | AC-3 | Record point-in-time unit/file measurements and cohesion review; no exact-text or layout tests |
| HT-4 | docs/adr/ADR-0003-local-runtime-path.md › Engineering Exceptions; this record › Decision › HT-4 | Remove the obsolete exception with measured evidence without accepting the runtime proposal or changing its product linkage | AC-3 | Inspect the exception evidence and unchanged runtime status and reciprocal candidate link |

## Risk Coverage Matrix [Required]

| Risk dimension | Applicability and scenario, or specific N/A reason | Owning boundary | Deterministic verification method | Exact expected result | Acceptance check IDs | Status | Actual evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concurrency and ordering | Applicable — extraction accidentally omits, repeats or reorders a case family | Test entry point | Before/after case inventory and dispatch review, then run CTest | Each original family executes once in the same relative order; both targets pass | AC-1, AC-2 | Pass | Verified Implementation Evidence inventories all 20 families in original dispatch order, retaining 84 extracted and 19 unchanged assertion sites; both targets pass before/after |
| timeout and deadline | N/A — this unit suite parses fixed in-memory inputs and adds no socket, wait or timer | Service tests | Inspect the scoped diff | No deadline-bearing operation introduced | AC-2 | N/A — no wait or transport | N/A — no timer |
| cancellation and interruption | N/A — no asynchronous work or cancellation lifecycle changes | Service tests | Inspect the scoped diff and allocation lifetime | No new interruptible work; scoped counter behavior retained | AC-2 | N/A — synchronous refactor | N/A — no cancellation state |
| resource bounds and backpressure | Applicable — changing helper scope accidentally includes fixtures or diagnostics in allocation accounting | Test allocation harness | Keep and execute the existing allocation assertions; inspect counter scopes | Existing zero-allocation and bounded-allocation checks pass; counter ownership is unchanged | AC-1, AC-2 | Pass | Counter and parse_counted bodies/scopes are unchanged; existing zero-allocation and at-most-one-allocation assertions pass in the before/after CTest runs |
| framework or trust-boundary rejection | Applicable — malformed and incomplete wire cases disappear during extraction or stop reporting through CTest | Parser test boundary | Compile and run both existing targets; review malformed/incomplete/cap input inventory | All original rejection expectations remain and both targets exit 0 | AC-1, AC-2 | Pass | All request-line, target, field, incomplete and cap cases map to the retained assertions in the inventory; both CTest targets exit 0 after extraction |

## Acceptance Checks [Required]

| Check ID | Subtask | Binary acceptance point | Preconditions or input | Verification method | Exact expected result | Expected evidence | Status | Actual result and evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AC-1 | T-1 | Existing compiled behavior suites pass before and after extraction | Existing C++20 toolchain and unchanged production source; fresh disposable verification build | cmake -S service -B service/build; cmake --build service/build; ctest --test-dir service/build --output-on-failure; clang-format --dry-run --Werror service/tests/http_message_tests.cpp; git diff --check | Configure/build/CTest/format/diff checks exit 0; both existing CTest targets pass before and after; no compiler warning | Exact compiler version, commands and exit codes, before/after revision and CTest summary | Pass | Apple clang 21.0.0; configure/build/CTest/format/diff checks all exit 0. Before/after CTest 2/2 with no compiler warnings; exact commands and content IDs in Verified Implementation Evidence |
| AC-2 | T-1 | Extraction preserves all test assertions and ownership semantics | Before/after test source at recorded revisions | Structured case inventory keyed by original wire input and asserted API result: incomplete line/head; request method/target/version and leading lines; malformed request/URI bytes; field values/order/OWS; header and request-line caps; the six existing helper families. Inspect reporter calls and AllocationCounter scopes | Every original input/assertion maps to one retained case, with no omitted or weakened expectation; each family is dispatched once in original relative order; reporter and counter contracts are unchanged; no production or CMake diff | Reviewed semantic inventory and ownership record; not an automated source-text/layout assertion | Pass | Verified Implementation Evidence maps all 84 original inline assertion sites and six unchanged helper families (19 sites and their generated cases) in original order; one reporter and unchanged allocation scopes; production and CMake diff empty |
| AC-3 | T-1 | The size exception is removed with accurate proposal evidence | Verified extraction; runtime proposal's approval metadata, Decision Status, Implementation Status and reciprocal ADD candidate link captured before this maintenance slice | Structured unit/file span and cohesion review; inspect the runtime proposal's exception update; npm test --prefix tools/governance-validator; npm run validate --prefix tools/governance-validator | Every changed executable unit at most 60 code lines, main at most 30, file at most 1,000 physical lines; intent comments precede new helpers; governance commands exit 0; the captured runtime metadata, statuses and ADD link are unchanged by this slice | Measured source revision, semantic review, exception-removal note and governance command results | Pass | main 24 code lines, new helpers at most 39, file 718 physical lines; intent/nesting/cohesion review and runtime metadata/ADD preservation recorded above. Governance tests 271/271, validation and diff check pass |

## Completion Checklist [Required]

| ID | Item | Completion Criterion | Expected Evidence | Status | Actual Evidence |
| --- | --- | --- | --- | --- | --- |
| A-1 | ADR approved | An eligible non-author approver, approval time, and exact `Approval Evidence: Approve` are recorded; any optional Approval Context Revision is informational, non-binding, and exactly represents the approved document | ADR metadata | Complete | @hailingu replied with exact Approve for this service record; metadata records 2026-10-15T11:24:20Z and the same previously self-declared identity |
| A-2 | Complete task delivered | Every declared subtask has actual implementation evidence, every applicable acceptance check is `Pass` with actual result and evidence, and together they satisfy the complete task outcome | Implementation Plan and Acceptance Checks rows | Complete | T-1 Complete; AC-1 through AC-3 Pass with source identities, full case inventory, ownership review, compiled behavior suites and proposal-evidence verification |
| A-3 | Reciprocal ADD link synchronized, when applicable | The selected candidate records this exact ADR path, this ADR records the exact ADD path and candidate ID, both references agree, and the candidate reaches `Complete` only with this ADR's `Complete` or `Verified` status | Exact ADD path, candidate ID, ADR path, and Git blob or commit | N/A — maintenance, not product-demand implementation | N/A — Architecture Source is reasoned N/A; no ADD selection or relink |
| A-4 | Requirement levels satisfied | Every required section is complete, every conditional trigger is assessed and completed or marked `N/A — <reason>`, and optional sections are complete or removed | Structured document review | Complete | Structured completion review confirms all required content, conditional applicability, retained optional evidence, one service boundary/subtask and full-path index identity; no unresolved required content |
| A-5 | Acceptance checks are decidable | Every check names one subtask, preconditions or input, deterministic method, exact expected result, and evidence; no unqualified subjective criterion remains | Structured acceptance-check review | Complete | All three predetermined checks retain their approved inputs, methods and binary results, with actual results and reproducible command/source evidence |
| A-6 | Engineering exceptions governed, when applicable | Every exceeded or waived engineering rule has one complete exception row, an accountable owner, a lifecycle, and verification evidence before approval; otherwise the conditional subsection records `N/A — <reason>` | Engineering Exceptions subsection and affected-file evidence | N/A — no engineering cap waiver proposed | Threshold reviews and resulting size requirements are recorded in Implementation Plan |
| A-7 | Contract and baseline risks covered | Every normative contract clause maps to an explicit check or deterministic test, and every required Risk Coverage Matrix row is complete before approval and reaches Pass or specific N/A before review-ready or completion | Contract-To-Check Traceability, Risk Coverage Matrix, acceptance checks, and stable evidence | Complete | HT-1 through HT-4 map to AC-1 through AC-3; all three applicable risk rows Pass with actual case/allocation/CTest evidence, other two rows retain specific N/A reasons |
| A-8 | Governance validation passed | The independent validator reports no required-section, template-field, lifecycle-status, index, reciprocal-link, or Mermaid contract error for this record and repository | `npm run validate --prefix tools/governance-validator` output | Complete | npm run validate --prefix tools/governance-validator exits 0; final terminal-state validation and scoped structured review are recorded in the Change Log |

## Archival [Conditionally Required — Decision Status is `Rejected`, or Decision Status is `Deprecated` or `Superseded` and Implementation Status is final]

This record is Accepted and has not been retired. Retain the following as
inactive future-lifecycle guidance; it does not affect proposal approval or
implementation completion. When the archival trigger in the heading applies:

- [ ] Move this record to `service/docs/adr/archive/ADR-0001-http-message-test-decomposition.md`.
- [ ] Update every source marker and reference to the final archived path in the same change.
- [ ] Preserve reciprocal replacement paths when superseded; otherwise retain Superseded By: None.
- [ ] Update this record's single row in `docs/adr/INDEX.md` with its final path, scope and statuses.
- [ ] Confirm no active record or code marker still cites the former path.

## Change Log [Required]

| Date | Change | Author |
| --- | --- | --- |
| 2026-10-15 | Draft a separate Proposed repair for quality-review finding 4, with one implementation boundary, deterministic acceptance checks, contract traceability and five-dimension risk assessment. The owner explicitly waived serialization for preparing findings 1, 2, 4 and 5 in this chat; individual approval before implementation remains required. No implementation is authorized or claimed | @codex |
| 2026-10-15 | Proposal verification: npm test --prefix tools/governance-validator passes 208/208; npm run validate --prefix tools/governance-validator and git diff --check pass. Four disposable Accepted-stage preflights pass without changing repository approval state. A fresh C++ verification build with Apple clang 21.0.0 passes 2/2 CTest targets without warnings; clang-format check passes. The build used a task-owned temporary directory to preserve the existing service/build output. Structured planning review covers requirement levels, routing and per-root number uniqueness, one boundary/subtask per record, exact acceptance inputs and outputs, contract/risk traceability and truthful unexecuted implementation checks | @codex |
| 2026-10-15 | Record owner @hailingu's exact Approve at 2026-10-15T11:24:20Z in the chat identifying this service record; Decision Status becomes Accepted and Implementation Status becomes In Progress. Synchronize the central index and begin T-1 with existing-suite characterization before source extraction. The separate runtime proposal remains unapproved | @codex |
| 2026-10-15 | Complete and verify T-1 under the recorded approval: extract fourteen cohesive local functions, retain all 84 inline assertion sites and six existing helpers unchanged, and reduce main from 307 to 24 code lines. Before/after CTest passes 2/2 without warnings; format/diff checks pass; governance suite passes 271/271 and validation passes. Record source content IDs, case/ownership/size review and evidence-only removal of the old runtime exception; runtime approval/status metadata and reciprocal ADD link remain unchanged. Synchronize Verified status in the central index; remove all task-owned disposable verification output before completion | @codex |
