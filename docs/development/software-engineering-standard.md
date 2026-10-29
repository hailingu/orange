<!-- Governed by docs/adr/ADR-0004-engineering-standards.md -->
# Software Engineering Standard

**Applies to**: all maintained production and test source code in this
repository — the C++20 compute service, the TypeScript web client, and the
governance tooling. Read this file together with every matching language
standard. Like the root [`AGENTS.md`](../../AGENTS.md), this file is written
in English for agent interoperability.

**Last reviewed**: 2026-10-14

## Purpose And Precedence

This standard governs software structure and maintainability across
languages. Language standards define how these rules map to native modules,
types, components, interfaces, and tools; a language standard may narrow this
baseline but must not weaken or contradict it.

Accepted ADRs, the architecture baseline (ADD-0001), public contracts,
security requirements, and platform constraints take precedence when they
bind a design. Record and resolve a conflict in the governing record instead
of silently choosing one rule over another.

orange is a single-user, local, learning-purpose product (ADD-0001 D-03,
D-06): rules scale to that reality — one learner, one machine, loopback-only
services — and grow only through a governed revision of this standard.

## Size And Complexity Guardrails

Size is a review signal, not a substitute for judging cohesion. Crossing a
review threshold requires an explicit decomposition review in the change that
introduces or materially modifies the unit.

| Unit | Decomposition-review threshold | Cap |
| --- | --- | --- |
| Maintained production source file | More than 600 physical lines | 800 physical lines |
| Maintained test source file | More than 1,000 physical lines | 1,800 physical lines |
| Function, method, closure, component, or equivalent executable unit | More than 60 code lines | 80 code lines (comments excluded) |
| Cyclomatic complexity, when measured by configured tooling | More than 10 | N/A — record and decompose |
| Executable nesting depth | More than 4 levels | 6 levels |

**Enforcement is review-based today.** orange has no automated file-size or
complexity check yet; adding one to the governance validator is a named
future governed change, and until it lands every cap above is enforced in
structured review. The absence of a measuring tool never means a threshold
passed: when no tool measures complexity, record `N/A — no configured
complexity tool` and use executable-unit line span plus nesting depth as the
required substitute signals.

Apply the guardrails as follows:

- Count a file's complete physical span, including comments and
  documentation, because that is what a reviewer must navigate. Count an
  executable unit's span from its signature through the end of its body,
  excluding comment lines anywhere in the span; blank lines still count.
- Treat every measurement as point-in-time evidence for the inspected
  revision. Re-run the review when an affected change crosses a threshold; do
  not rewrite historical evidence after unrelated edits.
- Generated, vendored, lock, snapshot, fixture-data, and immutable
  migration-history files are excluded from the numeric limits. Their source
  or generating workflow remains subject to review.
- Declarative registries, protocol declarations, and exhaustive test tables
  are not automatically exempt; they may remain large when the decomposition
  review demonstrates one cohesive responsibility.
- Crossing a review threshold does not require a mechanical split: record
  why the unit is cohesive and why extraction would worsen coupling,
  readability, ordering, or lifecycle safety.
- Do not satisfy a limit by moving unrelated code into a generic helper,
  creating pass-through wrappers, or splitting one operation into files that
  must always be read and changed together.

## Responsibility And Module Boundaries

- A module or component owns one coherent capability and has one primary
  reason to change. Name it after that responsibility, not an incidental
  implementation detail.
- Organize by feature or domain at the main boundary; use technical layers
  inside a feature only when separation creates a meaningful dependency,
  failure, ownership, or test boundary.
- Separate transport delivery, application orchestration, domain policy,
  persistence, and external-provider integration when they vary, fail, or are
  tested independently (the ADD-0001 component boundaries C-01 through C-12
  are the reference map).
- Keep the public surface minimal and intentional. Internal details must not
  become public to make tests or imports convenient.
- A new module needs a clear owner, inputs, outputs, invariants, and error
  behavior. Public modules and declarations require intent-bearing
  documentation before their implementation bodies (AGENTS.md Core Rules).
- Generic `utils`, `helpers`, `common`, `shared`, or `misc` dumping grounds
  are prohibited. Shared code needs a specific capability name and must
  satisfy the extraction criteria in Abstractions And Design Patterns.
- Prefer colocating code that changes together; split when responsibilities
  or change lifecycles differ, not merely because a directory looks large.

## Dependency Design

- Dependencies point from delivery and infrastructure adapters toward stable
  application or domain contracts. Domain computation (expression
  evaluation, matrix computation, exercise synthesis and grading) must not
  depend on HTTP delivery, browser, persistence, or provider details
  (ADD-0001 INV-01).
- Cyclic dependencies between maintained modules are prohibited. Resolve a
  cycle by clarifying ownership, extracting a stable contract, or moving
  shared policy to the module that owns it.
- Contract types shared across an internal boundary live in a dedicated leaf
  module both sides import; type ownership must not attach to an application
  entry point.
- Cross-module calls use the owning module's public API. Do not import
  another module's internal state, framework objects, or private helpers.
- Translate external request, response, and provider types at the boundary;
  do not let them become the repository-wide domain model by convenience.
- Third-party dependencies are exactly pinned and require an accepted ADR
  (AGENTS.md Decision Records). Prefer the standard library and the smallest
  pinned dependency that satisfies the accepted scope.

