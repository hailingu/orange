// ADR: docs/adr/ADR-0007-governance-fixture-lifecycle.md
// Direct repair authorization: docs/reviews/2026-10-16-peer-assessment-followup.md.
// Real child-runner probe: reports ownership before waiting for parent release.
import assert from "node:assert/strict";
import { rmSync, writeFileSync } from "node:fs";
import { once } from "node:events";
import { join } from "node:path";
import test from "node:test";
import * as fixtures from "../fixtures.mjs";

test("fixture ownership probe", { timeout: 25000 }, async (t) => {
  const scenario = process.argv[2];
  const roots = [fixtures.validRepository(t)];
  if (scenario === "multiple") roots.push(fixtures.validRepository(t));
  if (scenario === "outside") {
    roots.push(fixtures.temporaryDirectory(t, "orange-governance-outside-"));
    writeFileSync(join(roots[1], "outside.md"), "# Outside\n");
  }
  process.send({ roots });
  await once(process, "message");
  process.disconnect();
  if (scenario === "removed") rmSync(roots[0], { recursive: true });
  if (scenario === "throw") assert.fail("ORANGE_EXPECTED_FIXTURE_FAILURE");
});
