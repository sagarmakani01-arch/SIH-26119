import { Link } from "react-router-dom";
import { BookOpen, ExternalLink, GitBranch, Server } from "lucide-react";
import { Badge, PageHeader, Panel } from "../components/ui";

const endpoints = [
  { method: "GET", path: "/api/health", desc: "Liveness + solver_cli availability" },
  { method: "GET", path: "/api/demo", desc: "Small demo model (LP, 2 vars)" },
  { method: "POST", path: "/api/validate", desc: "Validate a model — structural issues + statistics" },
  { method: "POST", path: "/api/solve", desc: "Solve a model — full result JSON with statistics" },
  { method: "GET", path: "/docs", desc: "Swagger UI (proxied from FastAPI)" },
];

const roadmap = [
  "POST /api/ai/generate-model — natural-language → model JSON (AI Model Generation panel already targets this path).",
  "Async job queue: POST /api/jobs + polling for long solves (the UI currently uses synchronous POST /api/solve with a 130 s timeout).",
  "Server-side model/scenario database (PostgreSQL) behind /api/models and /api/scenarios — UI currently uses browser storage, labeled as such.",
  "Quadratic programming (QP) model class — solver core roadmap; frontend honestly states LP/MILP active only.",
  "GPU benchmark rows — require a SOLVER_ENABLE_CUDA build on a machine with a real CUDA device.",
  "Session auth + multi-user audit store for solve history.",
];

const jsonExample = `{
  "name": "example",
  "objective": {
    "sense": "maximize",
    "coefficients": [3, 5],
    "constant": 0
  },
  "variables": [
    { "name": "x1", "type": "continuous", "lb": 0, "ub": 4 },
    { "name": "x2", "type": "binary",     "lb": 0, "ub": 1 }
  ],
  "constraints": [
    { "name": "c1", "sense": "<=",
      "coefficients": [1, 2], "rhs": 8 }
  ],
  "config": {
    "time_limit_sec": 30, "mip_rel_gap": 0.001,
    "presolve": true, "verify": true,
    "backend": "auto"
  }
}`;

const resultExample = `{
  "status": "OPTIMAL",
  "problem_class": "LP",
  "objective": 36,
  "feasible_point": true,
  "verified": true,
  "primal_violation": 0,
  "dual_violation": 0,
  "duality_gap": 0,
  "x": [2, 6],
  "duals": [...],
  "reduced_costs": [...],
  "constraint_activities": [...],
  "termination_reason": "...",
  "warnings": [],
  "statistics": {
    "solve_time_sec": 0.0001,
    "iterations": 5, "nodes": 0,
    "rows": 2, "cols": 2, "nonzeros": 4,
    "backend_used": "cpu"
  }
}`;

