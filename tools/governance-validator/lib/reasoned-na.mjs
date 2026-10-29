// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.

// Recognizes the repository's only valid table-level omission form: a
// reasoned N/A using the required em dash delimiter.
export function isReasonedNa(value) {
  return /^N\/A\s+—\s+\S/.test(value);
}
