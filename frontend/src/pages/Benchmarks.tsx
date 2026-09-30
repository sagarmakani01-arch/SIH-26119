import { useCallback, useEffect, useState } from "react";
import { Cpu, Gauge, Play, Timer, ZapOff } from "lucide-react";
import {
  Badge,
  Banner,
  Button,
  EmptyState,
  PageHeader,
  Panel,
  Stat,
} from "../components/ui";
import {
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import { solveModel } from "../services/api/solver";
import { getTemplate } from "../templates";
import type { SolverModel } from "../services/api/types";
import { formatMs, formatSeconds } from "../lib/format";

interface BenchRow {
  name: string;
  problemClass: string;
  vars: number;
  constraints: number;
  status: string;
  wallMs: number;
  solverSec: number;
}

interface BenchRun {
  ranAt: string;
  rows: BenchRow[];
}

const STORAGE_KEY = "sovereign-benchmarks-v1";

function lcg(seed: number): () => number {
  let s = seed >>> 0;
  return () => {
    s = (s * 1664525 + 1013904223) >>> 0;
    return s / 0xffffffff;
  };
}

function largeLP(): SolverModel {
  const rand = lcg(42);
  const nVars = 60;
  const nCons = 30;
  const variables = Array.from({ length: nVars }, (_, i) => ({
    name: `x${i + 1}`,
    type: "continuous" as const,
    lb: 0,
    ub: 10,
  }));
  const coefficients = Array.from({ length: nVars }, () => Number((rand() * 5).toFixed(3)));
  const constraints = Array.from({ length: nCons }, (_, i) => ({
    name: `cap${i + 1}`,
    sense: "<=" as const,
    coefficients: Array.from({ length: nVars }, (_, j) =>
      Number(((i + 1) * 0.01 + coefficients[(i * 7 + j) % nVars]).toFixed(3)),
    ),
    rhs: 120,
  }));
  const objectiveCoeffs = Array.from(
    { length: nVars },
    (_, i) => Number((5 + (i % 9) + rand() * 4).toFixed(3)),
  );
  return {
    name: "Synthetic large LP (60×30)",
    objective: { sense: "maximize", coefficients: objectiveCoeffs },
    variables,
    constraints,
    config: { presolve: true, verify: true },
  };
}

function knapsackMILP(): SolverModel {
  const rand = lcg(7);
  const n = 30;
  const values = Array.from({ length: n }, () => Math.round(50 + rand() * 150));
  const weights = Array.from({ length: n }, () => Math.round(20 + rand() * 80));
  const budget = 600;
  return {
    name: "Synthetic knapsack MILP (30 binaries)",
    objective: { sense: "maximize", coefficients: values },
    variables: Array.from({ length: n }, (_, i) => ({
      name: `item${i + 1}`,
      type: "binary" as const,
      lb: 0,
      ub: 1,
    })),
    constraints: [{ name: "budget", sense: "<=", coefficients: weights, rhs: budget }],
    config: { presolve: true, verify: true, mip_rel_gap: 0 },
  };
}

function buildSuite(): { name: string; build: () => SolverModel }[] {
  const ids = ["production", "logistics", "refinery", "scheduling"];
  return [
    ...ids.map((id) => ({
      name: getTemplate(id)!.name,
      build: () => getTemplate(id)!.build(getTemplate(id)!.defaults()).model,
    })),
    { name: "Synthetic large LP (60×30)", build: largeLP },
    { name: "Synthetic knapsack MILP (30 binaries)", build: knapsackMILP },
  ];
}

function loadRun(): BenchRun | null {
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    return raw ? (JSON.parse(raw) as BenchRun) : null;
  } catch {
    return null;
  }
}

