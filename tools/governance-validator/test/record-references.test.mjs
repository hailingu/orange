// ADR: docs/adr/ADR-0002-governance-validator-adoption.md, docs/adr/ADR-0007-governance-fixture-lifecycle.md
// Direct repair authorization: docs/reviews/2026-10-16-peer-assessment-followup.md.
// Exercises active concrete record identities through real CLI documents.
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import test from "node:test";
import { run, validRepository, write } from "./fixtures.mjs";

const missing = "docs/adr/ADR-9999-missing.md";
const existing = "docs/adr/ADR-0001-example.md";

for (const reference of [`See \`${missing}\`.`, `See [decision](${missing}).`, `See ${missing}.`]) {
  test(`rejects a dangling active record reference: ${reference}`, (t) => {
    const root = validRepository(t);
    write(root, "docs/guide.md", reference);
    const result = run(root);
    assert.equal(result.status, 1, result.stderr);
    assert.match(result.stderr, /RECORD_REFERENCE_INVALID/);
    assert.match(result.stderr, /ADR-9999-missing/);
  });
}

test("accepts concrete references, relative Markdown links and inert examples", (t) => {
  const root = validRepository(t);
  write(root, "docs/guide.md", `See \`${existing}\`, [decision](adr/ADR-0001-example.md#decision)
and [relative decision](../docs/adr/ADR-0001-example.md).
<!-- ${missing} -->
\`\`\`markdown
${missing}
\`\`\`
Filename grammar: \`docs/adr/ADR-NNNN-<slug>.md\`.
External: [example](https://example.test/docs/adr/ADR-9999-example.md).
`);
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("rejects a dangling document-relative record link", (t) => {
  const root = validRepository(t);
  write(root, "docs/guide.md", "See [decision](adr/ADR-9999-missing.md#decision).");
  const result = run(root);
  assert.equal(result.status, 1, result.stderr);
  assert.match(result.stderr, /RECORD_REFERENCE_INVALID/);
});

test("active records retain inactive future archival instructions", (t) => {
  const root = validRepository(t);
  write(root, existing, readFileSync(join(root, existing), "utf8")
    .replace("Inactive guidance.", `Move to \`docs/adr/archive/ADR-0001-example.md\` when retired.`));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
});

test("an ordinary Archival heading does not suppress an active reference", (t) => {
  const root = validRepository(t);
  write(root, "docs/guide.md", `## Archival\nSee \`${missing}\`.`);
  const result = run(root);
  assert.equal(result.status, 1, result.stderr);
  assert.match(result.stderr, /RECORD_REFERENCE_INVALID/);
});
