<!-- Governed by docs/adr/ADR-0002-governance-validator-adoption.md -->
# Governance Validator

The validator enforces the repository's deterministic ADD, ADR, and OCR
structure and lifecycle contracts under `AGENTS.md`. Run it with the commands
documented in `docs/README.md`:

- `npm test --prefix tools/governance-validator`
- `npm run validate --prefix tools/governance-validator`

Both are executed from the repository root. The validator is source work
under its AGENTS.md routing row: every behavioral edit lands its failing test
first and dependencies stay exactly locked.

Adopted and localized by
`docs/adr/ADR-0002-governance-validator-adoption.md` from a proven
same-contract implementation owned by the repository owner. The inherited
208-test suite is the behavior anchor for localization. The source system's
2026-08-14 point-in-time decomposition review is not maintained in this
repository; re-measure locally before citing any unit-size limit.