export default function Benchmarks() {
  const [run, setRun] = useState<BenchRun | null>(loadRun);
  const [running, setRunning] = useState(false);
  const [progress, setProgress] = useState({ done: 0, total: 0, current: "" });
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (run) {
      try {
        window.localStorage.setItem(STORAGE_KEY, JSON.stringify(run));
      } catch {
        // ignore
      }
    }
  }, [run]);

  const start = useCallback(async () => {
    setRunning(true);
    setError(null);
    const suite = buildSuite();
    setProgress({ done: 0, total: suite.length, current: suite[0].name });
    const rows: BenchRow[] = [];
    try {
      for (let i = 0; i < suite.length; i++) {
        const entry = suite[i];
        setProgress({ done: i, total: suite.length, current: entry.name });
        const model = entry.build();
        const t0 = performance.now();
        const result = await solveModel(model);
        const wallMs = performance.now() - t0;
        rows.push({
          name: entry.name,
          problemClass: result.problem_class,
          vars: model.variables.length,
          constraints: model.constraints.length,
          status: result.status,
          wallMs,
          solverSec: result.statistics.solve_time_sec,
        });
      }
      setRun({ ranAt: new Date().toISOString(), rows });
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setRunning(false);
      setProgress({ done: 0, total: 0, current: "" });
    }
  }, []);

  const totalWall = run ? run.rows.reduce((acc, r) => acc + r.wallMs, 0) : 0;
  const fastest = run ? run.rows.reduce((a, b) => (a.wallMs < b.wallMs ? a : b)) : null;

  return (
    <div>
      <PageHeader
        eyebrow="Performance"
        title="Measured CPU Benchmarks"
        description="Every timing below is measured live in this browser session against the real solver core over HTTP. Nothing is pre-recorded or estimated."
        right={
          <Button onClick={() => void start()} disabled={running}>
            {running ? <Timer size={15} className="animate-pulse-soft" /> : <Play size={15} />}
            {running
              ? `Running ${progress.done + 1}/${progress.total}…`
              : run
                ? "Re-run suite"
                : "Run benchmark suite"}
          </Button>
        }
      />

      <div className="mb-5 grid gap-4 lg:grid-cols-3">
        <Panel className="p-5">
          <div className="flex items-center gap-2 text-good">
            <Cpu size={16} />
            <span className="text-sm font-bold uppercase tracking-wider">CPU — active</span>
          </div>
          <p className="mt-2 text-[13px] text-ink-2">
            Native C++ core (MSVC Release build) behind FastAPI. Wall-clock includes the HTTP
            round-trip; solver-internal time is reported separately per row.
          </p>
        </Panel>
        <Panel className="p-5">
          <div className="flex items-center gap-2 text-warn">
            <ZapOff size={16} />
            <span className="text-sm font-bold uppercase tracking-wider">GPU — unavailable</span>
          </div>
          <p className="mt-2 text-[13px] text-ink-2">
            No CUDA device/toolchain in this environment, so no GPU row exists here. GPU numbers
            appear only when a{" "}
            <code className="num text-ink">SOLVER_ENABLE_CUDA</code> build reports a real device —
            we never fabricate speedups.
          </p>
        </Panel>
        <Panel className="p-5">
          <div className="flex items-center gap-2 text-accent">
            <Gauge size={16} />
            <span className="text-sm font-bold uppercase tracking-wider">Suite</span>
          </div>
          <p className="mt-2 text-[13px] text-ink-2">
            4 guided templates + 2 synthetic stress models (dense 60×30 LP, 30-binary knapsack).
            All problem data is synthetic — labeled as such.
          </p>
        </Panel>
      </div>

      {error && (
        <div className="mb-5">
          <Banner tone="bad" title="Benchmark run failed">
            {error}
          </Banner>
        </div>
      )}

      {running && (
        <div className="mb-5">
          <Banner tone="info" title={`Benchmark running — ${progress.current}`}>
            Real solves are in progress; results appear when the suite finishes.
          </Banner>
        </div>
      )}

      {!run && !running ? (
        <EmptyState
          icon={<Gauge size={24} />}
          title="No benchmark data yet"
          body="Press “Run benchmark suite” to measure solve times against the live backend. Only measured results are ever displayed."
          action={
            <Button onClick={() => void start()}>
              <Play size={15} /> Run benchmark suite
            </Button>
          }
        />
      ) : (
        run && (
          <>
            <div className="mb-5 grid grid-cols-2 gap-3 md:grid-cols-4">
              <Stat label="Last run" value={new Date(run.ranAt).toLocaleString()} tone="accent" />
              <Stat label="Suite wall time" value={formatMs(totalWall)} />
              <Stat label="Fastest solve" value={fastest ? formatMs(fastest.wallMs) : "—"} tone="good" />
              <Stat label="Models" value={run.rows.length} />
            </div>

            <div className="grid gap-5 lg:grid-cols-2">
              <Panel className="p-5">
                <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                  Wall-clock time per model (includes HTTP)
                </h3>
                <div className="mt-4 h-72">
                  <ResponsiveContainer width="100%" height="100%">
                    <BarChart
                      data={run.rows.map((r) => ({ name: r.name.slice(0, 22), ms: Math.round(r.wallMs) }))}
                      margin={{ top: 8, right: 12, left: -8, bottom: 4 }}
                    >
                      <CartesianGrid stroke="#3a3578" strokeDasharray="3 3" vertical={false} />
                      <XAxis dataKey="name" tick={{ fill: "#8f86c9", fontSize: 10 }} tickLine={false} axisLine={false} />
                      <YAxis tick={{ fill: "#8f86c9", fontSize: 11 }} tickLine={false} axisLine={false} />
                      <Tooltip
                        contentStyle={{
                          background: "#232052",
                          border: "1px solid #4c4694",
                          borderRadius: "10px",
                          fontSize: 12,
                        }}
                      />
                      <Bar dataKey="ms" name="ms" radius={[6, 6, 0, 0]}>
                        {run.rows.map((_, i) => (
                          <Cell key={i} fill={i % 2 === 0 ? "#2dd4bf" : "#c084fc"} />
                        ))}
                      </Bar>
                    </BarChart>
                  </ResponsiveContainer>
                </div>
              </Panel>

              <Panel className="overflow-x-auto p-5">
                <div className="flex items-center justify-between">
                  <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                    Results table
                  </h3>
                  <Badge tone="good">Live measurements</Badge>
                </div>
                <table className="mt-3 w-full text-left text-[13px]">
                  <thead>
                    <tr className="border-b border-line text-[11px] uppercase tracking-wider text-ink-3">
                      <th className="py-2 pr-3 font-semibold">Model</th>
                      <th className="py-2 pr-3 font-semibold">Class</th>
                      <th className="py-2 pr-3 font-semibold">Size</th>
                      <th className="py-2 pr-3 font-semibold">Status</th>
                      <th className="py-2 pr-3 font-semibold">Wall</th>
                      <th className="py-2 font-semibold">Solver</th>
                    </tr>
                  </thead>
                  <tbody className="num">
                    {run.rows.map((r, i) => (
                      <tr key={i} className="border-b border-line/60 text-ink-2">
                        <td className="py-2 pr-3 text-ink">{r.name}</td>
                        <td className="py-2 pr-3 text-accent">{r.problemClass}</td>
                        <td className="py-2 pr-3 text-ink-3">
                          {r.vars}×{r.constraints}
                        </td>
                        <td className="py-2 pr-3">{r.status}</td>
                        <td className="py-2 pr-3 font-semibold text-ink">{formatMs(r.wallMs)}</td>
                        <td className="py-2 text-ink-3">{formatSeconds(r.solverSec)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </Panel>
            </div>

            <div className="mt-4 text-[11px] text-ink-3">
              Synthetic problem data — not MRPL plant data. Wall time = browser{" "}
              <code className="num">performance.now()</code> around the HTTP request; solver time ={" "}
              <code className="num">statistics.solve_time_sec</code> measured inside the backend.
            </div>
          </>
        )
      )}
    </div>
  );
}
