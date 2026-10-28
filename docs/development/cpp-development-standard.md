<!-- Governed by docs/adr/ADR-0004-engineering-standards.md -->
# C++20 Development Standard

**Applies to**: every C++ compilation unit in this repository (initially
`service/**`, the loopback compute service). Like the root
[`AGENTS.md`](../../AGENTS.md), this file is written in English for agent
interoperability.

**Last reviewed**: 2026-10-20

## Required Reading

- [Software Engineering Standard](software-engineering-standard.md) — the
  repository-wide baseline for size, module boundaries, dependency
  direction, abstractions, testing, and exceptions. This file narrows it
  for C++ and never weakens it.
- The C++ Core Guidelines — the canonical reference for naming, ownership,
  interfaces, and error handling judgment calls not fixed below.
- [AGENTS.md](../../AGENTS.md) — governance, verification routing, and
  commit policy.

## Baseline Practices

- Language and compiler: C++20 (`-std=c++20`). The toolchain is the host
  Apple clang (record the exact `clang++ --version` in the implementing
  change's verification evidence); introducing another compiler or platform
  target is a governed decision, not a convenience.
- Build system: CMake 3.21+. The verification trio, run from the repository
  root, is authoritative for every C++ change:
  `cmake -S service -B service/build`, then `cmake --build service/build`,
  then `ctest --test-dir service/build --output-on-failure`. Build
  directories are disposable output and never committed.
- Formatting: clang-format with the repository configuration file that lands
  with the first C++ implementation change. Do not hand-format around it;
  formatting fixes are their own commit or ride along mechanically.
- Warnings: build with warnings enabled; a new warning in maintained code is
  a defect to fix, not to suppress. Narrow suppressions require an in-place
  comment stating why.
- Development is test-first (AGENTS.md): the failing test for the missing
  behavior lands before the production change, for features and reproducible
  defect fixes alike.

## Project Layout

- `service/src/` — production sources: one capability per file, named in
  `snake_case.cpp`/`.hpp` after the responsibility (for example
  `http_messages`, `http_server`, `routes`).
- `service/tests/` — the CTest suites: unit tests next to the wire format
  and parsers, integration tests that drive real sockets against a server
  started on an ephemeral loopback port.
- `service/CMakeLists.txt` — the single build description; keep targets
  small and explicit (`orange_http` library, one executable per test suite,
  later the server executable).
- Namespaces follow `orange::<area>` (for example `orange::http`); namespace
  components mirror the directory area. Names are descriptive; no
  abbreviation dialect that a reviewer must learn.
- Every public header and every public symbol receive intent-bearing
  documentation before the implementation body (AGENTS.md Core Rules);
  translation units include their own header first, then project headers,
  then the standard library, then system headers — the LLVM convention
  direction, with project headers before system headers (owner-directed
  choice, 2026-10-14; see Rule Provenance).

## Error Handling And Contracts

- No exceptions across module API boundaries. Recoverable failures return
  `std::optional<T>` or `std::variant<Result, Error>` with an error value
  owned by the failing layer; the wire-format parser and the event loop are
  the reference patterns.
- Assertions assert only provably local invariants; a failed assertion is a
  program defect, never an input-handling path. External input is validated
  and answered with coded diagnostics (ADD-0001 CON-07), never asserted
  against.
- System-call failure paths are handled explicitly: check returns, translate
  `errno` into the layer's error value, and never ignore a failure silently.
- Error types stay close to their layer; preserve the original cause when
  adding context. One unstructured error variant for unrelated failure
  classes is prohibited.

## Resource, Concurrency, And Time

- Ownership is explicit RAII: no raw owning pointers, no manual `new`/`delete`
  in maintained code; use values, `std::unique_ptr`, or references with
  documented lifetime.
- Sockets and descriptors are wrapped in owning types whose destructors
  close them; a leak on an error path is a defect with a regression test.
- The runtime concurrency model is the `kqueue()` acceptor loop (ADR-0003):
  readiness-driven, non-blocking sockets, one connection state machine per
  descriptor. Per the ADR's RT-1 revision, `LoopbackServer` may be constructed
  with up to 64 acceptor threads, each running one independent shared-nothing
  kqueue loop over an SO_REUSEPORT listener group — no mutable state crosses
  loops, an unrecoverable loop failure raises the shared abort flag so its
  siblings stop, and the request handler must be safe for concurrent calls
  when several acceptors run. One acceptor remains the default construction,
  and the poll()-era single-threaded comparison control is unchanged. Any
  further threading beyond this accepted shape requires a governed revision
  that records lock ownership, ordering, contention, and failure behavior.
- Time uses `<chrono>` types end to end; `time()`-family calls and
  wall-clock arithmetic on integers are not accepted in maintained code.
- Shutdown is explicit: the event loop observes its stop flag, closes every
  descriptor it owns, and returns a status; at-exit implicit cleanup of open
  sockets is not a design.

## Testing

- Unit tests exercise the wire format, parsing, and pure logic directly;
  integration tests start a real listener on `127.0.0.1` with an ephemeral
  port and drive it with POSIX clients, because framing, cancellation,
  timeout, and descriptor ownership only verify at the real boundary
  (Software Engineering Standard, Testing And Change Design).
- Every declared protocol behavior in the governing ADR — keep-alive
  lifecycle, chunked decode and encode, range responses, input caps, idle
  timeout — has a named passing test before its acceptance check records
  `Pass`.
- Tests are deterministic: fixed inputs, seeded values where randomness
  would appear, and deadlines (`SO_RCVTIMEO`) so a broken server fails fast
  instead of hanging the suite.
- Test helpers live in the test tree, never in production headers; a test
  seam in production code must be justified as a boundary, not as
  convenience.

## Dependencies

- The default dependency set is the C++ standard library and POSIX. Every
  third-party library requires an accepted ADR and an exactly pinned version
  through the `vcpkg.json` manifest (`service/vcpkg.json` when first
  needed; ADR-0003 pins OpenSSL for TLS).
- Cryptography is never hand-written: TLS and any future crypto use the
  approved OpenSSL dependency, while the protocol layer around it remains
  hand-written (ADR-0003 Decision).
- Vendoring source copies of libraries is prohibited; pin manifests, do not
  paste.

## Before Writing Code

Read this file and the Software Engineering Standard in full, then inspect
the target area for existing state machines, error types, and test helpers.
Answer the baseline checklist, then additionally:

- Which layer owns the failure being handled — parse, transport, route, or
  caller?
- Does the change keep the event loop single-threaded and non-blocking?
- Which declared protocol behavior or invariant does each new test pin?
- Is every descriptor, buffer, and timeout bounded and owned?

## Appendix: Rule Provenance (Informative)

This appendix records the evidence that the rules above conform to
authoritative C++ development standards and best practices. It is
**informative, not normative**: it adds no rule and changes none; every
citation was verified against the official online texts on 2026-10-14.
Evidence layers: ISO C++ standard library; the C++ Core Guidelines
(isocpp official); flagship industrial style guides (Google, LLVM);
official tool documentation; and orange project governance, which is
labeled honestly as a governance choice rather than an industry norm.

| Rule in this standard | Verified authority |
| --- | --- |
| RAII ownership; no raw owning pointers; no explicit `new`/`delete` | C++ Core Guidelines **R.1** (Manage resources automatically using resource handles and RAII), **R.11** (Avoid calling `new` and `delete` explicitly), R.20 (Use `unique_ptr` or `shared_ptr` to represent ownership) |
| Descriptor-owning types; leak-on-error-path is a defect | Direct corollary of R.1 (destructor release) |
| A `.cpp` includes its own header first; headers are self-contained | C++ Core Guidelines **SF.5** (A .cpp file must include the header file(s) that defines its interface), **SF.11** (Header files should be self-contained); file naming follows the SF section |
| Naming follows the NL section conventions | C++ Core Guidelines **NL** (Naming and Layout) section; descriptive names as in Google/LLVM conventions |
| `.cpp` / `.hpp` suffixes | C++ Core Guidelines **SF.1** (Use a `.cpp` suffix for code files and `.h` for interface files *if your project doesn't already follow another convention*) — the qualifier sanctions a project-wide `.hpp` convention for a C++-only codebase; Guidelines issues #1408 and #2265 document the community push toward `.hpp`; lowercase-with-underscores file names per the Google C++ Style Guide |
| `src/` + `tests/` layout with a root `CMakeLists.txt` | Aligns with the Pitchfork convention (both split and merged test placements are sanctioned; Beman's template follows the same `src`/`tests` tree). See the Pitchfork alignment record below |
| Include order: own header, then project, then standard library, then system | Own-header-first is the universal core (SF.5 exposes hidden header dependencies at compile time). The tail order follows the **LLVM Coding Standards** direction (main header, local/project headers, then system headers); the Google C++ Style Guide orders project headers last — two recognized conventions with the same goal. Owner-directed LLVM choice, 2026-10-14 |
| `<chrono>` end to end; no integer wall-clock arithmetic | C++ Core Guidelines **I.4** (Make interfaces precisely and strongly typed) — the guideline's own example types time units with `std::chrono` durations |
| Concurrency rules (data races, locks, sharing) | C++ Core Guidelines **CP.1** (Assume that your code will run as part of a multi-threaded program), **CP.2** (Avoid data races), CP.3, CP.20 (Use RAII locks only), CP.21 |
| Assertions only for local invariants; external input gets coded diagnostics | C++ Core Guidelines E-section layering — assertions express unreachable states, error handling expresses expected failures; boundary validation per `AGENTS.md` Security |
| System calls checked explicitly | POSIX conventions; CERT C++ Secure Coding |
| Machine formatting with clang-format | LLVM official tooling; Google and LLVM both mandate machine formatting over hand-formatting |
| CMake/CTest verification trio | Official CMake and CTest documentation (cmake.org) standard workflow |
| Cryptography never hand-written | `AGENTS.md` Security ("Use approved libraries for cryptography"); CERT cryptographic rules |
| Dependencies exactly pinned via manifest | Supply-chain reproducible-build best practice (governance choice; PlotWeave-calibrated) |

### Error-policy provenance (the one deliberate narrowing)

The C++ Core Guidelines default to exceptions — **I.10** (Use exceptions to
signal a failure to perform a required task) — while the same rule
acknowledges value/error-code returns as an alternative and **E.1** requires
an error-handling strategy fitted to the component. This standard's
no-exceptions-across-boundaries rule is a deliberate narrowing under that
authority, with flagship precedent:

1. The Google C++ Style Guide states verbatim, **"We do not use C++
   exceptions."** — its rationale (whole-codebase exception-safety cost,
   migration cost) matches orange's greenfield-local posture.
2. LLVM propagates failures through `Expected<T>`-style types instead of
   exceptions — the same design in an industrial codebase.
3. ISO C++ provides the sanctioned non-throwing channels this standard
   uses: `std::optional` ([optional]) and `std::variant` ([variant]),
   both C++17 and available under the pinned C++20 profile; the C++23
   `std::expected` (P0323) continues that line and is unavailable here only
   because the language level is pinned.

### Governance choices (not industry norms)

Toolchain pinning and the exact verification-command shape, the
shared-nothing `kqueue()` acceptor loops (an ADR-0003 accepted decision —
one loop by default, up to 64 acceptor threads over an SO_REUSEPORT
listener group under the RT-1 revision — that satisfies CP.2 by
construction), documentation-before-implementation and
test-first development (`AGENTS.md` Core Rules), and the numeric guardrails
inherited from the PlotWeave-calibrated baseline (ADR-0004 Q-1 owner
determination) are orange governance decisions. They are recorded here as
choices, not as external best practice.

### Pitchfork alignment record

The `src/` + `tests/` + root `CMakeLists.txt` shape of Project Layout
aligns with the Pitchfork convention, but **Pitchfork is not adopted as a
governing standard of this repository** — the standard's layout rules are
self-contained, and this alignment is recorded as evidence, not as a
dependency. Consequences of not adopting Pitchfork, recorded deliberately
(the omission itself is a decision, owner-directed 2026-10-14):

- The `include/<project>/` public-header split is not prescribed. Headers
  live beside sources (Pitchfork's merged layout, sanctioned for
  internal/non-installed targets); introducing an installed public library
  would revisit this.
- `CMakePresets.json` is not required. It remains an optional future
  addition; adopting it, or adopting Pitchfork formally, is a governed
  revision of this standard rather than a convenience change.
- Pitchfork's data-layout rules were verified against the spec on
  2026-10-14; the two sanctioned test placements (separate `tests/`, or
  merged under `src/`) both cover orange's choice.

Sources verified 2026-10-14:

- C++ Core Guidelines: <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines>
  and <https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md>
- Google C++ Style Guide (Exceptions decision):
  <https://google.github.io/styleguide/cppguide.html>
- C++ Core Guidelines Explained (Rainer Grimm), CP rules:
  <https://ptgmedia.pearsoncmg.com/images/9780136875673/samplepages/9780136875673_Sample.pdf>
