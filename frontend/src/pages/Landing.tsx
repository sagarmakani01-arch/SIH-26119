import { Link } from "react-router-dom";
import {
  ArrowRight,
  BarChart3,
  CheckCircle2,
  Cpu,
  Database,
  FileJson,
  Gauge,
  Layers,
  Lock,
  Play,
  Server,
  ShieldCheck,
  Sparkles,
} from "lucide-react";
import { PipelineViz } from "../components/PipelineViz";
import { Badge, Button, Panel } from "../components/ui";
import { useBackend } from "../services/BackendContext";

const capabilities = [
  {
    icon: Layers,
    title: "Linear Programming",
    body: "Revised simplex with primal/dual feasibility checks, presolve and independent KKT verification of every reported optimum.",
    tag: "LP",
  },
  {
    icon: ShieldCheck,
    title: "Mixed-Integer Programming",
    body: "Branch & bound with most-fractional branching, best-bound search, rounding heuristics and true MIP-gap termination.",
    tag: "MILP",
  },
  {
    icon: Cpu,
    title: "CPU Active · GPU-Ready",
    body: "The C++20 core runs on CPU today. The CUDA backend is compiled in only when a GPU toolchain is present — never faked.",
    tag: "ENGINE",
  },
  {
    icon: FileJson,
    title: "Open Model Format",
    body: "Every model and result is plain JSON with full statistics — auditable, diffable and portable across the whole stack.",
    tag: "JSON",
  },
  {
    icon: Gauge,
    title: "Verified Results",
    body: "Solutions are re-checked in the original problem space before being called optimal. Violations are reported, not hidden.",
    tag: "AUDIT",
  },
  {
    icon: Lock,
    title: "Indigenous Stack",
    body: "Solver core, API and UI built end-to-end for sovereign industrial workloads — no black-box optimization dependency.",
    tag: "SIH",
  },
];

const workflow = [
  { step: "01", title: "Capture", body: "Pick a guided template or paste expert JSON." },
  { step: "02", title: "Review", body: "Inspect variables, constraints and validation issues." },
  { step: "03", title: "Solve", body: "Send to the C++ core and watch real elapsed time." },
  { step: "04", title: "Interpret", body: "Charts, statistics and a plain-language explanation." },
  { step: "05", title: "Explore", body: "Branch into what-if scenarios and compare objectives." },
];

