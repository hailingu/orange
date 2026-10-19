// ADR: docs/adr/ADR-0002-governance-validator-adoption.md, docs/adr/ADR-0006-complete-record-index-validation.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
import { recordKind, recordPathFromValue, recordPathsFromValue } from "./record-identity.mjs";

// Returns the non-empty title from a recognized record H1 without matching a
// greedy pattern across the full Markdown document.
function recordTitleFromHeading(markdown) {
  for (const line of String(markdown).split("\n")) {
    const title = recordTitleFromLine(line);
    if (title !== undefined) return title;
  }
  return "";
}

// Parses one heading line of the form `# [Lightweight ]ADR-NNNN: Title` and
// returns its title, or undefined when the line is not a recognized record H1.
function recordTitleFromLine(line) {
  if (!line.startsWith("#")) return undefined;
  let index = 1;
  while (line[index] === " " || line[index] === "\t") index += 1;
  if (index === 1) return undefined;

  if (line.startsWith("Lightweight ", index)) index += "Lightweight ".length;
  const recordPrefix = ["ADR-", "OCR-", "ADD-"].find((prefix) => line.startsWith(prefix, index));
  if (!recordPrefix) return undefined;
  index += recordPrefix.length;

  const idStart = index;
  while (line[index] >= "0" && line[index] <= "9") index += 1;
  if (index === idStart || line[index] !== ":") return undefined;

  const title = line.slice(index + 1).trim();
  return title || undefined;
}

// Returns the complete token for one ADR or ADD path when it has the expected
// record directory and filename prefix; callers retain ownership of resolution.
function findRecordPath(value, directory, filenamePrefix) {
  for (const candidate of recordPathsFromValue(value)) {
    const filename = candidate.slice(candidate.lastIndexOf("/") + 1);
    if (
      candidate.includes(directory)
      && filename.startsWith(filenamePrefix)
      && /\d/.test(filename[filenamePrefix.length] ?? "")
      && filename.endsWith(".md")
    ) {
      return candidate;
    }
  }
  return undefined;
}

// The index columns each INDEX.md template must declare.
function expectedIndexColumns(path) {
  return path.endsWith("docs/architecture/INDEX.md")
    ? ["ID", "Title", "Design Status", "Scope Level", "Scope", "Path", "Demand Source", "Superseded By"]
    : ["Type", "ID", "Title", "Decision Status", "Implementation Status", "Scope", "Architecture Source", "Path", "Superseded By"];
}

// Resolves the row's record path from the Path column when present, falling
// back to the last recognized record token in the row.
function indexedRecordPath(cells, columns, rowText) {
  if (columns.has("Path")) {
    const value = cells[columns.get("Path")];
    return recordPathFromValue(value) ?? value;
  }
  return recordPathsFromValue(rowText)
    .findLast((token) =>
      findRecordPath(token, "docs/adr/", "ADR-")
      || findRecordPath(token, "docs/architecture/", "ADD-"),
    );
}

// Title comparison
function validateIndexedTitle(path, cells, columns, indexed, record, errors) {
  if (!columns.has("Title")) return;
  const indexedTitle = cells[columns.get("Title")];
  const recordTitle = recordTitleFromHeading(record);
  if (recordTitle && indexedTitle !== recordTitle) {
    errors.push(`${path}: index Title disagrees with record for ${indexed}: ${indexedTitle} vs ${recordTitle}`);
  }
}

// Derives an index row's record type from its filename and title heading.
function indexRecordType(indexed, record) {
  const { isOcr, isLightweight } = recordKind(indexed, record);
  if (isOcr) return "OCR";
  if (isLightweight) return "Lightweight ADR";
  return "Full ADR";
}

// Type comparison (ADR/OCR index)
function validateIndexedType(path, cells, columns, indexed, record, errors) {
  if (!columns.has("Type")) return;
  const indexedType = cells[columns.get("Type")];
  const recordType = indexRecordType(indexed, record);
  if (indexedType !== recordType) {
    errors.push(`${path}: index Type ${indexedType} disagrees with record ${recordType} for ${indexed}`);
  }
}

