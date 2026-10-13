<!-- Restored per the acceptance of docs/adr/ADR-0001-orange-issue-governance.md. -->
# orange Documentation

This file is the documentation navigation and standards catalog referenced by
[AGENTS.md](../AGENTS.md): before writing, modifying, or reviewing source code
or infrastructure configuration, read this file and every matching language or
platform standard listed in the catalog below in full. AGENTS.md remains the
authoritative agent guide; this file only routes readers to the documents that
exist and defers lifecycle status to the indexes below.

## Layout

| Path | Contents |
| --- | --- |
| `docs/adr/` | Decision records: Full and Lightweight ADRs and OCRs, with their index, templates, and review fixtures |
| `docs/architecture/` | Architecture Design Documents (ADDs), with their index and shared template |
| `docs/development/` | Development standards for repository work |

## Decision Records — `docs/adr/`

- [`docs/adr/INDEX.md`](adr/INDEX.md) — the single repository-wide index of
  every project and service ADR and OCR, active and archived. The
  repository-relative path is the record identity; consult it for current
  statuses rather than enumerating records here.
- [`docs/adr/template/0000-template.md`](adr/template/0000-template.md) —
  Full ADR template.
- [`docs/adr/template/0000-lightweight-template.md`](adr/template/0000-lightweight-template.md) —
  Lightweight ADR template.
- [`docs/adr/template/0000-operational-change-template.md`](adr/template/0000-operational-change-template.md) —
  Operational Change Record (OCR) template.
- [`docs/adr/fixtures/`](adr/fixtures/) — fixed constructed inputs cited by
  ADR acceptance checks.

## Architecture Design — `docs/architecture/`

- [`docs/architecture/INDEX.md`](architecture/INDEX.md) — the repository-wide
  index of active and archived ADDs at every scope level.
- [`docs/architecture/template/0000-add-template.md`](architecture/template/0000-add-template.md) —
  the shared ADD template.

## Development Standards — `docs/development/`

- [`docs/development/issue-standard.md`](development/issue-standard.md) —
  issue title, severity, label, body, evidence, and closure rules for the
  `hailingu/orange` tracker, governed by
  [`docs/adr/ADR-0001-orange-issue-governance.md`](adr/ADR-0001-orange-issue-governance.md).

## Language And Platform Standards Catalog

orange's backend is C++20 and its frontend is TypeScript. No repository-owned
C++20, TypeScript, or general software-engineering standard has been authored
yet. When one is added under `docs/development/`, list it here with its scope
so the AGENTS.md required-reading rule can find it. Until then this catalog
has no language or platform entries, and AGENTS.md plus the documents routed
above are the complete documentation baseline.

## Verification Tooling

The configured governance validator at `tools/governance-validator/` does not
exist yet, so the `npm test --prefix tools/governance-validator` and
`npm run validate --prefix tools/governance-validator` checks referenced by
AGENTS.md Scope Routing cannot run. This is the recorded open prerequisite of
ADR-0001 (AC-06) and is not a passing state; structured review supplements it
until the validator is supplied.
