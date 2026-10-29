// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.

import { isReasonedNa } from "./reasoned-na.mjs";
import { validateSubtaskPlan, validateStableImplementationTouchpoints, validateAcceptanceChecks } from "./accepted-plans.mjs";
import { validateTraceability, validateAcceptedRiskMatrix } from "./accepted-risk.mjs";
import { validateOcrEligibility, validateOcrRunbookPlan } from "./accepted-ocr.mjs";
import { recordKind } from "./record-identity.mjs";
const FULL_ADR_SOURCE_ONLY_SECTIONS = new Set(["Contract-To-Check Traceability", "Risk Coverage Matrix"]);

// A standalone Pending placeholder anywhere in the body — not only as its
// first token — leaves a lifecycle-gated required section deliberately
// incomplete.
function containsStandalonePending(body) {
  // The placeholder is the capitalized token `Pending`, either as a whole
  // line or as the value of a Markdown field (`- **Owner**: Pending`); a
  // label prefix, bullet, quote, or emphasis never hides it. Lowercase
  // prose wrapping onto a line starting with "pending" is content.
  return body.split(/\n+/).some((line) => {
    const trimmed = line.trim();
    // Strip markdown scaffolding — bullets, quotes, emphasis, and a
    // field label ending in ":" — then require the placeholder at the
    // value position.
    const stripped = trimmed
      .replace(/^[-*_>]+\s*/, "")
      .replace(/^\*\*([^*]*)\*\*\s*:\s*/, "")
      .replace(/^[^:*]{0,80}:\s*/, "");
    if (/^\|.*\|$/.test(trimmed)) {
      const cells = trimmed
        .slice(1, -1)
        .split("|")
        .map((cell) => cell.trim());
      // Two-column field/value tables are narrative section content. Their
      // values cannot retain a lifecycle placeholder. Structured plan and
      // acceptance tables have their own status-aware validation below,
      // where Pending remains valid before a terminal implementation state.
      return cells.length === 2 && cells.some((cell) => /^Pending\b/.test(cell));
    }
    return /^Pending\b/.test(stripped);
  });
}

// The template's required top-level sections for one classified record kind.
function requiredSectionsForKind(context, kind) {
  const { ADD_REQUIRED_SECTIONS, FULL_ADR_REQUIRED_SECTIONS, LIGHTWEIGHT_ADR_REQUIRED_SECTIONS, OCR_REQUIRED_SECTIONS } = context;
  if (kind.isAdd) return ADD_REQUIRED_SECTIONS;
  if (kind.isOcr) return OCR_REQUIRED_SECTIONS;
  if (kind.isLightweight) return LIGHTWEIGHT_ADR_REQUIRED_SECTIONS;
  return FULL_ADR_REQUIRED_SECTIONS;
}

// Requires every top-level section mandated by the record template to carry
// substantive accepted-stage content.
function validateRequiredSectionBodies(context, path, markdown, kind, errors) {
  const { sectionContent, stripFencedCode } = context;
  const required = requiredSectionsForKind(context, kind);
  for (const section of required) {
    const raw = sectionContent(markdown, section);
    if (raw === undefined) continue;
    const body = stripFencedCode(raw).trim();
    const allowsReasonedNa = !kind.isAdd && !kind.isOcr && !kind.isLightweight
      && FULL_ADR_SOURCE_ONLY_SECTIONS.has(section)
      && isReasonedNa(body);
    if (
      !body
      || containsStandalonePending(body)
      || (/^N\/A\b/i.test(body) && !allowsReasonedNa)
    ) {
      errors.push(`${path}: ${section} body must contain substantive content, not Pending or N/A`);
    }
  }
  validateRetainedOptionalSections(context, path, markdown, errors);
}

// Retained optional sections must carry complete content at final
// lifecycle gates; removing the section entirely stays permitted.
function validateRetainedOptionalSections(context, path, markdown, errors) {
  const { sectionContent, stripFencedCode } = context;
  const seen = new Set();
  for (const match of markdown.matchAll(/^## ([^\n]+)$/gm)) {
    const heading = match[1].trim();
    if (!/\[Optional[^\]]*\]/.test(heading) || seen.has(heading)) continue;
    seen.add(heading);
    const raw = sectionContent(markdown, heading);
    if (raw === undefined) continue;
    const body = stripFencedCode(raw).trim();
    if (!body || containsStandalonePending(body) || (/^N\/A\b/.test(body) && !isReasonedNa(body))) {
      errors.push(
        `${path}: ${heading.split("[")[0].trim()} is a retained optional section and must contain complete content`,
      );
    }
  }
}

// Lifecycle-gated records retain every conditionally-required top-level
// section so each trigger has either substantive content or a reasoned N/A.
function validateConditionalSectionBodies(context, path, markdown, sections, errors) {
  const { sectionContent, stripFencedCode } = context;
  for (const section of sections) {
    const raw = sectionContent(markdown, section);
    if (raw === undefined) {
      errors.push(`${path}: missing conditionally required section ${section}`);
      continue;
    }
    const body = stripFencedCode(raw).trim();
    if (
      !body
      || containsStandalonePending(body)
      || (/^N\/A\b/i.test(body) && !isReasonedNa(body))
    ) {
      errors.push(
        `${path}: ${section} body must contain substantive content or N/A — <reason>`,
      );
    }
  }
}

// A Lightweight ADR always authorizes source behavior. A Full ADR is
// governance-only only when every source/configuration-only section records
// the required explicit non-applicability reason.
function sourceConfigurationApplies(context, markdown, isLightweight) {
  const { sectionContent, stripFencedCode, subsectionContent } = context;
  if (isLightweight) return true;
  const implementationPlan = sectionContent(markdown, "Implementation Plan") ?? "";
  const sourceOnlyBodies = [
    subsectionContent(implementationPlan, "Stable Implementation Touchpoints"),
    sectionContent(markdown, "Contract-To-Check Traceability"),
    sectionContent(markdown, "Risk Coverage Matrix"),
  ];
  return !sourceOnlyBodies.every((body) => isReasonedNa(stripFencedCode(body ?? "").trim()));
}

// Coordinates accepted-stage content validation without mixing the distinct
// ADD, ADR, and OCR table contracts in one oversized function.
export function validateRequiredBodyContent(context, path, markdown, errors) {
  const { ADD_CONDITIONAL_SECTIONS, OCR_CONDITIONAL_SECTIONS } = context;
  const kind = recordKind(path, markdown);
  validateRequiredSectionBodies(context, path, markdown, kind, errors);
  if (kind.isAdd) {
    validateConditionalSectionBodies(context, path, markdown, ADD_CONDITIONAL_SECTIONS, errors);
    return;
  }
  const subtaskIds = validateSubtaskPlan(context, path, markdown, kind.isOcr, errors);
  if (kind.isOcr) {
    validateConditionalSectionBodies(context, path, markdown, OCR_CONDITIONAL_SECTIONS, errors);
    validateOcrEligibility(context, path, markdown, errors);
    validateOcrRunbookPlan(context, path, markdown, errors);
    return;
  }
  const checkIds = validateAcceptanceChecks(context, path, markdown, subtaskIds, errors);
  if (!sourceConfigurationApplies(context, markdown, kind.isLightweight)) return;
  validateStableImplementationTouchpoints(context, path, markdown, errors);
  validateTraceability(context, path, markdown, checkIds, errors);
  validateAcceptedRiskMatrix(context, path, markdown, checkIds, errors);
}