// ID comparison — derive the record ID from its filename.
function validateIndexedId(path, cells, columns, indexed, errors) {
  if (!columns.has("ID")) return;
  const indexedId = cells[columns.get("ID")];
  const recordId = /^(?:ADR|ADD|OCR)-\d+/.exec(indexed.split("/").at(-1))?.[0] ?? "";
  if (recordId && indexedId !== recordId) {
    errors.push(`${path}: index ID ${indexedId} disagrees with record ${recordId} for ${indexed}`);
  }
}

// For Demand Source, normalize Markdown links to their URL for comparison.
function compareIndexedDemandSource(path, column, indexed, indexedValue, recordValue, errors) {
  const indexedUrl = /https?:\/\/[^\s)]+/.exec(indexedValue)?.[0] ?? indexedValue;
  const recordUrl = /https?:\/\/[^\s)]+/.exec(recordValue)?.[0] ?? recordValue;
  if (indexedUrl !== recordUrl) {
    errors.push(`${path}: index ${column} disagrees with record for ${indexed}`);
  }
}

// Parses candidate identities, links and deferral reasons for lifecycle checks.
export function candidateTable(context, markdown) {
  const { sectionContent, tableFromContent } = context;
  const parsed = tableFromContent(sectionContent(markdown, "ADR Task Candidates") ?? "");
  if (!parsed) return undefined;
  const { header, rows } = parsed;
  const idCol = header.indexOf("ID");
  const statusCol = header.indexOf("Status");
  const pathCol = header.indexOf("ADR path");
  const reasonCol = header.findIndex((cell) => /reason|evidence/i.test(cell));
  if (idCol === -1 || statusCol === -1 || pathCol === -1) return undefined;
  const table = new Map();
  const seen = new Set();
  const duplicateIds = [];
  const malformedIds = [];
  for (const cells of rows) {
    const id = cells[idCol];
    if (!id || !/^CAND-\d+$/.test(id)) {
      malformedIds.push(id || "<missing>");
      continue;
    }
    if (seen.has(id)) {
      duplicateIds.push(id);
    } else {
      seen.add(id);
    }
    table.set(id, { status: cells[statusCol] ?? "", path: cells[pathCol] ?? "", reason: cells[reasonCol] ?? "" });
  }
  return { table, duplicateIds, malformedIds };
}

// Checks every structured index table using its own columns and shared identities.
export function validateIndex(context, root, path, markdown, errors) {
  const { tablesFromContent } = context;
  const seen = new Set();
  const tables = tablesFromContent(markdown);
  if (tables.length === 0) {
    errors.push(`${path}: INDEX_TABLE_REQUIRED: index requires a structured Markdown table`);
    return seen;
  }
  for (const parsed of tables) {
    const columns = new Map(parsed.header.map((cell, index) => [cell, index]));
    for (const col of expectedIndexColumns(path)) {
      if (!columns.has(col)) {
        errors.push(`${path}: INDEX_REQUIRED_COLUMN: index is missing required column ${col}`);
      }
    }
    for (const cells of parsed.rows) {
      validateIndexRow(context, root, path, cells, columns, seen, errors);
    }
  }
  return seen;
}

// Validates one index row: resolves its indexed record and compares every
// authoritative field against the record's own content.
function validateIndexRow(context, root, path, cells, columns, seen, errors) {
  const { readFileSync, resolveRepositoryFile } = context;
  const indexed = indexedRecordPath(cells, columns, `| ${cells.join(" | ")} |`);
  if (!indexed) {
    errors.push(`${path}: index Path is missing`);
    return;
  }
  if (seen.has(indexed)) errors.push(`${path}: INDEX_DUPLICATE_PATH: duplicate index path ${indexed}`);
  seen.add(indexed);
  const absolute = resolveRepositoryFile(root, indexed, path, "index path", errors);
  if (!absolute) return;
  const record = readFileSync(absolute, "utf8");
  validateIndexedStatus(context, path, cells, columns, indexed, record, errors);
  validateIndexedTitle(path, cells, columns, indexed, record, errors);
  validateIndexedType(path, cells, columns, indexed, record, errors);
  validateIndexedId(path, cells, columns, indexed, errors);
  validateIndexedFields(context, path, cells, columns, indexed, record, errors);
}