export default function Landing() {
  const { state } = useBackend();
  const solverAvailable = state.kind === "up" && state.health.solver_cli.available;

  return (
    <div className="space-y-16 pb-6">
      <section className="pt-6 sm:pt-10">
        <div className="flex flex-col items-start gap-6 lg:flex-row lg:items-center lg:gap-12">
          <div className="flex-1 animate-rise">
            <div className="flex flex-wrap items-center gap-2">
              <Badge tone="accent">Industrial Optimization</Badge>
              <Badge tone="violet">SIH 2026 · PS 26119</Badge>
              <Badge tone={solverAvailable ? "good" : "neutral"}>
                {solverAvailable ? "Solver Core Online" : "Solver Core Status Unknown"}
              </Badge>
            </div>
            <h1 className="mt-5 text-4xl font-extrabold leading-[1.05] tracking-tight text-ink sm:text-5xl lg:text-[3.4rem]">
              Optimize critical decisions,{" "}
              <span className="bg-gradient-to-r from-accent to-accent-2 bg-clip-text text-transparent">
                compute optimal solutions.
              </span>
            </h1>
            <p className="mt-5 max-w-xl text-[15px] leading-relaxed text-ink-2">
              An indigenous GPU-accelerated optimization engine for linear, mixed-integer and
              quadratic models — from refinery blend planning to shift scheduling — with a verified
              C++ core, an open JSON model format and a full audit trail behind every solve.
            </p>
            <div className="mt-7 flex flex-wrap gap-3">
              <Link to="/optimizer">
                <Button>
                  <Play size={15} fill="currentColor" />
                  Start Optimizing
                </Button>
              </Link>
              <Link to="/optimizer/guided/refinery">
                <Button variant="secondary">
                  Explore Refinery Demo
                  <ArrowRight size={15} />
                </Button>
              </Link>
            </div>
            <div className="mt-6 flex flex-wrap items-center gap-x-5 gap-y-2 text-xs text-ink-3">
              <span className="flex items-center gap-1.5">
                <CheckCircle2 size={13} className="text-good" /> Verified primal & dual
              </span>
              <span className="flex items-center gap-1.5">
                <CheckCircle2 size={13} className="text-good" /> LP · MILP active
              </span>
              <span className="flex items-center gap-1.5">
                <Sparkles size={13} className="text-warn" /> QP on roadmap
              </span>
            </div>
          </div>

          <div className="w-full flex-1 animate-rise">
            <PipelineViz />
            <div className="mt-4 grid grid-cols-3 gap-3">
              <Panel className="px-4 py-3">
                <div className="label-cap">Backend</div>
                <div className="mt-1 flex items-center gap-2 text-sm font-semibold text-ink">
                  <Server size={14} className="text-accent" /> FastAPI
                </div>
              </Panel>
              <Panel className="px-4 py-3">
                <div className="label-cap">Compute</div>
                <div className="mt-1 flex items-center gap-2 text-sm font-semibold text-ink">
                  <Cpu size={14} className="text-accent" /> C++20 Core
                </div>
              </Panel>
              <Panel className="px-4 py-3">
                <div className="label-cap">Data</div>
                <div className="mt-1 flex items-center gap-2 text-sm font-semibold text-ink">
                  <Database size={14} className="text-accent" /> JSON
                </div>
              </Panel>
            </div>
          </div>
        </div>
      </section>

      <section>
        <div className="mb-6 flex items-end justify-between gap-4">
          <div>
            <div className="label-cap text-accent">Capabilities</div>
            <h2 className="mt-1 text-2xl font-bold text-ink">A solver you can inspect</h2>
          </div>
          <Link to="/documentation" className="hidden text-sm text-accent hover:underline sm:block">
            Read the architecture docs →
          </Link>
        </div>
        <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {capabilities.map((c) => (
            <Panel key={c.title} className="group p-5 transition-colors hover:border-line-2">
              <div className="flex items-start justify-between">
                <span className="grid h-10 w-10 place-items-center rounded-xl border border-line-2 bg-panel-2 text-accent transition-colors group-hover:border-accent/40">
                  <c.icon size={18} />
                </span>
                <Badge tone="neutral">{c.tag}</Badge>
              </div>
              <h3 className="mt-4 text-[15px] font-semibold text-ink">{c.title}</h3>
              <p className="mt-1.5 text-[13px] leading-relaxed text-ink-2">{c.body}</p>
            </Panel>
          ))}
        </div>
      </section>

      <section>
        <div className="mb-6">
          <div className="label-cap text-accent">Workflow</div>
          <h2 className="mt-1 text-2xl font-bold text-ink">One continuous path to a decision</h2>
        </div>
        <div className="grid gap-3 sm:grid-cols-2 lg:grid-cols-5">
          {workflow.map((w) => (
            <Panel key={w.step} className="p-4">
              <div className="num text-xs font-bold text-accent">{w.step}</div>
              <div className="mt-2 text-sm font-semibold text-ink">{w.title}</div>
              <div className="mt-1 text-xs leading-relaxed text-ink-2">{w.body}</div>
            </Panel>
          ))}
        </div>
        <div className="mt-6 flex justify-center">
          <Link to="/optimizer">
            <Button variant="secondary">
              Open the Optimizer <ArrowRight size={15} />
            </Button>
          </Link>
        </div>
      </section>

      <section>
        <div className="mb-6">
          <div className="label-cap text-accent">Honest Scope</div>
          <h2 className="mt-1 text-2xl font-bold text-ink">What this build does — and does not</h2>
        </div>
        <div className="grid gap-4 lg:grid-cols-3">
          <Panel className="p-5">
            <div className="flex items-center gap-2 text-good">
              <CheckCircle2 size={16} />
              <span className="text-sm font-bold uppercase tracking-wider">Live today</span>
            </div>
            <ul className="mt-3 space-y-2 text-[13px] text-ink-2">
              <li>· LP & MILP solves through the C++ core with verified results</li>
              <li>· Real benchmark timings measured in your browser session</li>
              <li>· Model validation with structural statistics</li>
              <li>· Swagger API at <code className="num text-accent">/docs</code></li>
            </ul>
          </Panel>
          <Panel className="p-5">
            <div className="flex items-center gap-2 text-warn">
              <Sparkles size={16} />
              <span className="text-sm font-bold uppercase tracking-wider">Roadmap</span>
            </div>
            <ul className="mt-3 space-y-2 text-[13px] text-ink-2">
              <li>· Quadratic programming (QP) models</li>
              <li>· AI natural-language model generation endpoint</li>
              <li>· Persistent scenario database & async job queue</li>
            </ul>
          </Panel>
          <Panel className="p-5">
            <div className="flex items-center gap-2 text-bad">
              <Lock size={16} />
              <span className="text-sm font-bold uppercase tracking-wider">Never faked</span>
            </div>
            <ul className="mt-3 space-y-2 text-[13px] text-ink-2">
              <li>· GPU speedups without a detected GPU</li>
              <li>· Benchmark numbers that were not measured</li>
              <li>· MRPL plant data — all templates are labeled synthetic</li>
            </ul>
          </Panel>
        </div>
        <div className="mt-4 flex items-center gap-2">
          <BarChart3 size={14} className="text-ink-3" />
          <Link to="/benchmarks" className="text-sm text-accent hover:underline">
            See measured CPU benchmark results
          </Link>
        </div>
      </section>
    </div>
  );
}
