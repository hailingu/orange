// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.

import { isReasonedNa } from "./reasoned-na.mjs";

// The template columns an Accepted subtask plan table must declare.
function subtaskRequiredHeaders(isOcr) {
  return isOcr
    ? [/^ID$/, /^Objective/i, /^Included scope/i, /^Completion criterion/i, /^Expected evidence/i, /^Status$/, /^Actual.*evidence/i]
    : [/^ID$/, /^Objective/i, /^Included scope/i, /^Status$/, /^Actual.*evidence/i];
}

// The template columns an Accepted acceptance-checks table must declare.
function acceptanceCheckRequiredHeaders() {
  return [
    /^Check ID$/i, /^Subtask$/i, /^Binary acceptance point$/i,
    /^Preconditions? or input$/i, /^Verification method$/i,
    /^Exact expected result$/i, /^Expected evidence$/i, /^Status$/i,
    /^Actual result and evidence$/i,
  ];
}

// Validates the one-to-three accepted subtasks and returns their declared IDs
// for acceptance-check reference validation.
export function validateSubtaskPlan(context, path, markdown, isOcr, errors) {
  const { sectionTable } = context;
  const section = isOcr ? "Task Definition" : "Implementation Plan";
  const table = sectionTable(markdown, section);
  const declared = new Set();
  if (!table) {
    errors.push(`${path}: ${section} requires a structured table for an Accepted record`);
    return declared;
  }
  const columns = {
    id: table.header.indexOf("ID"),
    status: table.header.indexOf("Status"),
    evidence: new Set(table.header.flatMap((header, index) =>
      /^Actual.*evidence/i.test(header) ? [index] : [],
    )),
  };
  const requiredHeaders = subtaskRequiredHeaders(isOcr);
  if (columns.id === -1 || columns.status === -1
    || !requiredHeaders.every((pattern) => table.header.some((header) => pattern.test(header)))) {
    errors.push(`${path}: ${section} table is missing required template columns for an Accepted record`);
    return declared;
  }
  for (const row of table.rows) {
    validateSubtaskRow(context, path, section, row, columns, declared, errors);
  }
  if (declared.size === 0) {
    errors.push(`${path}: ${section} requires at least one T-N subtask row for an Accepted record`);
  } else if (declared.size > 3) {
    errors.push(`${path}: ${section} must have at most 3 subtasks for an Accepted record (has ${declared.size})`);
  }
  return declared;
}

// Validates one subtask row and declares its ID; illegal or duplicate IDs
// are rejected rather than silently skipped.
function validateSubtaskRow(context, path, section, row, columns, declared, errors) {
  const { SUBTASK_STATUSES, isCompleteTableValue } = context;
  const id = row[columns.id] ?? "";
  if (!/^T-\d+$/.test(id)) {
    errors.push(`${path}: ${section} has a row with a missing or illegal ID (${id || "<missing>"})`);
    return;
  }
  if (declared.has(id)) {
    errors.push(`${path}: ${section} duplicates subtask ID ${id}`);
    return;
  }
  declared.add(id);
  const status = (row[columns.status] ?? "").trim();
  if (!SUBTASK_STATUSES.has(status) && !/^N\/A\s+—\s+\S/.test(status)) {
    errors.push(`${path}: subtask ${id} has an illegal Status (${status})`);
  }
  for (let index = 0; index < row.length; index++) {
    if (index === columns.id || index === columns.status || columns.evidence.has(index)) continue;
    if (!isCompleteTableValue((row[index] ?? "").trim())) {
      errors.push(`${path}: ${section} has an incomplete cell in a planned column for ${id}`);
      break;
    }
  }
}