// Status columns must agree with the indexed record's active metadata.
function validateIndexedStatus(context, path, cells, columns, indexed, record, errors) {
  const { metadata } = context;
  for (const [column, field] of [
    ["Decision Status", "Decision Status"],
    ["Implementation Status", "Implementation Status"],
    ["Design Status", "Design Status"],
  ]) {
    if (!columns.has(column)) continue;
    const indexedStatus = cells[columns.get(column)];
    const recordStatus = metadata(record, field);
    if (recordStatus && indexedStatus !== recordStatus) {
      errors.push(`${path}: index ${column} ${indexedStatus} disagrees with record ${recordStatus} for ${indexed}`);
    }
  }
}

// Additional authoritative field comparisons — do not skip missing record
// metadata; a missing field is itself a disagreement (AGENTS.md).
function validateIndexedFields(context, path, cells, columns, indexed, record, errors) {
  const { metadata } = context;
  const recordIsAdd = indexed.includes("/architecture/");
  const fieldMap = recordIsAdd
    ? [["Scope Level", "Scope Level"], ["Scope", "Scope"], ["Superseded By", "Superseded By"], ["Demand Source", "Demand Sources"]]
    : [["Scope", "Record Scope"], ["Architecture Source", "Architecture Source"], ["Superseded By", "Superseded By"]];
  for (const [column, field] of fieldMap) {
    if (!columns.has(column)) continue; // missing columns caught at header
    const indexedValue = (cells[columns.get(column)] ?? "").replaceAll("`", "");
    const recordValue = (metadata(record, field) ?? "").replaceAll("`", "");
    if (column === "Demand Source") {
      compareIndexedDemandSource(path, column, indexed, indexedValue, recordValue, errors);
    } else if (indexedValue !== recordValue) {
      errors.push(`${path}: index ${column} disagrees with record for ${indexed}`);
    }
  }
}

// Checks each ADD candidate against its linked ADR and reciprocal source.
export function validateReciprocalLinks(context, root, path, markdown, errors) {
  const { sectionContent } = context;
  if (!path.includes("/architecture/") || !path.split("/").at(-1).startsWith("ADD-")) return;
  // A missing section is already flagged by required-sections validation; a
  // section that exists but cannot be parsed (missing/misspelled ID, Status, or
  // ADR path header) must not silently bypass the reciprocal-link check.
  if (!sectionContent(markdown, "ADR Task Candidates")) return;
  const parsed = candidateTable(context, markdown);
  if (!parsed) {
    errors.push(
      `${path}: ADR Task Candidates table is missing required ID, Status, or ADR path columns, or is not a structured Markdown table`,
    );
    return;
  }
  for (const id of parsed.malformedIds) {
    errors.push(`${path}: ADR Task Candidates table has a row with a missing or illegal ID (${id})`);
  }
  for (const id of parsed.duplicateIds) {
    errors.push(`${path}: ADR Task Candidates table has a duplicate candidate ID ${id}`);
  }
  for (const [candidate, entry] of parsed.table) {
    validateCandidateLink(context, root, path, candidate, entry, errors);
  }
}

