import { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Cpu, ShieldCheck, Zap } from "lucide-react";
import { Banner, Button, Panel, ProgressBar } from "../components/ui";
import { solveModel } from "../services/api/solver";
import { useBackend } from "../services/BackendContext";
import { useWorkspace } from "../workspace/WorkspaceContext";

export default function Solving() {
  const { workspace, setResult, clearResult } = useWorkspace();
  const { state } = useBackend();
  const navigate = useNavigate();
  const [elapsed, setElapsed] = useState(0);
  const [phase, setPhase] = useState("Dispatching model to the solver core…");
  const [error, setError] = useState<string | null>(null);
  const startedRef = useRef(false);
  const model = workspace.model!;

  useEffect(() => {
    if (startedRef.current) return;
    startedRef.current = true;
    clearResult();

    const start = performance.now();
    const timer = window.setInterval(() => {
      const ms = performance.now() - start;
      setElapsed(ms / 1000);
      if (ms > 600) setPhase("Solving in the C++ core…");
      if (ms > 4000) setPhase("Still solving — large or difficult models can take longer…");
    }, 100);

    const minDisplay = new Promise((r) => setTimeout(r, 1400));

    Promise.all([
      solveModel(model),
      minDisplay,
    ])
      .then(([result]) => {
        setResult(result);
        navigate("/optimizer/results", { replace: true });
      })
      .catch((err: Error) => setError(err.message))
      .finally(() => window.clearInterval(timer));

    return () => window.clearInterval(timer);
  }, [model, navigate, setResult, clearResult]);

  const backendLabel =
    state.kind === "up"
      ? `FastAPI ${state.health.api_version} · solver_cli ${state.health.solver_version ?? ""}`.trim()
      : "Backend status unknown";

  if (error) {
    return (
      <div className="mx-auto max-w-2xl pt-10">
        <Banner tone="bad" title="Solve failed">
          {error}
        </Banner>
        <div className="mt-4 flex gap-3">
          <Button variant="secondary" onClick={() => navigate("/optimizer/review")}>
            Back to review
          </Button>
          <Button variant="ghost" onClick={() => setError(null)}>
            Try again
          </Button>
        </div>
      </div>
    );
  }

  return (
    <div className="mx-auto max-w-2xl pt-8">
      <Panel className="glow-accent p-8 text-center">
        <div className="mx-auto grid h-16 w-16 place-items-center rounded-2xl border border-accent/40 bg-accent/10 text-accent">
          <Cpu size={30} className="animate-pulse-soft" />
        </div>
        <div className="label-cap mt-5 text-accent">Step 3 — Solving</div>
        <h1 className="mt-2 text-2xl font-bold text-ink">Optimization in progress</h1>
        <p className="mt-2 text-sm text-ink-2">{phase}</p>

        <div className="mt-6">
          <ProgressBar indeterminate />
        </div>

        <div className="num mt-6 text-4xl font-bold text-accent tabular-nums">
          {elapsed.toFixed(1)}
          <span className="ml-1 text-lg text-ink-3">s elapsed</span>
        </div>
        <p className="mt-1 text-[11px] text-ink-3">
          Real wall-clock time measured in this browser session — no synthetic progress data.
        </p>

        <div className="mt-7 grid grid-cols-2 gap-3 text-left">
          <div className="panel-soft px-4 py-3">
            <div className="label-cap flex items-center gap-1.5">
              <Zap size={11} className="text-accent" /> Model
            </div>
            <div className="mt-1 truncate text-sm font-semibold text-ink">
              {model.name ?? "Untitled"}
            </div>
            <div className="num text-[11px] text-ink-3">
              {model.variables.length} vars · {model.constraints.length} constraints
            </div>
          </div>
          <div className="panel-soft px-4 py-3">
            <div className="label-cap flex items-center gap-1.5">
              <ShieldCheck size={11} className="text-good" /> Backend
            </div>
            <div className="mt-1 truncate text-sm font-semibold text-ink">{backendLabel}</div>
            <div className="num text-[11px] text-ink-3">verification enabled</div>
          </div>
        </div>
      </Panel>

      <p className="mt-4 text-center text-xs text-ink-3">
        You will land on the results screen as soon as the solver returns a verified answer.
      </p>
    </div>
  );
}