## Abstractions And Design Patterns

- Start with the simplest direct design that preserves the required
  boundary. Introduce an abstraction only for demonstrated variation,
  independent lifecycle, external I/O, reusable policy, or a necessary test
  seam.
- Interfaces belong at the boundary that consumes the behavior. Do not
  create one interface per concrete type or a factory for a single direct
  construction path.
- Before applying a named pattern, identify the recurring problem, the
  forces it resolves, the expected variation, and the simpler alternative
  considered. The pattern is an implementation tool, not a target
  architecture.
- Prefer composition and explicit delegation over inheritance; inheritance
  only for a genuine substitutable relationship with preserved invariants.
- Use dependency injection explicitly through constructors or parameters.
  Service locators and mutable global singletons are prohibited unless an
  approved engineering exception defines their bounded lifetime.
- Extract shared behavior when at least two real consumers have the same
  semantics and expected evolution. Similar syntax with different meaning
  stays separate.

### Pattern Selection Guide

| Pattern or approach | Appropriate problem | Do not use it merely to |
| --- | --- | --- |
| Adapter / anti-corruption layer | Translate an external API, provider, or storage model into an owned contract | Rename fields while leaking external semantics into the core |
| Strategy | Select between behaviorally meaningful algorithms behind one consumer-owned contract | Hide one implementation or replace a simple conditional |
| Explicit state machine | Model a finite lifecycle with guarded transitions, retries, cancellation, or terminal outcomes | Distribute state checks across handlers without one transition owner |
| Repository | Give domain logic collection-like access to persisted aggregates | Wrap every table without domain semantics |
| Domain/application service | Own policy or orchestration that belongs to no single value | Create broad `Service`/`Manager` classes of unrelated use cases |
| Event / observer | Notify independent consumers across an asynchronous boundary with defined delivery semantics | Replace a direct call when ordering and failure are unspecified |
| Middleware / decorator | Apply ordered cross-cutting boundary behavior | Hide core rules or an untraceable execution order |
| Factory / builder | Centralize construction that varies by type or enforces several invariants | Avoid a clear constructor or add indirection for one fixed type |
| Facade | Present a small stable API over a complex owned subsystem | Add a pass-through layer with no ownership or policy value |
| Ports and adapters | Isolate a substantial core from volatile delivery or provider technologies | Layer a small script or simple feature with no independent domain |

## State, Concurrency, And Failure Boundaries

- Every mutable state value has one clear owner; transitions are explicit,
  validated, and observable where failure matters.
- Side effects are visible in APIs and live at controlled boundaries.
  Domain calculations are deterministic and pure where the architecture
  demands it (INV-01, INV-03).
- Define cancellation, timeout, retry, idempotency, and partial-success
  behavior before adding concurrent execution (ADD-0001 CON-12 is the
  product-level deadline map).
- Handle errors at the layer that can add context or choose recovery;
  preserve the cause and use structured, coded diagnostics (CON-07). Do not
  log-and-rethrow the same failure at every layer.
- Validate external input at trust boundaries; after validation, maintain
  valid internal types and invariants rather than propagating uncertain
  external state (AGENTS.md Security).

## Testing And Change Design

- Develop source features and reproducible defect fixes with
  Red-Green-Refactor as AGENTS.md requires: first the smallest focused test
  for the missing behavior, observe the expected failure, then implement and
  keep the focused and routed suites green. Documentation-only changes
  undergo structured review instead.
- Test observable behavior and stable contracts; avoid coupling to private
  call order or incidental data structures unless that detail is the
  contract.
- When an applicable risk belongs to a production framework, datastore,
  provider transport, or other external adapter, at least one linked check
  exercises that production boundary or a behaviorally equivalent
  integration harness; a unit double that bypasses protocol framing,
  cancellation, timeout, or resource ownership cannot alone verify the
  boundary.
- Keep unit tests near their owning module per language convention;
  integration tests live at the boundary they exercise.
- A defect fix adds the smallest regression test that fails for the reported
  behavior when reproduction is feasible.
- A module split is complete only when its tests, ownership, public API, and
  dependency direction reflect the new boundary.
- Reuse an existing component, client, fixture, or helper with the same
  ownership and semantics before creating a parallel abstraction.

## Incremental Adoption And Exceptions

New code complies immediately with the guardrails above. Existing code is
assessed when materially changed; do not expand an unrelated task to
remediate old thresholds — record the observed debt, its affected path or
symbol, and the reason it stays out of scope in the pull request that
touched it.

An engineering exception for any guardrail in this standard must be recorded
in the governing ADR's Engineering Exceptions subsection (the orange
instrument; see the ADR templates) with: the exact rule and affected path or
symbol; the measured value or structural condition; why the unit remains
cohesive or why compliance is unsafe now; the risks of retention;
compensating controls such as focused tests; one accountable owner; and a
removal or review date, or a specific permanent rationale. Revisit the
exception whenever the unit is materially changed.

## Before Writing Code

Read this file and every matching language or platform standard listed in
[`docs/README.md`](../README.md) in full. Then inspect the affected module
and answer:

- Which module owns the behavior and its invariants?
- Which responsibilities change together, and which vary independently?
- What is the permitted dependency direction?
- Is an existing internal capability semantically suitable?
- Does the design cross a review threshold or require an engineering
  exception?
- What observable contract and failure behavior will the tests verify?
