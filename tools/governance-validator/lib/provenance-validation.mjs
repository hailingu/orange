// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
// Validates explicit source comment provenance; ordinary strings are not markers.
import { readFileSync } from "node:fs";
import { recordPathsFromValue, validRecordLocation } from "./record-identity.mjs";
import { resolveRepositoryFile } from "./repository-file.mjs";

// Limits marker reads to maintained text formats that support comments.
export function supportsProvenance(path) {
  return /\.(?:[cm]?js|tsx?|cpp|cc|c|hpp|hh|h|py|sh|ya?ml|toml|cmake)$/.test(path)
    || /(?:^|\/)(?:CMakeLists\.txt|Makefile)$/.test(path);
}

// Restricts file provenance to the leading comment position required by
// AGENTS.md, skipping shebangs and blanks without interpreting source examples.
function* provenanceHeader(content) {
  let block = false;
  for (const line of content.split("\n")) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#!")) continue;
    if (block || trimmed.startsWith("/*")) {
      block = !trimmed.includes("*/");
      yield line;
    } else if (/^(?:\/\/|#)/.test(trimmed)) {
      yield line;
    } else {
      break;
    }
  }
}

// Checks each path named by an explicit ADR/OCR comment against its live identity.
export function validateProvenance(root, path, errors) {
  const source = resolveRepositoryFile(root, path, path, "source path", errors);
  if (!source) return;
  const content = readFileSync(source, "utf8");
  for (const line of provenanceHeader(content)) {
    const marker = /^\s*(?:\/\/|#|\/\*|\*)\s*(?:ADR:|OCR:|Governed by)\s+(.+)$/.exec(line);
    if (!marker) continue;
    for (const record of recordPathsFromValue(marker[1])) {
      const detail = [];
      const resolved = resolveRepositoryFile(root, record, path, "provenance record", detail);
      if (!resolved || !validRecordLocation(record)) {
        errors.push(`${path}: PROVENANCE_RECORD_MISSING: ${record}${detail.length ? ` (${detail.join("; ")})` : " has an invalid record location"}`);
      }
    }
  }
}