// Requires source ADRs to identify stable implementation touchpoints; no
// record path is exempt from this gate.
export function validateStableImplementationTouchpoints(context, path, markdown, errors) {
  const { isCompleteTableValue, sectionContent, subsectionContent, subsectionTable } = context;
  const subsection = subsectionContent(sectionContent(markdown, "Implementation Plan") ?? "", "Stable Implementation Touchpoints");
  if (!subsection) {
    errors.push(`${path}: Stable Implementation Touchpoints requires a structured table for an active Accepted ADR`);
    return;
  }
  const table = subsectionTable(markdown, "Implementation Plan", "Stable Implementation Touchpoints");
  const headers = [
    /^Path$/i,
    /^Stable symbol or contract anchor$/i,
    /^Key code excerpt, when needed$/i,
    /^Purpose$/i,
    /^Source revision$/i,
  ];
  if (!table) {
    errors.push(`${path}: Stable Implementation Touchpoints requires a structured table for an Accepted ADR`);
    return;
  }
  if (!headers.every((pattern) => table.header.some((header) => pattern.test(header)))) {
    errors.push(`${path}: Stable Implementation Touchpoints table is missing required template columns`);
    return;
  }
  if (table.rows.length === 0) {
    errors.push(`${path}: Stable Implementation Touchpoints requires at least one row for an Accepted ADR`);
    return;
  }
  const columns = headers.map((pattern) => table.header.findIndex((header) => pattern.test(header)));
  for (const row of table.rows) {
    const [pathValue, anchor, excerpt, purpose, revision] = columns.map((column) => (row[column] ?? "").trim());
    const hasAnchor = isCompleteTableValue(anchor);
    const hasExcerpt = isCompleteTableValue(excerpt);
    const requiredValuesComplete = [pathValue, purpose, revision]
      .every((value) => isCompleteTableValue(value));
    const anchorsValid = (hasAnchor || isReasonedNa(anchor))
      && (hasExcerpt || isReasonedNa(excerpt))
      && (hasAnchor || hasExcerpt);
    if (!requiredValuesComplete || !anchorsValid) {
      errors.push(`${path}: Stable Implementation Touchpoints has an incomplete required cell`);
      return;
    }
  }
}

// Validates accepted ADR checks and returns their IDs for traceability and
// risk-matrix reference validation.
export function validateAcceptanceChecks(context, path, markdown, subtaskIds, errors) {
  const { sectionTable } = context;
  const table = sectionTable(markdown, "Acceptance Checks");
  const declared = new Set();
  if (!table) {
    errors.push(`${path}: Acceptance Checks requires a structured table for an Accepted ADR`);
    return declared;
  }
  const columns = {
    id: table.header.findIndex((header) => /^Check ID$/i.test(header)),
    subtask: table.header.findIndex((header) => /^Subtask$/i.test(header)),
    status: table.header.findIndex((header) => /^Status$/i.test(header)),
    headers: table.header,
  };
  const requiredHeaders = acceptanceCheckRequiredHeaders();
  if (columns.id === -1 || columns.subtask === -1
    || !requiredHeaders.every((pattern) => table.header.some((header) => pattern.test(header)))) {
    errors.push(`${path}: Acceptance Checks table is missing required template columns`);
    return declared;
  }
  for (const row of table.rows) {
    validateAcceptanceCheckRow(context, path, row, columns, subtaskIds, declared, errors);
  }
  if (declared.size === 0) {
    errors.push(`${path}: Acceptance Checks requires at least one AC-N row for an Accepted ADR`);
  }
  return declared;
}

// Validates one acceptance-check row and declares its ID; illegal or
// duplicate IDs are rejected rather than silently skipped.
function validateAcceptanceCheckRow(context, path, row, columns, subtaskIds, declared, errors) {
  const { CHECK_STATUSES, isCompleteTableValue } = context;
  const id = row[columns.id] ?? "";
  if (!/^AC-\d+$/.test(id)) {
    errors.push(`${path}: Acceptance Checks has a row with a missing or illegal Check ID (${id || "<missing>"})`);
    return;
  }
  if (declared.has(id)) {
    errors.push(`${path}: Acceptance Checks duplicates ${id}`);
    return;
  }
  declared.add(id);
  if (!subtaskIds.has((row[columns.subtask] ?? "").trim())) {
    errors.push(`${path}: acceptance check ${id} Subtask must reference a declared T-N ID`);
  }
  const status = (row[columns.status] ?? "").trim();
  if (!CHECK_STATUSES.has(status) && !/^N\/A\s+—\s+\S/.test(status)) {
    errors.push(`${path}: acceptance check ${id} has an illegal Status (${status || "<missing>"})`);
  }
  for (let index = 0; index < columns.headers.length; index++) {
    const header = columns.headers[index];
    if (index === columns.id || index === columns.subtask || /^Status$/i.test(header)
      || /^Actual result and evidence$/i.test(header)) continue;
    if (!isCompleteTableValue((row[index] ?? "").trim())) {
      errors.push(`${path}: acceptance check ${id} has an incomplete planned column`);
      break;
    }
  }
}
