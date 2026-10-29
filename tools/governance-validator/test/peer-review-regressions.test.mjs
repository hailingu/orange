// ADR: docs/adr/ADR-0002-governance-validator-adoption.md, docs/adr/ADR-0007-governance-fixture-lifecycle.md
// Repair authorization: docs/reviews/2026-10-16-peer-assessment-repairs.md.
// Repairs authorized directly by @hailingu on 2026-10-16; see
// docs/reviews/2026-10-16-peer-assessment-repairs.md. Tests exercise CLI semantics.
import assert from "node:assert/strict";
import { readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import test from "node:test";
import { acceptedAdr, acceptedOcr, run, validRepository, write } from "./fixtures.mjs";

const adr = "docs/adr/ADR-0001-example.md";
const add = "docs/architecture/ADD-0001-example.md";
const index = "docs/adr/INDEX.md";


// Transforms controlled fixture grammar, never repository implementation text.
function edit(root, path, transform) {
  write(root, path, transform(readFileSync(join(root, path), "utf8")));
}

// Requires a validation rejection carrying the intended diagnostic.
function rejects(root, diagnostic) {
  const result = run(root);
  assert.equal(result.status, 1, result.stderr);
  assert.match(result.stderr, diagnostic);
}

// Relocates a controlled fixture and its indexed identity together.
function moveRecord(root, from, to, indexPath = index) {
  write(root, to, readFileSync(join(root, from), "utf8"));
  rmSync(join(root, from));
  edit(root, indexPath, (text) => text.replaceAll(from, to));
}

for (const [from, to, indexPath] of [
  [adr, "docs/adr/ocr/ADR-0001-example.md", index],
  [add, "docs/adr/ADD-0001-example.md", "docs/architecture/INDEX.md"],
]) {
  test(`rejects mismatched record location: ${to}`, (t) => {
    const root = validRepository(t);
    moveRecord(root, from, to, indexPath);
    rejects(root, /RECORD_LOCATION_INVALID/);
  });
}

for (const path of [adr, "docs/adr/ocr/OCR-0002-example.md", "docs/guide.md"]) {
  test(`syntax-checks Mermaid in ${path}`, (t) => {
    const root = validRepository(t);
    if (path.includes("OCR-")) acceptedOcr(root, "0002");
    if (path.endsWith("guide.md")) write(root, path, "# Guide\n");
    edit(root, path, (text) => text + "\n```mermaid\nthis is invalid\n```\n");
    rejects(root, /invalid Mermaid block/);
  });
}

test("OCR prefix requires the ocr directory", (t) => {
  const root = validRepository(t);
  acceptedOcr(root, "0002");
  moveRecord(root, "docs/adr/ocr/OCR-0002-example.md", "docs/adr/OCR-0002-example.md");
  rejects(root, /RECORD_LOCATION_INVALID/);
});

test("ordinary diagrams do not acquire ADD completeness gates", (t) => {
  const root = validRepository(t);
  write(root, "docs/guide.md", "# Guide\n```mermaid\nflowchart LR\nA --> B\n```\n");
  assert.equal(run(root).status, 0);
});

test("Deferred candidate needs a recorded reason", (t) => {
  const root = validRepository(t);
  edit(root, add, (text) => text.replace("| CAND-1 | Ready | None |", "| CAND-1 | Deferred | None |"));
  rejects(root, /CANDIDATE_DEFERRED_REASON_REQUIRED/);
});

test("Deferred candidate accepts an explicit reason column", (t) => {
  const root = validRepository(t);
  edit(root, add, (text) => text
    .replace("| ID | Status | ADR path |", "| ID | Status | ADR path | Status reason or evidence |")
    .replace("| --- | --- | --- |", "| --- | --- | --- | --- |")
    .replace("| CAND-1 | Ready | None |", "| CAND-1 | Deferred | None | Awaiting a captured requirement baseline. |"));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("risk legend is not a baseline dimension table", (t) => {
  const root = validRepository(t);
  edit(root, adr, (text) => text.replace("## Acceptance Checks", "\nLegend:\n\n| Status | Meaning |\n| --- | --- |\n| Pass | Verified |\n| Fail | Rejected |\n\n## Acceptance Checks"));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("fenced and commented template examples are inactive", (t) => {
  const root = validRepository(t);
  write(root, "docs/guide.md", "# Guide\n````markdown\n{{AUTHOR}}\n```mermaid\ninvalid\n```\n````\n<!-- {{AUTHOR}} -->\n");
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("active unresolved variables still reject", (t) => {
  const root = validRepository(t);
  write(root, "docs/guide.md", "# Guide\nAuthor: {{AUTHOR}}\n");
  rejects(root, /unresolved template variable AUTHOR/);
});

test("authoring templates may retain declared variables inside Mermaid", (t) => {
  const root = validRepository(t);
  write(root, "docs/architecture/template/example.md", "<!--\n| `{{NODE}}` | Component |\n-->\n```mermaid\nflowchart LR\n{{NODE}} --> A\n```\n");
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("OCR eligibility clauses are independent of list position", (t) => {
  const root = validRepository(t);
  acceptedOcr(root, "0002");
  edit(root, "docs/adr/ocr/OCR-0002-example.md", (text) => text.replace("## Eligibility [Required]\n", "## Eligibility [Required]\n- [x] Additional local precondition confirmed.\n"));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("a missing required eligibility confirmation still rejects after reordering", (t) => {
  const root = validRepository(t);
  acceptedOcr(root, "0002");
  edit(root, "docs/adr/ocr/OCR-0002-example.md", (text) => text.replace("- [x] Is reversible", "- [ ] Is reversible"));
  rejects(root, /Eligibility item "reversible"/);
});

test("a checked duplicate cannot hide an unconfirmed eligibility clause", (t) => {
  const root = validRepository(t);
  acceptedOcr(root, "0002");
  edit(root, "docs/adr/ocr/OCR-0002-example.md", (text) => text.replace("## Eligibility [Required]", "## Eligibility [Required]\n- [ ] Is reversible; recovery discards the output."));
  rejects(root, /Eligibility item "reversible"/);
});

test("archiving a record requires updating source provenance markers", (t) => {
  const root = validRepository(t);
  const archived = "docs/adr/archive/ADR-0001-example.md";
  edit(root, adr, (text) => text
    .replace("Decision Status**: Proposed", "Decision Status**: Rejected")
    .replace("Implementation Status**: Not Started", "Implementation Status**: Not Applicable")
    .replace("## Metadata [Required]", "## Metadata [Required]\n- **Rejector**: @linhai\n- **Rejection Time**: 2026-08-13T00:00:00Z\n- **Rejection Evidence**: Reject\n- **Rejection Reason**: Proposal withdrawn."));
  edit(root, index, (text) => text.replace("| Proposed | Not Started |", "| Rejected | Not Applicable |"));
  moveRecord(root, adr, archived);
  write(root, "service/src/example.cpp", `// Governed by ${adr}.\nint example;\n`);
  rejects(root, /PROVENANCE_RECORD_MISSING/);
  edit(root, "service/src/example.cpp", (text) => text.replace(adr, archived));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("a multiline source example is not a file provenance marker", (t) => {
  const root = validRepository(t);
  write(root, "tools/example.mjs", `// Governed by ${adr}.\nexport const example = \`\n// ADR: docs/adr/ADR-9999-example.md\n\`;\n`);
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("Markdown link supersession retains a complete service-root identity", (t) => {
  const root = validRepository(t);
  const old = "docs/adr/archive/ADR-0003-old.md";
  const replacement = "demo-service/docs/adr/ADR-0002-example.md";
  acceptedAdr(root, "0002");
  moveRecord(root, "docs/adr/ADR-0002-example.md", replacement);
  edit(root, replacement, (text) => text.replace("## Metadata [Required]", `## Metadata [Required]\n- **Supersedes**: [old](${old})`));
  write(root, old, readFileSync(join(root, adr), "utf8")
    .replace("ADR-0001:", "ADR-0003:")
    .replace("Decision Status**: Proposed", "Decision Status**: Superseded")
    .replace("Implementation Status**: Not Started", "Implementation Status**: Not Applicable")
    .replace("Superseded By**: None", `Superseded By**: [replacement](${replacement})`)
    .replace("## Metadata [Required]", "## Metadata [Required]\n- **Retired By**: @linhai\n- **Retirement Time**: 2026-08-13T00:00:00Z\n- **Retirement Evidence**: Supersede\n- **Retirement Reason**: Replaced by a revised decision."));
  edit(root, index, (text) => text + `| Full ADR | ADR-0003 | Example | Superseded | Not Applicable | Project | N/A — governance-only example | [old](${old}) | [replacement](${replacement}) |\n`);
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});
