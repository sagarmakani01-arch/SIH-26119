import { Cpu, Database, RefreshCw, Server, ShieldCheck, Zap, ZapOff } from "lucide-react";
import { Badge, Banner, Button, PageHeader, Panel, Spinner } from "../components/ui";
import { useBackend } from "../services/BackendContext";

interface Row {
  icon: typeof Server;
  name: string;
  state: "up" | "down" | "unknown" | "off";
  detail: string;
}

export default function Status() {
  const { state, refresh } = useBackend();

  const apiUp = state.kind === "up";
  const health = state.kind === "up" ? state.health : null;

  const rows: Row[] = [
    {
      icon: Server,
      name: "Frontend (this UI)",
      state: "up",
      detail: "React + Vite static app",
    },
    {
      icon: Server,
      name: "API — FastAPI",
      state: apiUp ? "up" : state.kind === "loading" ? "unknown" : "down",
      detail: apiUp
        ? `status ${health!.status} · api_version ${health!.api_version}`
        : state.kind === "down"
          ? state.message
          : "Checking…",
    },
    {
      icon: Cpu,
      name: "Solver core (solver_cli)",
      state: health?.solver_cli.available ? "up" : health ? "down" : "unknown",
      detail: health?.solver_cli.available
        ? `${health.solver_cli.path} · version ${health.solver_version ?? "unknown"}`
        : health
          ? "Binary not found — set SOLVER_CLI_PATH or rebuild"
          : "Unknown until API responds",
    },
    {
      icon: Cpu,
      name: "CPU backend",
      state: health?.solver_cli.available ? "up" : health ? "down" : "unknown",
      detail: health?.solver_cli.available
        ? "Available — native C++20 engine"
        : "Depends on solver core availability",
    },
    {
      icon: ZapOff,
      name: "GPU backend (CUDA)",
      state: "off",
      detail:
        "Unavailable — no CUDA device/toolchain in this environment. The engine transparently uses CPU; speedups are never fabricated.",
    },
    {
      icon: Database,
      name: "Database / persistence",
      state: "off",
      detail:
        "Not connected — models & scenarios use browser-local storage. Server-side DB is on the backend roadmap.",
    },
  ];

  const toneFor = (s: Row["state"]) =>
    s === "up"
      ? "good"
      : s === "down"
        ? "bad"
        : s === "off"
          ? "warn"
          : "neutral";

  return (
    <div>
      <PageHeader
        eyebrow="Operations"
        title="System Status"
        description="Live health of every component behind this UI."
        right={
          <Button variant="secondary" onClick={refresh} disabled={state.kind === "loading"}>
            {state.kind === "loading" ? <Spinner /> : <RefreshCw size={15} />} Refresh
          </Button>
        }
      />

      <div className="mb-5 grid grid-cols-2 gap-3 md:grid-cols-4">
        <Panel className="p-4">
          <div className="label-cap">API</div>
          <div className="mt-1 flex items-center gap-2">
            <span
              className={`h-2.5 w-2.5 rounded-full ${
                apiUp ? "bg-good" : state.kind === "down" ? "bg-bad" : "bg-ink-3"
              }`}
            />
            <span className="text-sm font-semibold text-ink">
              {apiUp ? "Operational" : state.kind === "down" ? "Offline" : "Checking…"}
            </span>
          </div>
        </Panel>
        <Panel className="p-4">
          <div className="label-cap">Solver binary</div>
          <div className="mt-1 flex items-center gap-2">
            <span
              className={`h-2.5 w-2.5 rounded-full ${
                health?.solver_cli.available ? "bg-good" : health ? "bg-bad" : "bg-ink-3"
              }`}
            />
            <span className="text-sm font-semibold text-ink">
              {health?.solver_cli.available ? "Available" : health ? "Missing" : "Unknown"}
            </span>
          </div>
        </Panel>
        <Panel className="p-4">
          <div className="label-cap">Compute</div>
          <div className="mt-1 flex items-center gap-2">
            <Cpu size={15} className="text-good" />
            <span className="text-sm font-semibold text-ink">CPU active</span>
          </div>
        </Panel>
        <Panel className="p-4">
          <div className="label-cap">GPU</div>
          <div className="mt-1 flex items-center gap-2">
            <ZapOff size={15} className="text-warn" />
            <span className="text-sm font-semibold text-ink">Unavailable</span>
          </div>
        </Panel>
      </div>

      {state.kind === "down" && (
        <div className="mb-5">
          <Banner tone="bad" title="Backend unreachable">
            {state.message} — start it with{" "}
            <code className="num">docker compose up -d</code> or{" "}
            <code className="num">uvicorn app.main:app</code> from <code>backend/</code>.
          </Banner>
        </div>
      )}

      <Panel className="divide-y divide-line/70">
        {rows.map((r) => (
          <div key={r.name} className="flex items-start gap-4 px-5 py-4">
            <span className="mt-0.5 grid h-9 w-9 shrink-0 place-items-center rounded-xl border border-line-2 bg-panel-2 text-accent">
              <r.icon size={16} />
            </span>
            <div className="min-w-0 flex-1">
              <div className="flex flex-wrap items-center gap-2">
                <span className="text-sm font-semibold text-ink">{r.name}</span>
                <Badge tone={toneFor(r.state) as "good" | "bad" | "warn" | "neutral"}>
                  {r.state === "up"
                    ? "Available"
                    : r.state === "down"
                      ? "Unavailable"
                      : r.state === "off"
                        ? "Not configured"
                        : "Unknown"}
                </Badge>
              </div>
              <div className="num mt-1 truncate text-xs text-ink-3">{r.detail}</div>
            </div>
            {r.name.includes("GPU") ? (
              <Zap size={14} className="mt-2 shrink-0 text-ink-3" />
            ) : r.state === "up" ? (
              <ShieldCheck size={16} className="mt-2 shrink-0 text-good" />
            ) : null}
          </div>
        ))}
      </Panel>

      {health && (
        <div className="num mt-4 text-[11px] text-ink-3">
          solver_cli.path = {health.solver_cli.path ?? "null"} · page refreshed at{" "}
          {new Date().toLocaleTimeString()}
        </div>
      )}
    </div>
  );
}
