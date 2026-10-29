// ADR: docs/adr/ADR-0007-governance-fixture-lifecycle.md
// Direct repair authorization: docs/reviews/2026-10-16-peer-assessment-followup.md.
// Exercises actual runner teardown, including failure and concurrent owners.
import assert from "node:assert/strict";
import { fork } from "node:child_process";
import { once } from "node:events";
import { existsSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const driver = fileURLToPath(new URL("fixtures/fixture-lifecycle-child.mjs", import.meta.url));

// Owns only this probe's parent directory; the child owns its fixture roots.
function parentDirectory(t) {
  const parent = mkdtempSync(join(tmpdir(), "orange-lifecycle-probe-"));
  t.after(() => rmSync(parent, { recursive: true, force: true }));
  writeFileSync(join(parent, "sentinel"), "unrelated parent content");
  return parent;
}

// Starts a real test runner with an isolated TMPDIR and a bounded IPC handshake.
async function launch(t, parent, scenario) {
  const child = fork(driver, [scenario], {
    env: { ...process.env, TMPDIR: parent },
    stdio: ["ignore", "pipe", "pipe", "ipc"],
    execArgv: [],
  });
  t.after(() => { if (child.exitCode === null) child.kill(); });
  let output = "";
  child.stdout.on("data", (bytes) => { output += bytes; });
  child.stderr.on("data", (bytes) => { output += bytes; });
  const completed = once(child, "close");
  const ready = once(child, "message").then(([message]) => message);
  const message = await Promise.race([
    ready,
    completed.then(([code]) => { throw new Error(`probe exited before ready (${code}): ${output}`); }),
  ]);
  return { child, roots: message.roots, completed, output: () => output };
}

// Releases one owner and verifies every reported allocation is gone at exit.
async function finish(probe, expectedCode) {
  probe.child.send("release");
  const [code, signal] = await probe.completed;
  assert.equal(signal, null, probe.output());
  assert.equal(code, expectedCode, probe.output());
  for (const root of probe.roots) assert.equal(existsSync(root), false, root);
}

for (const scenario of ["success", "multiple", "removed", "throw", "outside"]) {
  test(`fixture cleanup after ${scenario}`, { timeout: 30000 }, async (t) => {
    const parent = parentDirectory(t);
    const probe = await launch(t, parent, scenario);
    await finish(probe, scenario === "throw" ? 1 : 0);
    if (scenario === "throw") assert.match(probe.output(), /ORANGE_EXPECTED_FIXTURE_FAILURE/);
    assert.deepEqual(readdirSync(parent), ["sentinel"]);
  });
}

test("one child cannot delete another live child's fixtures", { timeout: 30000 }, async (t) => {
  const parent = parentDirectory(t);
  const first = await launch(t, parent, "success");
  const second = await launch(t, parent, "success");
  assert.notEqual(first.roots[0], second.roots[0]);
  await finish(first, 0);
  assert.equal(existsSync(second.roots[0]), true);
  await finish(second, 0);
  assert.deepEqual(readdirSync(parent), ["sentinel"]);
});
