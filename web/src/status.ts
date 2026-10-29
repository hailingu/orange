// ADR: docs/adr/ADR-0003-local-runtime-path.md
// The status module of the orange web client (ADR-0003, subtask T-3):
// fetches /api/health once and renders the observed status into the page's
// status element — ok on the declared successful answer, a connection-error
// state within AC-8's two-second budget on any failure, and no success
// trace after a failure. Wiring stub for the red phase: the contract is
// pinned by web/test/status.test.ts and the behavior lands in the
// implementing change.
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

// Wiring stub: renders nothing yet.
export function applyStatus(element: StatusElement, state: "loading" | "ok" | "error"): void {
  void element;
  void state;
}

// Wiring stub: performs no request.
export async function renderServiceStatus(element: StatusElement, fetchNow: FetchLike): Promise<void> {
  void element;
  void fetchNow;
}

// Wiring stub: binds nothing yet.
export function bindPage(): void {
  // Intentionally empty until the implementing change.
}
