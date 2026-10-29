// ADR: docs/adr/ADR-0002-governance-validator-adoption.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.

import { isReasonedNa } from "./reasoned-na.mjs";

// The former three-column traceability shape preserved for completed
// historical records.
function isLegacyTraceabilityTable(table) {
  return table.header.some((header) => /^Normative contract clause$/i.test(header))
    && table.header.some((header) => /^Acceptance check or deterministic test$/i.test(header));
}

// Resolves the matrix's status, check-reference, applicability, and header
// columns, accepting the legacy terminal matrix shape; returns undefined
// when neither the current nor the legacy header contract is satisfied.
function riskMatrixColumns(table, isTerminalRecord) {
  const currentHeaders = [
    /^Risk dimension$/i,
    /^Applicability and scenario, or specific N\/A reason$/i,
    /^Owning boundary$/i,
    /^Deterministic verification method$/i,
    /^Exact expected result$/i,
    /^Acceptance check IDs$/i,
    /^Status$/i,
    /^Actual evidence$/i,
  ];
  const hasCurrentHeaders = currentHeaders.every((pattern) =>
    table.header.some((header) => pattern.test(header)),
  );
  const legacyHeaders = [
    /^Risk dimension$/i,
    /^Applicability and scenario$/i,
    /^Owning boundary$/i,
    /^Deterministic verification$/i,
    /^Exact expected result$/i,
    /^Checks$/i,
    /^Status and stable evidence$/i,
  ];
  const usesLegacyTerminalMatrix = isTerminalRecord
    && legacyHeaders.every((pattern) => table.header.some((header) => pattern.test(header)));
  if (!hasCurrentHeaders && !usesLegacyTerminalMatrix) return undefined;
  return {
    status: table.header.findIndex((header) => (
      usesLegacyTerminalMatrix ? /^Status and stable evidence$/i.test(header) : /^Status$/i.test(header)
    )),
    checkRefs: table.header.findIndex((header) => (
      usesLegacyTerminalMatrix ? /^Checks$/i.test(header) : /^Acceptance check IDs$/i.test(header)
    )),
    applicability: table.header.findIndex((header) => /^Applicability and scenario/i.test(header)),
    headers: table.header,
    usesLegacyTerminalMatrix,
  };
}

// Enforces current traceability columns for active ADRs while preserving the
// former three-column shape of completed historical records.
export function validateTraceability(context, path, markdown, checkIds, errors) {
  const { metadata, sectionTable } = context;
  const table = sectionTable(markdown, "Contract-To-Check Traceability");
  const isTerminal = ["Complete", "Verified"].includes(metadata(markdown, "Implementation Status"));
  const isLegacy = isTerminal && table !== undefined && isLegacyTraceabilityTable(table);
  if (!table) {
    errors.push(`${path}: Contract-To-Check Traceability requires a structured table for an Accepted ADR`);
    return;
  }
  const columns = {
    clause: table.header.findIndex((header) => /^Clause ID$/i.test(header)),
    check: table.header.findIndex((header) => /Acceptance check/i.test(header)),
    columnCount: table.header.length,
  };
  const requiredHeaders = [
    /^Clause ID$/i, /^Authoritative contract path and heading$/i,
    /^Exact normative requirement$/i, /^Acceptance check or deterministic test IDs$/i,
    /^Explicit coverage method$/i,
  ];
  if (columns.clause === -1 || columns.check === -1
    || (!isLegacy && !requiredHeaders.every((pattern) => table.header.some((header) => pattern.test(header))))) {
    errors.push(`${path}: Contract-To-Check Traceability table is missing required template columns`);
    return;
  }
  const seen = new Set();
  for (const row of table.rows) {
    validateTraceabilityRow(context, path, row, columns, isLegacy, checkIds, seen, errors);
  }
  if (seen.size === 0) {
    errors.push(`${path}: Contract-To-Check Traceability requires at least one valid clause row for an Accepted ADR`);
  }
}

// Validates one traceability row and declares its clause ID; illegal or
// duplicate IDs are rejected rather than silently skipped.
function validateTraceabilityRow(context, path, row, columns, isLegacy, checkIds, seen, errors) {
  const clauseId = row[columns.clause] ?? "";
  if (!/^[A-Z]{2,}-\d+$/.test(clauseId)) {
    errors.push(`${path}: Contract-To-Check Traceability has a row with a missing or illegal Clause ID (${clauseId || "<missing>"})`);
    return;
  }
  if (seen.has(clauseId)) {
    errors.push(`${path}: Contract-To-Check Traceability duplicates ${clauseId}`);
    return;
  }
  seen.add(clauseId);
  const references = [...(row[columns.check] ?? "").matchAll(/\bAC-\d+\b/g)].map((match) => match[0]);
  if (references.length === 0) {
    errors.push(`${path}: clause ${clauseId} must reference at least one AC-N check`);
  } else {
    for (const checkId of references) {
      if (!checkIds.has(checkId)) errors.push(`${path}: clause ${clauseId} must reference declared acceptance check ${checkId}`);
    }
  }
  if (!isLegacy && hasIncompletePlannedColumn(context, row, columns.columnCount)) {
    errors.push(`${path}: clause ${clauseId} has an incomplete planned column`);
  }
}

// Returns whether any planned table cell is incomplete.
function hasIncompletePlannedColumn(context, row, columnCount) {
  const { isCompleteTableValue } = context;
  for (let index = 0; index < columnCount; index++) {
    if (!isCompleteTableValue((row[index] ?? "").trim())) return true;
  }
  return false;
}

