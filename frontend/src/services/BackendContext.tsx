import { createContext, useContext, useEffect, useMemo, useState, type ReactNode } from "react";
import { getHealth } from "../services/api/solver";
import type { HealthResponse } from "../services/api/types";

export type BackendState =
  | { kind: "loading" }
  | { kind: "up"; health: HealthResponse }
  | { kind: "down"; message: string };

interface BackendContextValue {
  state: BackendState;
  refresh: () => void;
}

const BackendContext = createContext<BackendContextValue | null>(null);

export function BackendProvider({ children }: { children: ReactNode }) {
  const [state, setState] = useState<BackendState>({ kind: "loading" });
  const [nonce, setNonce] = useState(0);

  useEffect(() => {
    let cancelled = false;
    setState((prev) => (prev.kind === "loading" ? prev : { kind: "loading" }));
    getHealth()
      .then((health) => {
        if (!cancelled) setState({ kind: "up", health });
      })
      .catch((err: Error) => {
        if (!cancelled) setState({ kind: "down", message: err.message });
      });
    return () => {
      cancelled = true;
    };
  }, [nonce]);

  const value = useMemo(
    () => ({ state, refresh: () => setNonce((n) => n + 1) }),
    [state],
  );

  return <BackendContext.Provider value={value}>{children}</BackendContext.Provider>;
}

export function useBackend(): BackendContextValue {
  const ctx = useContext(BackendContext);
  if (!ctx) throw new Error("useBackend must be used inside BackendProvider");
  return ctx;
}
