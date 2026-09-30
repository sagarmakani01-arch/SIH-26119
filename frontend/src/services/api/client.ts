export class ApiError extends Error {
  constructor(
    public readonly status: number,
    message: string,
  ) {
    super(message);
    this.name = "ApiError";
  }
}

function extractDetail(data: unknown): string | null {
  if (!data || typeof data !== "object") return null;
  const detail = (data as { detail?: unknown }).detail;
  if (typeof detail === "string") return detail;
  if (Array.isArray(detail)) {
    return detail
      .map((d) => {
        if (d && typeof d === "object" && "msg" in d) return String((d as { msg: unknown }).msg);
        return JSON.stringify(d);
      })
      .join("; ");
  }
  if (detail && typeof detail === "object" && "error" in detail) {
    return String((detail as { error: unknown }).error);
  }
  return null;
}

export async function request<T>(
  path: string,
  init?: RequestInit,
  timeoutMs = 130000,
): Promise<T> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(path, {
      ...init,
      signal: controller.signal,
      headers: { "Content-Type": "application/json", ...(init?.headers ?? {}) },
    });
    const text = await res.text();
    let data: unknown = null;
    if (text) {
      try {
        data = JSON.parse(text);
      } catch {
        data = null;
      }
    }
    if (!res.ok) {
      const message = extractDetail(data) ?? `HTTP ${res.status}`;
      throw new ApiError(res.status, message);
    }
    if (data === null) throw new ApiError(500, "Empty response from server");
    return data as T;
  } catch (err) {
    if (err instanceof DOMException && err.name === "AbortError") {
      throw new ApiError(504, "Request timed out");
    }
    if (err instanceof TypeError) {
      throw new ApiError(0, "Cannot reach the server — is the backend running?");
    }
    throw err;
  } finally {
    clearTimeout(timer);
  }
}
