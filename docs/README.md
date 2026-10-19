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

- [`docs/development/software-engineering-standard.md`](development/software-engineering-standard.md) —
  the repository-wide engineering baseline: size and complexity guardrails,
  module boundaries, dependency direction, abstraction guidance, testing,
  and the engineering-exception process.
- [`docs/development/cpp-development-standard.md`](development/cpp-development-standard.md) —
  the C++20 standard narrowing the baseline for `service/**`: toolchain,
  verification commands, error policy, resource and event-loop rules, and
  testing.
- [`docs/development/issue-standard.md`](development/issue-standard.md) —
  issue title, severity, label, body, evidence, and closure rules for the
  `hailingu/orange` tracker, governed by
  [`docs/adr/ADR-0001-orange-issue-governance.md`](adr/ADR-0001-orange-issue-governance.md).

## Language And Platform Standards Catalog

This catalog is what the AGENTS.md required-reading rule resolves: before
writing, modifying, or reviewing source code, read every entry below whose
scope matches the affected paths, in full.

| Scope | Matching standard |
| --- | --- |
| All maintained source and test code, any language | [`docs/development/software-engineering-standard.md`](development/software-engineering-standard.md) |
| C++20 compilation units (`service/**`) | [`docs/development/cpp-development-standard.md`](development/cpp-development-standard.md) |

TypeScript does not yet have a language standard; when web client source
first lands, its standard is authored and cataloged in the same governed
change that adds its Scope Routing row.

## Verification Tooling

The configured governance validator is available at
[`tools/governance-validator/`](../tools/governance-validator/README.md), adopted
under [ADR-0002](adr/ADR-0002-governance-validator-adoption.md). Its configured
checks, executed from the repository root, are:

- `npm test --prefix tools/governance-validator`
- `npm run validate --prefix tools/governance-validator`

The original missing-tool prerequisite of ADR-0001 (AC-06) has been resolved;
the decision-record index records its completed status. Automated validation
supplements the structured review required by AGENTS.md.
