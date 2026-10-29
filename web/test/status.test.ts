// ADR: docs/adr/ADR-0003-local-runtime-path.md
// Pins the T-3 status-page contract (CC-7): the health request goes to
// /api/health, a 200 with exactly the declared status object visibly
// renders ok, and any failure — transport error, non-200 answer, wrong
// body, or the fail-fast deadline — replaces loading with a nonempty
// connection-error message and leaves no success state, well inside AC-8's
// two-second budget.
import { test } from "node:test";
import assert from "node:assert/strict";
import {
  applyStatus,
  FAIL_FAST_MS,
  HEALTH_PATH,
  renderServiceStatus,
  type FetchLike,
  type StatusElement,
} from "../src/status.js";

// One status element double carrying the two observable surfaces.
function elementOf(): StatusElement {
  const dataset = { state: "unloaded" };
  return { textContent: "", dataset };
}

// One fetch double answering a canned response after an optional delay.
interface CannedResponse {
  ok?: boolean;
  status?: number;
  body?: string;
}
function fetchAnswering(response: CannedResponse, delayMs = 0): FetchLike {
  return async () => {
    if (delayMs > 0) {
      await new Promise((resolve) => setTimeout(resolve, delayMs));
    }
    return {
      ok: response.ok ?? true,
      status: response.status ?? 200,
      text: async () => response.body ?? '{"status":"ok"}',
    };
  };
}

test("the health path is the declared route", () => {
  assert.equal(HEALTH_PATH, "/api/health");
});

test("the fail-fast deadline fits AC-8's two-second budget", () => {
  assert.ok(FAIL_FAST_MS > 0 && FAIL_FAST_MS < 2000);
});

test("each state renders its exact visible text", () => {
  const loading = elementOf();
  applyStatus(loading, "loading");
  assert.equal(loading.dataset.state, "loading");
  assert.equal(loading.textContent, "loading…");
  const ok = elementOf();
  applyStatus(ok, "ok");
  assert.equal(ok.dataset.state, "ok");
  assert.equal(ok.textContent, "ok");
  const error = elementOf();
  applyStatus(error, "error");
  assert.equal(error.dataset.state, "error");
  assert.ok(error.textContent.length > 0);
  assert.notEqual(error.textContent, "ok");
});

test("a 200 with the declared body renders ok", async () => {
  const element = elementOf();
  await renderServiceStatus(element, fetchAnswering({ status: 200, body: '{"status":"ok"}' }));
  assert.equal(element.dataset.state, "ok");
  assert.equal(element.textContent, "ok");
});

test("the request targets /api/health", async () => {
  const element = elementOf();
  const seen: string[] = [];
  await renderServiceStatus(element, async (input) => {
    seen.push(input);
    return fetchAnswering({})(input);
  });
  assert.deepEqual(seen, [HEALTH_PATH]);
});

test("a transport failure renders the error state", async () => {
  const element = elementOf();
  const failing: FetchLike = async () => {
    throw new TypeError("fetch failed");
  };
  await renderServiceStatus(element, failing);
  assert.equal(element.dataset.state, "error");
  assert.ok(element.textContent.length > 0);
  assert.notEqual(element.textContent, "ok");
});

test("a non-200 answer renders the error state", async () => {
  const element = elementOf();
  await renderServiceStatus(element, fetchAnswering({ ok: false, status: 503, body: "oops" }));
  assert.equal(element.dataset.state, "error");
  assert.notEqual(element.textContent, "ok");
});

test("a 200 with a wrong body renders the error state", async () => {
  const element = elementOf();
  await renderServiceStatus(element, fetchAnswering({ status: 200, body: '{"status":"down"}' }));
  assert.equal(element.dataset.state, "error");
});

test("an aborted fetch renders the error state within the budget", async () => {
  const element = elementOf();
  const hanging: FetchLike = (_input, init) =>
    new Promise((_resolve, reject) => {
      init?.signal?.addEventListener("abort", () => reject(new DOMException("aborted", "AbortError")));
    });
  const started = Date.now();
  await renderServiceStatus(element, hanging);
  const elapsed = Date.now() - started;
  assert.equal(element.dataset.state, "error");
  assert.ok(elapsed < 2000, `error state after ${elapsed} ms must stay under the 2 s budget`);
});
