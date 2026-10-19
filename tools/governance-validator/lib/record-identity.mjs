// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
// Peer-review repairs authorized by @hailingu on 2026-10-16.

// Reads a repository-relative Markdown path without losing a service prefix.
// Resolution and root/symlink containment remain the filesystem reader's job.
export function recordPathsFromValue(value) {
  if (typeof value !== "string") return [];
  return value.split(/[|`\s()[\]{}<> ,]+/).map((token) => token.replace(/\.md\.$/, ".md"))
    .filter((token) => /(?:^|\/)docs\/(?:adr|architecture)\/.+\.md$/.test(token)
      && !token.includes(":") && !token.startsWith("/"));
}

// Selects a single linked identity for fields that permit exactly one record.
export function recordPathFromValue(value) {
  return recordPathsFromValue(value)[0];
}

// Classifies by filename even when the directory is wrong, so diagnostics do
// not accidentally apply ADR lifecycle fields to an ADD.
export function recordKind(path, markdown = "") {
  const filename = path.split("/").at(-1);
  const isAdd = /^ADD-\d+.*\.md$/.test(filename);
  const isOcr = /^OCR-\d+.*\.md$/.test(filename);
  return { isAdd, isOcr, isLightweight: !isAdd && !isOcr && /^#\s+Lightweight ADR-\d+/m.test(markdown) };
}

// Enforces the prefix-plus-directory identity, including service and archive roots.
export function validRecordLocation(path) {
  const { isAdd, isOcr } = recordKind(path);
  const directory = isAdd ? "architecture" : isOcr ? "adr/ocr" : "adr";
  return new RegExp(`^(?:[^/]+/)*docs/${directory}/(?:archive/)?(?:ADR|OCR|ADD)-\\d+[^/]*\\.md$`).test(path);
}
