<!-- Governed by docs/adr/ADR-0004-engineering-standards.md -->
# C++20 Development Standard

**Applies to**: every C++ compilation unit in this repository (initially
`service/**`, the loopback compute service). Like the root
[`AGENTS.md`](../../AGENTS.md), this file is written in English for agent
interoperability.

**Last reviewed**: 2026-10-14

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
  then the standard library, then system headers.

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
- The runtime concurrency model is the single-threaded `poll()` event loop
  (ADR-0003): readiness-driven, non-blocking sockets, one connection state
  machine per descriptor. Introducing threads requires a governed revision
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
