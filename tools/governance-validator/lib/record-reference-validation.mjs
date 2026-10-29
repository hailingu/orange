// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Direct repair authorization: docs/reviews/2026-10-16-peer-assessment-followup.md.
// Checks active concrete record references, retaining filesystem containment.
import { posix } from "node:path";
import { sectionContent, stripFencedCode } from "./markdown-parsing.mjs";
import { createMetadataValidator } from "./metadata-validation.mjs";
import { recordPathsFromValue, validRecordLocation } from "./record-identity.mjs";
import { resolveRepositoryFile } from "./repository-file.mjs";

const { metadata } = createMetadataValidator({ sectionContent, stripFencedCode });

// Active records carry untriggered archival instructions naming future paths.
// Retired records and ordinary documents retain reference checks in this section.
function activeReferenceContent(path, markdown) {
  const active = stripFencedCode(markdown);
  const status = metadata(active, "Decision Status") ?? metadata(active, "Design Status");
  if (!validRecordLocation(path) || !["Proposed", "Accepted", "Draft", "Current"].includes(status)) {
    return active;
  }
  return active.split(/(?=^## )/m)
    .filter((section) => !/^## Archival(?:\s|$)/.test(section)).join("");
}

// Distinguishes instantiated identities from filename grammar and templates.
function isConcreteRecord(path) {
  return /^(?:ADR|OCR|ADD)-\d+[^/]*\.md$/.test(path.split("/").at(-1));
}

// Collects repository identities and document-relative inline link targets;
// external URLs, comments, fenced examples and filename placeholders are inert.
function references(path, markdown) {
  const found = new Set();
  const active = activeReferenceContent(path, markdown).replace(
    /\[[^\]\n]*\]\(<?([^\s)>]+)>?(?:\s+"[^"\n]*")?\)/g,
    (link, destination) => {
      if (/^(?:[a-z][a-z\d+.-]*:|\/\/|#)/i.test(destination)) return "";
      const target = destination.split("#")[0];
      if (!isConcreteRecord(target)) return "";
      const rooted = !/^\.\.?\//.test(target) && recordPathsFromValue(target).length > 0;
      found.add(rooted ? target : posix.join(posix.dirname(path), target));
      return "";
    },
  );
  for (const target of recordPathsFromValue(active.replace(/(\.md)#[^\s`)\]>]+/g, "$1"))) {
    if (isConcreteRecord(target)) found.add(target);
  }
  return found;
}

// Requires every active concrete identity to resolve to a contained regular file.
export function validateRecordReferences(root, path, markdown, errors) {
  for (const target of references(path, markdown)) {
    const detail = [];
    if (!resolveRepositoryFile(root, target, path, "record reference", detail)) {
      errors.push(`${path}: RECORD_REFERENCE_INVALID: ${target} (${detail.join("; ")})`);
    }
  }
}