export default function Documentation() {
  return (
    <div>
      <PageHeader
        eyebrow="Reference"
        title="Documentation"
        description="Model format, API surface, verification guarantees and the backend roadmap behind this UI."
        right={
          <a href="/docs" target="_blank" rel="noreferrer">
            <Badge tone="accent">
              <ExternalLink size={11} /> Open Swagger /docs
            </Badge>
          </a>
        }
      />

      <div className="grid gap-5 lg:grid-cols-2">
        <Panel className="p-6">
          <div className="flex items-center gap-2">
            <GitBranch size={16} className="text-accent" />
            <h3 className="text-base font-bold text-ink">Architecture</h3>
          </div>
          <div className="num mt-4 space-y-2 text-[13px] leading-relaxed text-ink-2">
            <div className="rounded-lg border border-line bg-bg-2/60 px-3 py-2">
              React + TypeScript UI (Vite, Tailwind, Recharts)
            </div>
            <div className="text-center text-accent">↓ HTTP /api/*</div>
            <div className="rounded-lg border border-line bg-bg-2/60 px-3 py-2">
              FastAPI backend (Python) — decode, timeout, error mapping
            </div>
            <div className="text-center text-accent">↓ stdin/stdout JSON</div>
            <div className="rounded-lg border border-line bg-bg-2/60 px-3 py-2">
              solver_cli (C++20) — presolve → simplex / branch &amp; bound → verify → result JSON
            </div>
          </div>
          <p className="mt-4 text-[13px] text-ink-2">
            The solver core is a standalone C++ binary speaking JSON, so any frontend or service
            can drive it. The API adds liveness, validation and timeout handling.
          </p>
        </Panel>

        <Panel className="p-6">
          <div className="flex items-center gap-2">
            <Server size={16} className="text-accent" />
            <h3 className="text-base font-bold text-ink">API endpoints</h3>
          </div>
          <table className="mt-4 w-full text-left text-[13px]">
            <thead>
              <tr className="border-b border-line text-[11px] uppercase tracking-wider text-ink-3">
                <th className="py-2 pr-3 font-semibold">Method</th>
                <th className="py-2 pr-3 font-semibold">Path</th>
                <th className="py-2 font-semibold">Purpose</th>
              </tr>
            </thead>
            <tbody className="num">
              {endpoints.map((e) => (
                <tr key={e.path + e.method} className="border-b border-line/60 text-ink-2">
                  <td className="py-2 pr-3">
                    <span
                      className={
                        e.method === "GET" ? "text-good" : "text-accent"
                      }
                    >
                      {e.method}
                    </span>
                  </td>
                  <td className="py-2 pr-3 text-ink">{e.path}</td>
                  <td className="py-2 font-sans text-ink-3">{e.desc}</td>
                </tr>
              ))}
            </tbody>
          </table>
          <p className="mt-3 text-[12px] text-ink-3">
            Status codes: 422 invalid input · 503 solver binary missing · 504 timeout · 500 crash.
            Solver-level INFEASIBLE/UNBOUNDED returns HTTP 200 with{" "}
            <code className="num">status</code> set accordingly.
          </p>
        </Panel>

        <Panel className="p-6">
          <div className="flex items-center gap-2">
            <BookOpen size={16} className="text-accent" />
            <h3 className="text-base font-bold text-ink">Model format (request)</h3>
          </div>
          <pre className="num mt-4 overflow-auto rounded-xl border border-line bg-bg-2/70 p-4 text-[12px] leading-relaxed text-ink-2">
            {jsonExample}
          </pre>
          <ul className="mt-3 space-y-1 text-[13px] text-ink-2">
            <li>· Variable types: <code className="num">continuous | integer | binary</code></li>
            <li>· Constraint senses: <code className="num">&lt;= | &gt;= | =</code>; omit bounds for ±∞</li>
            <li>· <code className="num">config.backend</code>: <code className="num">auto | cpu | gpu</code> (gpu falls back to CPU when no CUDA build exists)</li>
          </ul>
        </Panel>

        <Panel className="p-6">
          <div className="flex items-center gap-2">
            <BookOpen size={16} className="text-accent" />
            <h3 className="text-base font-bold text-ink">Result format (response)</h3>
          </div>
          <pre className="num mt-4 overflow-auto rounded-xl border border-line bg-bg-2/70 p-4 text-[12px] leading-relaxed text-ink-2">
            {resultExample}
          </pre>
          <p className="mt-3 text-[13px] text-ink-2">
            Statuses: <code className="num">OPTIMAL · FEASIBLE · INFEASIBLE · UNBOUNDED ·
            TIME_LIMIT · ITERATION_LIMIT · NUMERICAL_ERROR · ERROR</code>. Every solve is
            re-verified on the original model before <code className="num">OPTIMAL</code> is
            reported.
          </p>
        </Panel>

        <Panel className="p-6 lg:col-span-2">
          <div className="flex items-center gap-2">
            <GitBranch size={16} className="text-accent-2" />
            <h3 className="text-base font-bold text-ink">Backend roadmap (capabilities not yet live)</h3>
            <Badge tone="warn">Pending</Badge>
          </div>
          <ul className="mt-4 grid gap-2 text-[13px] text-ink-2 sm:grid-cols-2">
            {roadmap.map((r, i) => (
              <li key={i} className="flex gap-2 rounded-lg border border-line bg-bg-2/50 p-3">
                <span className="num text-warn">{String(i + 1).padStart(2, "0")}</span>
                <span>{r}</span>
              </li>
            ))}
          </ul>
        </Panel>
      </div>

      <div className="mt-5 flex flex-wrap gap-3">
        <a href="/docs" target="_blank" rel="noreferrer">
          <button className="focus-ring inline-flex items-center gap-2 rounded-xl bg-accent px-5 py-2.5 text-sm font-semibold text-white transition-colors hover:bg-[#0d9488]">
            Open Swagger UI <ExternalLink size={15} />
          </button>
        </a>
        <Link
          to="/status"
          className="focus-ring inline-flex items-center gap-2 rounded-xl border border-line-2 bg-panel-2 px-5 py-2.5 text-sm font-semibold text-ink transition-colors hover:border-accent/50 hover:text-accent"
        >
          System status
        </Link>
      </div>
    </div>
  );
}