// Counts only the structured matrix's dimensions, excluding later legend tables.
export function validateRiskMatrixDimensions(context, path, markdown, errors) {
  const { RISK_DIMENSIONS, normalizeDimension, sectionContent, sectionTable, stripFencedCode } = context;
  const content = stripFencedCode(sectionContent(markdown, "Risk Coverage Matrix") ?? "");
  if (!content) return;
  const table = sectionTable(markdown, "Risk Coverage Matrix");
  if (!table) {
    // A matrix with no table is only allowed as an explicit N/A with a reason;
    // otherwise the five-dimension coverage is mandatory.
    if (!isReasonedNa(content.trimStart())) {
      errors.push(`${path}: Risk Coverage Matrix must contain a five-dimension table or N/A — <reason>`);
    }
    return;
  }
  // The first non-separator row is the table header; the rest are dimension
  // rows. The matrix must contain each baseline dimension exactly once and no
  // extra dimension rows.
  const dataRows = table.rows.map((row) => {
    const raw = row[0] ?? "";
    return { raw, normalized: normalizeDimension(raw) };
  });
  const counts = new Map();
  for (const { normalized } of dataRows) {
    counts.set(normalized, (counts.get(normalized) ?? 0) + 1);
  }
  const baseline = new Set(RISK_DIMENSIONS.map(normalizeDimension));
  for (const dimension of RISK_DIMENSIONS) {
    const count = counts.get(normalizeDimension(dimension)) ?? 0;
    if (count === 0) {
      errors.push(`${path}: Risk Coverage Matrix is missing baseline dimension ${dimension}`);
    } else if (count > 1) {
      errors.push(`${path}: Risk Coverage Matrix duplicates baseline dimension ${dimension}`);
    }
  }
  for (const { raw, normalized } of dataRows) {
    if (!baseline.has(normalized)) {
      errors.push(`${path}: Risk Coverage Matrix has a non-baseline dimension row: ${raw}`);
    }
  }
}

// Enforces the approval-stage Risk Coverage Matrix contract: every baseline
// dimension must describe its scenario, owner, deterministic check, exact
// result, and declared acceptance-check linkage before an ADR is Accepted.
export function validateAcceptedRiskMatrix(context, path, markdown, declaredCheckIds, errors) {
  const { RISK_DIMENSIONS, metadata, normalizeDimension, sectionTable } = context;
  const table = sectionTable(markdown, "Risk Coverage Matrix");
  if (!table) {
    errors.push(`${path}: Risk Coverage Matrix requires a structured table for an Accepted ADR`);
    return;
  }
  const isTerminalRecord = ["Complete", "Verified"].includes(
    metadata(markdown, "Implementation Status"),
  );
  const columns = riskMatrixColumns(table, isTerminalRecord);
  if (!columns) {
    errors.push(`${path}: Risk Coverage Matrix table is missing required template columns`);
    return;
  }
  const baseline = new Set(RISK_DIMENSIONS.map(normalizeDimension));
  for (const row of table.rows) {
    const dimension = row[0] ?? "";
    if (baseline.has(normalizeDimension(dimension))) {
      validateRiskMatrixRow(context, path, row, dimension, columns, declaredCheckIds, errors);
    }
  }
}

// Validates one baseline-dimension row's status, planned columns, and
// declared acceptance-check references.
function validateRiskMatrixRow(context, path, row, dimension, columns, declaredCheckIds, errors) {
  const { CHECK_STATUSES } = context;
  const status = (row[columns.status] ?? "").trim();
  const normalizedStatus = columns.usesLegacyTerminalMatrix
    ? (status.match(/^(Pass|Fail|Not Started|In Progress|Blocked)(?:\s+[—-].*)?$/)?.[1]
      ?? status)
    : status;
  if (!CHECK_STATUSES.has(normalizedStatus) && !/^N\/A\s+—\s+\S/.test(normalizedStatus)) {
    errors.push(`${path}: Risk Coverage Matrix dimension ${dimension} has an illegal Status (${status || "<missing>"})`);
  }
  validateRiskMatrixPlannedColumns(context, path, row, dimension, columns, errors);
  const references = [...(row[columns.checkRefs] ?? "").matchAll(/\bAC-\d+\b/g)].map((match) => match[0]);
  if (references.length === 0) {
    errors.push(`${path}: Risk Coverage Matrix dimension ${dimension} must reference at least one AC-N check`);
    return;
  }
  for (const checkId of references) {
    if (!declaredCheckIds.has(checkId)) {
      errors.push(`${path}: Risk Coverage Matrix dimension ${dimension} must reference declared acceptance check ${checkId}`);
    }
  }
}

// Every planned column must be complete; the applicability column may carry
// a reasoned N/A, and status/evidence columns carry stage-dependent values.
function validateRiskMatrixPlannedColumns(context, path, row, dimension, columns, errors) {
  const { isCompleteTableValue } = context;
  for (let i = 0; i < columns.headers.length; i++) {
    const header = columns.headers[i];
    if (
      /^Status$/i.test(header)
      || /^Actual evidence$/i.test(header)
      || (columns.usesLegacyTerminalMatrix && /^Status and stable evidence$/i.test(header))
    ) continue;
    const value = (row[i] ?? "").trim();
    if (i === columns.applicability && isReasonedNa(value)) continue;
    if (!isCompleteTableValue(value)) {
      errors.push(`${path}: Risk Coverage Matrix dimension ${dimension} has an incomplete planned column`);
      break;
    }
  }
}
