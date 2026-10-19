// ADR: docs/adr/ADR-0006-complete-record-index-validation.md, docs/adr/ADR-0007-governance-fixture-lifecycle.md
// Exercises IX-1 through IX-4 with complete records and the real CLI. Skipping
// later tables, resetting duplicate tracking or reusing a header must fail.

import assert from "node:assert/strict";
import { readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import test from "node:test";
import * as markdown from "../lib/markdown-parsing.mjs";
import { completeOcr, run, validRepository, write } from "./fixtures.mjs";

const adrIndex = "docs/adr/INDEX.md";
const addIndex = "docs/architecture/INDEX.md";
const adrHeader = ["Type", "ID", "Title", "Decision Status", "Implementation Status", "Scope", "Architecture Source", "Path", "Superseded By"];
const addHeader = ["ID", "Title", "Design Status", "Scope Level", "Scope", "Path", "Demand Source", "Superseded By"];
const projectAdr = "docs/adr/ADR-0001-example.md";
const archivedAdr = "docs/adr/archive/ADR-0002-example.md";
const serviceAdr = "demo-service/docs/adr/ADR-0001-example.md";
const archivedOcr = "docs/adr/ocr/archive/OCR-0002-example.md";
const projectAdd = "docs/architecture/ADD-0001-example.md";
const archivedAdd = "docs/architecture/archive/ADD-0002-example.md";
const anotherAdd = "docs/architecture/ADD-0003-example.md";
const adrRows = [
  ["Full ADR", "ADR-0001", "Example", "Proposed", "Not Started", "Project", "N/A — governance-only example", projectAdr, "None"],
  ["Full ADR", "ADR-0001", "Example", "Proposed", "Not Started", "Service internal — demo-service", "N/A — governance-only example", serviceAdr, "None"],
  ["Full ADR", "ADR-0002", "Example", "Rejected", "Not Applicable", "Project", "N/A — governance-only example", archivedAdr, "None"],
  ["OCR", "OCR-0002", "Example", "Accepted", "Complete", "Project", "N/A — example", archivedOcr, "None"],
];
const addRows = [
  ["ADD-0001", "Example", "Draft", "Repository / Cross-project", "Project", projectAdd, "https://example.test/card", "None"],
  ["ADD-0002", "Example", "Deprecated", "Repository / Cross-project", "Project", archivedAdd, "https://example.test/card", "None"],
  ["ADD-0003", "Example", "Draft", "Repository / Cross-project", "Project", anotherAdd, "https://example.test/card", "None"],
];
const indexes = [
  { path: adrIndex, header: adrHeader, rows: adrRows },
  { path: addIndex, header: addHeader, rows: addRows },
];

// Renders controlled fixture data; expected cells do not use the production parser.
function table(header, rows) {
  return [header, header.map(() => "---"), ...rows]
    .map((cells) => `| ${cells.join(" | ")} |`).join("\n");
}

// Changes only active metadata in a controlled fixture record.
function setMetadata(root, path, fields) {
  let content = readFileSync(join(root, path), "utf8");
  for (const [field, value] of Object.entries(fields)) {
    const entry = `- **${field}**: ${value}`;
    const pattern = new RegExp(`^- \\*\\*${field}\\*\\*:[^\\n]*$`, "m");
    content = pattern.test(content)
      ? content.replace(pattern, () => entry)
      : content.replace("## Metadata [Required]\n", `## Metadata [Required]\n${entry}\n`);
  }
  write(root, path, content);
}

// Creates archived and same-number service ADRs from existing complete fixtures.
function decisionRecords(root) {
  const base = readFileSync(join(root, projectAdr), "utf8");
  write(root, serviceAdr, base);
  setMetadata(root, serviceAdr, { "Record Scope": "Service internal — demo-service" });
  write(root, archivedAdr, base.replace("ADR-0001:", "ADR-0002:"));
  setMetadata(root, archivedAdr, {
    "Decision Status": "Rejected", "Implementation Status": "Not Applicable",
    Rejector: "@linhai", "Rejection Time": "2026-08-13T00:00:00Z",
    "Rejection Evidence": "Reject", "Rejection Reason": "The fixture proposal exceeded its approved scope.",
  });
  completeOcr(root, "0002");
  const original = "docs/adr/ocr/OCR-0002-example.md";
  const completed = readFileSync(join(root, original), "utf8")
    .replace("### Preflight [Required]\n", "### Preflight [Required]\n**Planned action and criterion**: Confirm the fixture input.\n")
    .replace("### Execute [Required]\n", "### Execute [Required]\n**Planned action**: Execute the fixture operation.\n")
    .replace("### Verify [Required]\n", "### Verify [Required]\n**Success criterion**: Fixture operation completes.\n")
    .replace("### Stop and Recovery [Required]\n", "### Stop and Recovery [Required]\n**Stop condition**: Invalid fixture input.\n**Recovery action**: Discard the fixture result.\n**Recovery verification**: No fixture output remains.\n");
  write(root, archivedOcr, completed);
  rmSync(join(root, original));
}

// Adds a retired ADD with no selected work and a third independently indexed ADD.
function architectureRecords(root) {
  const base = readFileSync(join(root, projectAdd), "utf8");
  write(root, anotherAdd, base.replace("ADD-0001:", "ADD-0003:"));
  write(root, archivedAdd, base.replace("ADD-0001:", "ADD-0002:")
    .replace("| CAND-1 | Ready | None |", "| CAND-1 | Deferred | None — design retired |"));
  setMetadata(root, archivedAdd, {
    "Design Status": "Deprecated", "Retired By": "@linhai",
    "Retirement Time": "2026-08-13T00:00:00Z", "Retirement Evidence": "Deprecate",
    "Retirement Reason": "The fixture design was retired without selected implementation work.",
  });
}

// Owns all fixture state, including cleanup after assertion failures.
function repository(t) {
  const root = validRepository(t);
  decisionRecords(root);
  architectureRecords(root);
  for (const { path, header, rows } of indexes) write(root, path, table(header, rows));
  return root;
}

// Requires a record-scoped index diagnostic, rather than an unrelated lifecycle error.
function assertIndexFailure(root, index, { record, code } = {}) {
  const result = run(root);
  assert.equal(result.status, 1, result.stderr || result.stdout);
  assert.ok(result.stderr.split("\n").some((line) =>
    line.startsWith(`${index}:`) && (!record || line.includes(record)) && (!code || line.includes(code))),
  result.stderr);
}

// IX-1/IX-2: table distribution and column order cannot change valid membership.
const layouts = [
  ["one table", (rows) => [rows]],
  ["two tables", (rows) => [rows.slice(0, 1), rows.slice(1)]],
  ["reversed tables", (rows) => [rows.slice(1), rows.slice(0, 1)]],
  ["three tables", (rows) => [rows.slice(0, 1), rows.slice(1, 2), rows.slice(2)]],
];
for (const [name, distribute] of layouts) {
  test(`IX-1 accepts ${name} with archived records and same-number ADRs`, (t) => {
    const root = repository(t);
    for (const { path, header, rows } of indexes) {
      const parts = distribute(rows).map((group, index) => index === 0
        ? table(header, group)
        : table([...header].reverse(), group.map((row) => [...row].reverse())));
      write(root, path, parts.join("\n\n## Next group\n\n"));
    }
    const result = run(root);
    assert.equal(result.status, 0, result.stderr || result.stdout);
  });
}

// IX-2: missing cross-table duplicate detection must fail for both index schemas.
for (const { path, header, rows } of indexes) {
  for (const reverse of [false, true]) {
    test(`IX-2 rejects a duplicate full path in ${path}, reverse=${reverse}`, (t) => {
      const root = repository(t);
      const parts = [table(header, rows), table(header, [rows[0]])];
      write(root, path, (reverse ? parts.reverse() : parts).join("\n\n"));
      assertIndexFailure(root, path, { code: "INDEX_DUPLICATE_PATH", record: rows[0][header.indexOf("Path")] });
    });
  }

  test(`IX-1 rejects a later table without Path in ${path}`, (t) => {
    const root = repository(t);
    const selected = header.map((_, index) => index).filter((index) => header[index] !== "Path");
    const later = table(selected.map((index) => header[index]), [selected.map((index) => rows[1][index])]);
    write(root, path, `${table(header, rows)}\n\n${later}`);
    assertIndexFailure(root, path, { code: "INDEX_REQUIRED_COLUMN" });
  });
}

// IX-2: later rows must reach every existing field/path comparison, not just coverage.
const mutations = [
  [0, "Type", "Lightweight ADR"], [0, "ID", "ADR-9999"], [0, "Title", "Different"],
  [0, "Decision Status", "Accepted"], [0, "Implementation Status", "Complete"],
  [0, "Scope", "Different"], [0, "Architecture Source", "N/A — different"],
  [0, "Superseded By", "docs/adr/ADR-9999-missing.md"],
  [0, "Path", "docs/adr/ADR-9999-missing.md"],
  [1, "ID", "ADD-9999"], [1, "Title", "Different"], [1, "Design Status", "Current"],
  [1, "Scope Level", "Service / Package internal"], [1, "Scope", "Different"],
  [1, "Demand Source", "https://example.test/different"],
  [1, "Superseded By", "docs/architecture/ADD-9999-missing.md"],
  [1, "Path", "docs/architecture/ADD-9999-missing.md"],
];
for (const [index, field, value] of mutations) {
  const { path, header, rows } = indexes[index];
  test(`IX-2 validates later-table ${field} in ${path}`, (t) => {
    const root = repository(t);
    const cells = [...rows[1]];
    cells[header.indexOf(field)] = value;
    write(root, path, `${table(header, rows.filter((_, row) => row !== 1))}\n\n${table(header, [cells])}`);
    const record = field === "Path" ? value : rows[1][header.indexOf("Path")];
    assertIndexFailure(root, path, { record });
  });
}

// Places false index content in literal examples before the real tables.
function examples(content) {
  return `<!--\n${content}\n-->\n\n\`\`\`md\n${content}\n\`\`\`\n\n`;
}

for (const { path, header, rows } of indexes) {
  test(`IX-3 ignores example rows before multiple real tables in ${path}`, (t) => {
    const root = repository(t);
    const wrong = [...rows[0]];
    wrong[header.indexOf("Path")] = "docs/adr/ADR-9999-missing.md";
    const parts = [table(header, rows.slice(0, 1)), table(header, rows.slice(1))];
    write(root, path, examples(table(header, [wrong])) + parts.join("\n\n"));
    const result = run(root);
    assert.equal(result.status, 0, result.stderr || result.stdout);
  });

  test(`IX-3 requires a real table in ${path}`, (t) => {
    const root = repository(t);
    write(root, path, examples(table(header, rows)));
    assertIndexFailure(root, path, { code: "INDEX_TABLE_REQUIRED" });
  });

  test(`IX-3 rejects example-only record membership in ${path}`, (t) => {
    const root = repository(t);
    write(root, path, examples(table(header, rows.slice(1))) + table(header, rows.slice(0, 1)));
    const result = run(root);
    assert.equal(result.status, 1, result.stderr || result.stdout);
    assert.ok(result.stderr.split("\n").some((line) => line.startsWith(`${rows[1][header.indexOf("Path")]}:`)), result.stderr);
  });
}

// IX-3/IX-4 specify parser results; examples use the CLI's comment normalization.
test("IX-3 enumerates only real tables and preserves their independent cells", () => {
  assert.equal(typeof markdown.tablesFromContent, "function");
  const first = table(["ID", "Title"], [["one", "First"]]);
  const second = table(["Title", "ID"], [["Second", "two"]]);
  const active = markdown.stripHtmlComments(examples(first) + first + "\n\n" + second);
  assert.deepEqual(markdown.tablesFromContent(active), [
    { header: ["ID", "Title"], rows: [["one", "First"]] },
    { header: ["Title", "ID"], rows: [["Second", "two"]] },
  ]);
});

test("IX-3 returns no tables for example-only or unstructured input", () => {
  assert.equal(typeof markdown.tablesFromContent, "function");
  for (const content of ["", "No archived records.", "| ID |\n| not a separator |", examples(table(["ID"], [["one"]]))]) {
    assert.deepEqual(markdown.tablesFromContent(markdown.stripHtmlComments(content)), []);
  }
});

test("IX-4 keeps the legacy first-table or undefined result", () => {
  const first = table(["ID"], [["one"]]);
  const second = table(["Title", "ID"], [["Second", "two"]]);
  const active = markdown.stripHtmlComments(examples(second) + first + "\n\n" + second);
  assert.deepEqual(markdown.tableFromContent(active), { header: ["ID"], rows: [["one"]] });
  assert.equal(markdown.tableFromContent(markdown.stripHtmlComments(examples(first))), undefined);
});