// Validates one candidate row's linked ADR when its status requires a link.
function validateCandidateLink(context, root, path, candidate, { status, path: linked, reason }, errors) {
  const { CANDIDATE_STATUSES, readFileSync, resolveRepositoryFile } = context;
  if (!CANDIDATE_STATUSES.has(status)) {
    errors.push(`${path}: ${candidate} has illegal status ${status}`);
    return;
  }
  // Older tables record the reason in their unused ADR-path cell.
  const deferredReason = reason || /^(?:None|N\/A)\s+—\s+(.+)$/.exec(linked)?.[1] || "";
  if (status === "Deferred" && (!deferredReason.trim() || /^(?:Pending|None|N\/A)(?:\b|$)/i.test(deferredReason))) {
    errors.push(`${path}: CANDIDATE_DEFERRED_REASON_REQUIRED: ${candidate} requires a recorded reason`);
  }
  if (status !== "Selected" && status !== "Complete") return;
  const linkedPath = findRecordPath(linked, "docs/adr/", "ADR-");
  if (!linkedPath) {
    errors.push(`${path}: Selected or Complete ${candidate} is missing its linked ADR path`);
    return;
  }
  const absolute = resolveRepositoryFile(root, linkedPath, path, "linked ADR path", errors);
  if (!absolute) return;
  const adr = readFileSync(absolute, "utf8");
  validateReciprocalSource(context, path, candidate, linkedPath, adr, errors);
  validateLinkedAdrStatus(context, path, candidate, status, linkedPath, adr, errors);
}

// The linked ADR's Architecture Source must reciprocate this exact ADD path
// and candidate.
function validateReciprocalSource(context, path, candidate, linkedPath, adr, errors) {
  const { metadata } = context;
  const source = metadata(adr, "Architecture Source") ?? "";
  const sourceAdd = findRecordPath(source, "docs/architecture/", "ADD-");
  const sourceCandidate = /\bCAND-\d+\b/.exec(source)?.[0];
  if (sourceAdd !== path || sourceCandidate !== candidate) {
    errors.push(`${path}: reciprocal Architecture Source is missing for ${candidate} -> ${linkedPath}`);
  }
}

// A Selected candidate's ADR must remain in an allowed non-terminal state;
// a Complete candidate's ADR must be Complete or Verified.
function validateLinkedAdrStatus(context, path, candidate, status, linkedPath, adr, errors) {
  const { metadata } = context;
  const decision = metadata(adr, "Decision Status");
  const implementation = metadata(adr, "Implementation Status");
  if (status === "Selected") {
    if (
      !["Proposed", "Accepted"].includes(decision)
      || !["Not Started", "In Progress", "Blocked"].includes(implementation)
    ) {
      errors.push(
        `${path}: ${candidate} is Selected but linked ADR ${linkedPath} is ${decision ?? "<missing>"}/${implementation ?? "<missing>"}`,
      );
    }
    return;
  }
  if (!["Complete", "Verified"].includes(implementation)) {
    errors.push(
      `${path}: ${candidate} is Complete but linked ADR ${linkedPath} is ${implementation ?? "<missing>"}`,
    );
  }
}

// Requires a reciprocal candidate selection for each product-derived ADR.
export function validateArchitectureSource(context, root, path, markdown, errors) {
  const { metadata, readFileSync, resolveRepositoryFile } = context;
  const source = metadata(markdown, "Architecture Source");
  if (source === undefined) {
    errors.push(`${path}: Architecture Source is missing`);
    return;
  }
  // Governance, process, and other non-product-demand ADRs may record
  // `N/A — <reason>`. Any other value must be an exact ADD path plus candidate.
  if (/^N\/A\s+—\s+\S/.test(source)) return;
  const addPath = findRecordPath(source, "docs/architecture/", "ADD-");
  const candidate = /\bCAND-\d+\b/.exec(source)?.[0];
  if (!addPath || !candidate) {
    errors.push(
      `${path}: Architecture Source must be an ADD path plus candidate ID or N/A — <reason>`,
    );
    return;
  }
  const absolute = resolveRepositoryFile(
    root,
    addPath,
    path,
    "Architecture Source ADD path",
    errors,
  );
  if (!absolute) return;
  const entry = candidateTable(context, readFileSync(absolute, "utf8"))?.table?.get(candidate);
  if (
    !entry
    || (entry.status !== "Selected" && entry.status !== "Complete")
    || findRecordPath(entry.path, "docs/adr/", "ADR-") !== path
  ) {
    errors.push(`${path}: reciprocal ADD candidate link is missing for ${addPath} — ${candidate}`);
  }
}
