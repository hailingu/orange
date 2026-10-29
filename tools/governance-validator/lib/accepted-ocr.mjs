// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.

import { escapeRegExp } from "./escape-regexp.mjs";

// A runbook field's `**Field**:` value capture, from the field label through
// the next field label, stage heading, or section heading.
function runbookFieldPattern(escapedField) {
  return new RegExp(
    String.raw`\*\*${escapedField}\*\*:\s*([\s\S]*?)(?=\*\*[^*]+\*\*:\s*|^### |^## |$)`,
  );
}

// Requires the six template-defined OCR eligibility assertions to be checked
// and to retain their normative boundary language.
export function validateOcrEligibility(context, path, markdown, errors) {
  const { checklistItems, sectionContent, stripFencedCode } = context;
  const content = stripFencedCode(sectionContent(markdown, "Eligibility") ?? "");
  if (!content) return;
  const requirements = [
    {
      label: "boundary",
      matches: (text) => /\buses?\s+(?:(?:an?|the)\s+)?accepted\b/i.test(text)
        && ["architecture", "pipeline", "artifact", "contract", "security", "data"]
          .filter((term) => new RegExp(String.raw`\b${term}`, "i").test(text)).length >= 3,
    },
    { label: "reversible", matches: (text) => /\bis reversible\b/i.test(text) && /(recovery|rollback|discard|restore)/i.test(text) },
    {
      label: "Does not modify",
      matches: (text) => /Does not modify/i.test(text) && [
        "Dockerfile", "Makefile", "CI", "pipeline", "artifact format", "signing",
        "credential", "deployment", "API/schema/protocol", "authentication",
        "security policy", "data lifecycle", "dependenc", "provider", "irreversible",
      ].every((term) => text.toLowerCase().includes(term.toLowerCase())),
    },
    {
      label: "preflight",
      matches: (text) => /\bhas a defined preflight\b/i.test(text) && /\bsuccess check\b/i.test(text)
        && /\bstop condition\b/i.test(text) && /(recovery|rollback)/i.test(text),
    },
    {
      label: "no secret",
      matches: (text) => /\bcontains no secret\b/i.test(text) && /\bcredential\b/i.test(text)
        && /\bprivate endpoint\b/i.test(text) && /\bsensitive user data\b/i.test(text),
    },
    {
      label: "automatic",
      matches: (text) => (/\b(?:configured )?automatic(?:-review| review)(?: mechanism)? covers\b/i.test(text)
        || /N\/A\s+—\s+no automatic-review mechanism/i.test(text))
        && (/(exact (?:declared )?input (?:revision|commit))/i.test(text)
          || /N\/A\s+—\s+no automatic-review mechanism/i.test(text)),
    },
  ];
  const items = checklistItems(content);
  for (const requirement of requirements) {
    const matches = items.filter((item) => requirement.matches(item.text));
    if (matches.length === 0 || matches.some((item) => !item.checked)) {
      errors.push(`${path}: Eligibility item "${requirement.label}" must be present and confirmed for an Accepted OCR`);
    }
  }
}

// Requires every OCR runbook stage to retain all planned action, success, and
// recovery fields before operational acceptance.
export function validateOcrRunbookPlan(context, path, markdown, errors) {
  const { sectionContent, stripFencedCode, subsectionContent, isCompleteValue } = context;
  const runbook = stripFencedCode(sectionContent(markdown, "Core Runbook And Evidence") ?? "");
  if (!runbook) return;
  const stageFields = {
    Preflight: ["Planned action and criterion"],
    Execute: ["Planned action"],
    Verify: ["Success criterion"],
    "Stop and Recovery": ["Stop condition", "Recovery action", "Recovery verification"],
  };
  for (const [stage, fields] of Object.entries(stageFields)) {
    const stageBody = subsectionContent(runbook, stage);
    if (!stageBody) {
      errors.push(`${path}: Core Runbook is missing required stage ${stage} for an Accepted OCR`);
      continue;
    }
    for (const field of fields) {
      const escaped = escapeRegExp(field);
      const match = runbookFieldPattern(escaped).exec(stageBody);
      if (!match || !isCompleteValue(match[1].trim())) {
        errors.push(`${path}: Core Runbook stage ${stage} field ${field} must be present and complete`);
      }
    }
    const actualResult = stageBody.match(/\*\*Actual result and stable evidence\*\*:\s*([^\n]+)/)?.[1].trim();
    if (!actualResult) {
      errors.push(`${path}: Core Runbook stage ${stage} field Actual result and stable evidence must be present`);
    }
  }
}
