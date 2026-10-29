<!-- Cataloged with the first web client source under docs/adr/ADR-0003-local-runtime-path.md (T-3). -->
# TypeScript Development Standard

**Applies to**: every TypeScript compilation unit in this repository
(initially `web/**`, the orange web client). Like the root
[`AGENTS.md`](../../AGENTS.md), this file is written in English for agent
interoperability.

**Last reviewed**: 2026-10-29

## Required Reading

- [Software Engineering Standard](software-engineering-standard.md) — the
  repository-wide baseline for size, module boundaries, dependency
  direction, abstractions, testing, and exceptions. This file narrows it
  for TypeScript and never weakens it.
- [`AGENTS.md`](../../AGENTS.md) — governance, verification routing, and
  commit policy.

## Baseline Practices

- Language and compiler: TypeScript in `strict` mode compiled by the
  `typescript` version exactly pinned in `web/package.json` (locked through
  `package-lock.json`); introducing another compiler, a bundler, or a
  framework is a governed decision, not a convenience.
- Runtime target: ES2022 modules loaded by the browser as `type="module"`
  scripts; Node-facing test builds compile under `module: Node16`. The
  declared platform is desktop Chromium/Firefox/Safari/Edge — the two most
  recent major versions (ADD-0001 CON-06).
- Build: `npm run build --prefix web` compiles `src/` to `web/dist/` and
  copies `index.html` beside the output; `dist/` and `.test-out/` are
  generated and never committed.
- Tests: `npm test --prefix web` recompiles with the test configuration and
  runs `node --test` over the compiled suites; development is test-first
  (AGENTS.md): the failing test for missing behavior lands before the
  production change.
- Formatting and linting have no configured tool yet; match the surrounding
  style, and adding a formatter/linter is a named governed change.

## Project Layout

- `web/src/` — production modules, one capability per file, named in
  `kebab-case.ts` or `status.ts`-style responsibility names; no `utils` or
  `helpers` dumping grounds (Software Engineering Standard).
- `web/test/` — `node --test` suites beside their modules, importing the
  compiled `../src/<module>.js` path.
- `web/index.html` — the served page source copied into `dist/` at build
  time; the service serves `web/dist/` (ADR-0003).
- Every exported function, class, type, and module receives intent-bearing
  documentation before its implementation (AGENTS.md Core Rules).

## Types And Module Boundaries

- `strict` plus `noUnusedLocals`, `noUnusedParameters`,
  `noImplicitOverride`, and `exactOptionalPropertyTypes` stay on; `any` and
  non-null assertions require an in-place comment stating why.
- Prefer structural interfaces at boundaries — the status module's
  `FetchLike`/`StatusElement` are the reference pattern: minimal surfaces
  that keep modules runnable under `node --test` without a DOM.
- Domain types live with the module that owns them; shared types move to an
  owning leaf module only with two real consumers (Software Engineering
  Standard, Abstractions).
- Dependencies point from presentation toward contracts; third-party
  runtime dependencies require an accepted ADR — the default is the standard
  library and the platform.

## Asynchronous And Error Handling

- Every promise is awaited or explicitly voided with a comment; floating
  promises are defects.
- Fail-fast budgets (ADD-0001 CON-12): user-facing requests abort themselves
  inside their declared deadline (the status page's 1.5 s inside AC-8's
  two-second budget) and render a coded error state — a hung request may
  never leave a loading indicator on screen.
- Rendered state is derived from explicit state values (`data-state`
  attributes), never from transient styling alone; superseded or failed
  renders replace, never accumulate (ADD-0001 INV-18).
- No `try`/`catch` around code that cannot throw; a caught error either
  changes observable behavior or is a defect.

## Testing And Change Design

- Test observable behavior through the module's exported surface with
  injected doubles (`FetchLike`); no mocking of module internals.
- Timing-sensitive assertions state their budget and assert against it (the
  abort test's two-second bound), never against wall-clock exactness.
- A defect fix adds the smallest failing test for the reported behavior when
  reproduction is feasible (Software Engineering Standard).
