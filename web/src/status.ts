// ADR: docs/adr/ADR-0003-local-runtime-path.md
// The status module of the orange web client (ADR-0003, subtask T-3):
// fetches /api/health once and renders the observed status into the page's
// status element. A successful response visibly renders ok; any failure —
// transport error, non-200 answer, wrong body, or the 1.5 s fail-fast
// deadline inside AC-8's two-second budget — replaces loading with a
// connection-error message and removes every success trace. The element
// surface stays minimal (textContent plus one data-state attribute) so the
// module runs under node --test without a DOM.
export type FetchLike = (input: string, init?: { signal?: AbortSignal }) => Promise<ResponseLike>;

export interface ResponseLike {
  readonly ok: boolean;
  readonly status: number;
  text(): Promise<string>;
}

export interface StatusElement {
  textContent: string;
  readonly dataset: { state: string };
}

export const HEALTH_PATH = "/api/health";
export const FAIL_FAST_MS = 1500;

// Renders one observed outcome into the status element; exported for the
// suite to pin each state's exact visible text.
export function applyStatus(element: StatusElement, state: "loading" | "ok" | "error"): void {
  element.dataset.state = state;
  element.textContent =
    state === "loading" ? "loading…" : state === "ok" ? "ok" : "connection error";
}

// Fetches /api/health through `fetchNow` and renders the outcome: a 200
// whose body is exactly the declared status object renders ok; anything
// else renders the connection-error state. The request aborts itself at
// FAIL_FAST_MS so the error state appears within AC-8's budget without
// waiting on the network's own timeouts.
export async function renderServiceStatus(
  element: StatusElement,
  fetchNow: FetchLike,
): Promise<void> {
  applyStatus(element, "loading");
  const controller = new AbortController();
  const deadline = setTimeout(() => controller.abort(), FAIL_FAST_MS);
  try {
    const response = await fetchNow(HEALTH_PATH, { signal: controller.signal });
    const body = await response.text();
    applyStatus(element, response.status === 200 && body === '{"status":"ok"}' ? "ok" : "error");
  } catch {
    applyStatus(element, "error");
  } finally {
    clearTimeout(deadline);
  }
}

// The browser entry point: binds the module to the served page's status
// element and the platform fetch. A missing element leaves the page's own
// loading state instead of throwing.
export function bindPage(): void {
  const element = document.getElementById("status");
  if (element === null) {
    return;
  }
  const status = element as HTMLElement & StatusElement;
  void renderServiceStatus(status, (input, init) => fetch(input, init));
}

// The served page is a module script; run the binding once on load. The
// typeof guard keeps the node --test import of this module DOM-free.
if (typeof document !== "undefined") {
  bindPage();
}
