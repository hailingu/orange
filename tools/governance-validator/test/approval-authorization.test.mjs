// ADR: docs/adr/ADR-0005-required-approver-match.md, docs/adr/ADR-0007-governance-fixture-lifecycle.md
// Exercises RA-1 through RA-3 at the real CLI boundary with complete records.
// In particular, removing actor equality must let the outsider cases fail.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import test from "node:test";
import { acceptedAdr, acceptedOcr, run, validRepository, write } from "./fixtures.mjs";

const families = ["Full ADR", "Lightweight ADR", "OCR", "ADD"];
const approval = {
  Approver: "@linhai",
  "Approval Time": "2026-08-13T00:00:00Z",
  "Approval Evidence": "Approve",
};

// Changes controlled fixture metadata without editing its other contracts.
function setMetadata(root, path, fields) {
  let markdown = readFileSync(join(root, path), "utf8");
  for (const [field, value] of Object.entries(fields)) {
    const entry = `- **${field}**: ${value}`;
    const pattern = new RegExp(`^- \\*\\*${field}\\*\\*:[^\\n]*$`, "m");
    markdown = pattern.test(markdown)
      ? markdown.replace(pattern, () => entry)
      : markdown.replace("## Metadata [Required]\n", `## Metadata [Required]\n${entry}\n`);
  }
  write(root, path, markdown);
}


// Reuses the accepted ADR fixture while selecting its lightweight contract.
function lightweightAdr(root, path) {
  const markdown = readFileSync(join(root, path), "utf8")
    .replace("# ADR-0002:", "# Lightweight ADR-0002:")
    .replace("## Context [Required]", "## Lightweight Eligibility Check [Required]\nEligible localized fixture.\n## Context [Required]");
  write(root, path, markdown);
  const index = "docs/adr/INDEX.md";
  write(root, index, readFileSync(join(root, index), "utf8")
    .replace("| Full ADR | ADR-0002 |", "| Lightweight ADR | ADR-0002 |"));
}

// Supplies one otherwise-valid record at its existing approval-bearing stage.
function approvalFixture(t, family) {
  const root = validRepository(t);
  let path = "docs/adr/ADR-0002-example.md";
  if (family === "ADD") {
    path = "docs/architecture/ADD-0001-example.md";
    setMetadata(root, path, { "Design Status": "Current", ...approval });
    const index = "docs/architecture/INDEX.md";
    write(root, index, readFileSync(join(root, index), "utf8").replace("| Draft |", "| Current |"));
  } else if (family === "OCR") {
    acceptedOcr(root, "0002");
    path = "docs/adr/ocr/OCR-0002-example.md";
  } else {
    acceptedAdr(root, "0002");
    if (family === "Lightweight ADR") lightweightAdr(root, path);
  }
  return { root, path };
}

// RA-1 defines the diagnostic code; record identity is part of its contract.
function assertUnauthorized(root, path) {
  const result = run(root);
  assert.equal(result.status, 1, result.stderr || result.stdout);
  const diagnostic = result.stderr.split("\n")
    .find((line) => line.includes("APPROVAL_ACTOR_NOT_AUTHORIZED"));
  assert.ok(diagnostic?.includes(path), result.stderr);
}

// RA-1: omitting equality or bypassing one document family must break a case.
for (const family of families) {
  test(`RA-1 accepts the concrete required approver for ${family}`, (t) => {
    const { root } = approvalFixture(t, family);
    const result = run(root);
    assert.equal(result.status, 0, result.stderr || result.stdout);
  });

  test(`RA-1 rejects an unrelated approver for ${family}`, (t) => {
    const { root, path } = approvalFixture(t, family);
    setMetadata(root, path, { Approver: "@quality-probe-outsider" });
    assertUnauthorized(root, path);
  });
}

test("RA-1 compares concrete actor identities case-sensitively", (t) => {
  const { root, path } = approvalFixture(t, "Full ADR");
  setMetadata(root, path, { Approver: "@Linhai" });
  assertUnauthorized(root, path);
});

// RA-2: treating equality as an early success would bypass these existing gates.
const invalidApprovals = [
  ["self-approval", { Author: "@codex", "Required Approver": "@codex", Approver: "@codex" }],
  ["a role label", { "Required Approver": "@Human", Approver: "@Human" }],
  ["an invalid timestamp", { "Approval Time": "2026-99-99T99:99:99+99:99" }],
  ["non-exact evidence", { "Approval Evidence": "approve" }],
];
for (const [name, fields] of invalidApprovals) {
  test(`RA-2 rejects ${name} despite matching actors`, (t) => {
    const { root, path } = approvalFixture(t, "Full ADR");
    setMetadata(root, path, fields);
    const result = run(root);
    assert.equal(result.status, 1, result.stderr || result.stdout);
  });
}

test("RA-3 preserves the existing rejection of descriptive approver rules", (t) => {
  const { root, path } = approvalFixture(t, "Full ADR");
  setMetadata(root, path, { "Required Approver": "Decision Owner after independent review" });
  const result = run(root);
  assert.equal(result.status, 1, result.stderr || result.stdout);
});

test("RA-3 leaves Proposed ADR and Draft ADD approval untriggered", (t) => {
  const root = validRepository(t);
  for (const path of ["docs/adr/ADR-0001-example.md", "docs/architecture/ADD-0001-example.md"]) {
    setMetadata(root, path, {
      Approver: "N/A — not approved",
      "Approval Time": "N/A — not approved",
      "Approval Evidence": "N/A — not approved",
    });
  }
  const result = run(root);
  assert.equal(result.status, 0, result.stderr || result.stdout);
});

// RA-3: reading an inactive authorized value must not repair an active mismatch.
const inactiveApproval = Object.entries(approval)
  .map(([field, value]) => `- **${field}**: ${value}`).join("\n");
const inactiveExamples = [
  ["comment", `<!--\n${inactiveApproval}\n-->\n`],
  ["fenced example", `\`\`\`md\n${inactiveApproval}\n\`\`\`\n`],
  ["Change Log", `## Change Log [Required]\n${inactiveApproval}\n`],
];
for (const family of families) {
  for (const [name, example] of inactiveExamples) {
    test(`RA-3 ignores authorized metadata in a ${name} for ${family}`, (t) => {
      const { root, path } = approvalFixture(t, family);
      setMetadata(root, path, { Approver: "@quality-probe-outsider" });
      const markdown = readFileSync(join(root, path), "utf8");
      const changed = name === "Change Log"
        ? markdown.replace("## Change Log [Required]\n", example)
        : markdown.replace("## Metadata [Required]\n", `## Metadata [Required]\n${example}`);
      write(root, path, changed);
      assertUnauthorized(root, path);
    });
  }
}
