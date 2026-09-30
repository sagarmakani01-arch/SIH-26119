import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import {
  ArrowRight,
  AlertTriangle,
  BookOpenCheck,
  CheckCircle2,
  Clock,
  Cpu,
  Layers,
  MessageSquareText,
  RefreshCw,
  Workflow,
} from "lucide-react";
import {
  Badge,
  Banner,
  Button,
  PageHeader,
  Panel,
  Stat,
} from "../components/ui";
import { AllocationChart, UtilizationChart } from "../components/charts";
import { useWorkspace } from "../workspace/WorkspaceContext";
import { formatNumber, formatSeconds, statusTone } from "../lib/format";
import type { SolveStatus } from "../services/api/types";

const tabs = ["Overview", "Variables", "Constraints", "Statistics"] as const;
type Tab = (typeof tabs)[number];

export default function Results() {
  const { workspace } = useWorkspace();
  const navigate = useNavigate();
  const model = workspace.model!;
  const result = workspace.result!;
  const [tab, setTab] = useState<Tab>("Overview");

  const tone = statusTone(result.status);
  const statusCopy: Record<SolveStatus, { title: string; body: string; tone: "good" | "warn" | "bad" | "info" }> = {
    OPTIMAL: {
      title: "Optimal solution found",
      body: "The solver proved this is the best achievable objective value for this model.",
      tone: "good",
    },
    FEASIBLE: {
      title: "Feasible solution (optimality not proven)",
      body: "A valid solution was found, but the solver stopped before proving it is optimal. Check termination reason and gap.",
      tone: "warn",
    },
    INFEASIBLE: {
      title: "No feasible solution exists",
      body: "The constraints cannot be satisfied simultaneously. Relax demands, capacity or bounds, then solve again.",
      tone: "bad",
    },
    UNBOUNDED: {
      title: "Objective is unbounded",
      body: "The objective can improve without limit — a bound or constraint is missing.",
      tone: "bad",
    },
    TIME_LIMIT: {
      title: "Stopped at the time limit",
      body: "The solver hit its configured time limit. The point below is the best known at that moment.",
      tone: "warn",
    },
    ITERATION_LIMIT: {
      title: "Stopped at the iteration limit",
      body: "The solver hit max_iterations before finishing. Results are the best known at that moment.",
      tone: "warn",
    },
    NUMERICAL_ERROR: {
      title: "Numerical error",
      body: "The solver could not trust its arithmetic. Rescale coefficients or tighten tolerances.",
      tone: "bad",
    },
    ERROR: {
      title: "Solver reported an error",
      body: "See termination reason and warnings below.",
      tone: "bad",
    },
    NOT_SOLVED: {
      title: "Not solved",
      body: "The solver did not produce a result.",
      tone: "bad",
    },
  };
  const copy = statusCopy[result.status];

  const hasPoint = result.x.length > 0;
  const optimalLike = result.status === "OPTIMAL";

  function statusBadge(s: SolveStatus) {
    const t = statusTone(s);
    return (
      <Badge tone={t === "neutral" ? "neutral" : t}>
        <span
          className={`h-1.5 w-1.5 rounded-full ${
            t === "good" ? "bg-good" : t === "warn" ? "bg-warn" : t === "bad" ? "bg-bad" : "bg-ink-3"
          }`}
        />
        {s}
      </Badge>
    );
  }

  return (
    <div>
      <PageHeader
        eyebrow="Step 4 — Results"
        title="Optimization Results"
        description={`${model.name ?? "Model"} · solved by the C++ core with independent verification.`}
        right={
          <div className="flex flex-wrap items-center gap-2">
            {statusBadge(result.status)}
            <Button variant="ghost" onClick={() => navigate("/optimizer/review")}>
              <RefreshCw size={14} /> Re-solve
            </Button>
          </div>
        }
      />

      <div className={`mb-5 rounded-2xl border p-5 ${
        copy.tone === "good"
          ? "border-good/40 bg-good/8"
          : copy.tone === "warn"
            ? "border-warn/40 bg-warn/8"
            : "border-bad/40 bg-bad/8"
      }`}>
        <div className="flex flex-wrap items-start justify-between gap-3">
          <div className="flex items-start gap-3">
            {copy.tone === "good" ? (
              <CheckCircle2 size={20} className="mt-0.5 text-good" />
            ) : (
              <AlertTriangle size={20} className="mt-0.5 text-warn" />
            )}
            <div>
              <h2 className="text-lg font-bold text-ink">{copy.title}</h2>
              <p className="mt-0.5 max-w-2xl text-[13px] text-ink-2">{copy.body}</p>
              <p className="num mt-1 text-[11px] text-ink-3">
                termination: {result.termination_reason} · problem class: {result.problem_class}
              </p>
            </div>
          </div>
          <div className="text-right">
            <div className="label-cap">Objective</div>
            <div className="num text-3xl font-extrabold text-ink">
              {formatNumber(result.objective, 2)}
            </div>
            <div className="text-[11px] text-ink-3">
              {model.objective.sense === "maximize" ? "maximize" : "minimize"}{" "}
              {workspace.objectiveUnit}
            </div>
          </div>
        </div>
      </div>

      {!result.verified && optimalLike && (
        <div className="mb-5">
          <Banner tone="warn" title="Verification caveat">
            The solver reported OPTIMAL but the independent check did not fully pass — review the
            violation metrics before relying on this solution.
          </Banner>
        </div>
      )}

      <div className="mb-5 grid grid-cols-2 gap-3 md:grid-cols-4 xl:grid-cols-6">
        <Stat label="Status" value={result.status} tone={tone === "neutral" ? "neutral" : tone} />
        <Stat
          label="Solve time"
          value={formatSeconds(result.statistics.solve_time_sec)}
          tone="accent"
        />
        <Stat
          label="Presolve"
          value={formatSeconds(result.statistics.presolve_time_sec)}
        />
        <Stat
          label={result.problem_class === "MILP" ? "B&B nodes" : "Iterations"}
          value={result.problem_class === "MILP" ? result.statistics.nodes : result.statistics.iterations}
        />
        <Stat
          label="Verified"
          value={result.verified ? "Yes" : "No"}
          tone={result.verified ? "good" : "bad"}
        />
        <Stat
          label="Duality gap"
          value={result.duality_gap === null ? "—" : formatNumber(result.duality_gap, 6)}
          tone={result.duality_gap === null ? "neutral" : Math.abs(result.duality_gap) < 1e-6 ? "good" : "warn"}
        />
      </div>

      <div className="mb-4 flex gap-1 border-b border-line">
        {tabs.map((t) => (
          <button
            key={t}
            onClick={() => setTab(t)}
            className={`focus-ring rounded-t-lg px-4 py-2.5 text-[13px] font-semibold transition-colors ${
              tab === t
                ? "border-b-2 border-accent text-accent"
                : "text-ink-3 hover:text-ink-2"
            }`}
          >
            {t}
          </button>
        ))}
      </div>

      {tab === "Overview" && (
        <div className="grid gap-5 lg:grid-cols-2">
          <Panel className="p-5">
            <div className="flex items-center gap-2">
              <Layers size={15} className="text-accent" />
              <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                Allocation — decision variables
              </h3>
            </div>
            {hasPoint ? (
              <div className="mt-3">
                <AllocationChart model={model} result={result} />
              </div>
            ) : (
              <p className="mt-4 text-sm text-ink-3">
                No solution vector was produced for this status.
              </p>
            )}
          </Panel>
          <Panel className="p-5">
            <div className="flex items-center gap-2">
              <Cpu size={15} className="text-accent" />
              <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                Constraint utilization — activity vs RHS
              </h3>
            </div>
            {result.constraint_activities.length > 0 ? (
              <div className="mt-3">
                <UtilizationChart model={model} result={result} />
              </div>
            ) : (
              <p className="mt-4 text-sm text-ink-3">No constraint activities reported.</p>
            )}
          </Panel>

          <div className="lg:col-span-2 grid gap-3 sm:grid-cols-3">
            <div className="panel-soft px-4 py-3">
              <div className="label-cap">Primal violation</div>
              <div className="num mt-1 text-sm font-semibold text-ink">
                {formatNumber(result.primal_violation, 8)}
              </div>
            </div>
            <div className="panel-soft px-4 py-3">
              <div className="label-cap">Dual violation</div>
              <div className="num mt-1 text-sm font-semibold text-ink">
                {formatNumber(result.dual_violation, 8)}
              </div>
            </div>
            <div className="panel-soft px-4 py-3">
              <div className="label-cap">Backend</div>
              <div className="num mt-1 text-sm font-semibold text-ink">
                {result.statistics.backend_used}
              </div>
            </div>
          </div>

          {result.warnings.length > 0 && (
            <div className="lg:col-span-2">
              <Banner tone="warn" title={`Solver warnings (${result.warnings.length})`}>
                <ul className="mt-1 list-inside list-disc space-y-0.5">
                  {result.warnings.map((w, i) => (
                    <li key={i}>{w}</li>
                  ))}
                </ul>
              </Banner>
            </div>
          )}
        </div>
      )}

      {tab === "Variables" && (
        <Panel className="overflow-x-auto p-5">
          <table className="w-full text-left text-[13px]">
            <thead>
              <tr className="border-b border-line text-[11px] uppercase tracking-wider text-ink-3">
                <th className="py-2 pr-4 font-semibold">#</th>
                <th className="py-2 pr-4 font-semibold">Variable</th>
                <th className="py-2 pr-4 font-semibold">Type</th>
                <th className="py-2 pr-4 font-semibold">Value</th>
                <th className="py-2 pr-4 font-semibold">Bounds</th>
                <th className="py-2 pr-4 font-semibold">Reduced cost</th>
                <th className="py-2 font-semibold">State</th>
              </tr>
            </thead>
            <tbody className="num">
              {model.variables.map((v, i) => {
                const value = result.x[i];
                const atLower = v.lb !== null && v.lb !== undefined && value !== undefined && Math.abs(value - v.lb) < 1e-6;
                const atUpper = v.ub !== null && v.ub !== undefined && value !== undefined && Math.abs(value - v.ub) < 1e-6;
                return (
                  <tr key={i} className="border-b border-line/60 text-ink-2">
                    <td className="py-2 pr-4 text-ink-3">{i + 1}</td>
                    <td className="py-2 pr-4 text-ink">{v.name}</td>
                    <td className="py-2 pr-4 text-ink-3">{v.type ?? "continuous"}</td>
                    <td className="py-2 pr-4 font-semibold text-ink">
                      {value === undefined ? "—" : formatNumber(value, 4)}
                    </td>
                    <td className="py-2 pr-4">
                      [{v.lb ?? "-∞"}, {v.ub ?? "+∞"}]
                    </td>
                    <td className="py-2 pr-4">
                      {result.reduced_costs[i] === undefined
                        ? "—"
                        : formatNumber(result.reduced_costs[i], 4)}
                    </td>
                    <td className="py-2">
                      {atUpper ? (
                        <Badge tone="good">at upper bound</Badge>
                      ) : atLower ? (
                        <Badge tone="neutral">at lower bound</Badge>
                      ) : (
                        <Badge tone="accent">active</Badge>
                      )}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </Panel>
      )}

      {tab === "Constraints" && (
        <Panel className="overflow-x-auto p-5">
          <table className="w-full text-left text-[13px]">
            <thead>
              <tr className="border-b border-line text-[11px] uppercase tracking-wider text-ink-3">
                <th className="py-2 pr-4 font-semibold">#</th>
                <th className="py-2 pr-4 font-semibold">Constraint</th>
                <th className="py-2 pr-4 font-semibold">Sense</th>
                <th className="py-2 pr-4 font-semibold">Activity</th>
                <th className="py-2 pr-4 font-semibold">RHS</th>
                <th className="py-2 pr-4 font-semibold">Slack / surplus</th>
                <th className="py-2 font-semibold">Dual</th>
              </tr>
            </thead>
            <tbody className="num">
              {model.constraints.map((c, i) => {
                const activity = result.constraint_activities[i];
                const dual = result.duals[i];
                const slack =
                  activity === undefined
                    ? undefined
                    : c.sense === "<="
                      ? c.rhs - activity
                      : c.sense === ">="
                        ? activity - c.rhs
                        : Math.abs(c.rhs - activity);
                const binding =
                  slack !== undefined && Math.abs(slack) < 1e-6;
                return (
                  <tr key={i} className="border-b border-line/60 text-ink-2">
                    <td className="py-2 pr-4 text-ink-3">{i + 1}</td>
                    <td className="py-2 pr-4 text-ink">{c.name ?? `c${i + 1}`}</td>
                    <td className="py-2 pr-4 text-accent">{c.sense}</td>
                    <td className="py-2 pr-4">
                      {activity === undefined ? "—" : formatNumber(activity, 4)}
                    </td>
                    <td className="py-2 pr-4">{c.rhs}</td>
                    <td className="py-2 pr-4">
                      {slack === undefined ? "—" : formatNumber(slack, 4)}
                    </td>
                    <td className="py-2">
                      {dual === undefined ? (
                        "—"
                      ) : binding ? (
                        <Badge tone="good">binding · λ {formatNumber(dual, 3)}</Badge>
                      ) : (
                        <span className="text-ink-3">λ {formatNumber(dual, 3)}</span>
                      )}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </Panel>
      )}

      {tab === "Statistics" && (
        <div className="grid grid-cols-2 gap-3 md:grid-cols-3 xl:grid-cols-4">
          <Stat label="Solve time" value={formatSeconds(result.statistics.solve_time_sec)} />
          <Stat label="Presolve time" value={formatSeconds(result.statistics.presolve_time_sec)} />
          <Stat label="Verify time" value={formatSeconds(result.statistics.verify_time_sec)} />
          <Stat label="Iterations" value={result.statistics.iterations} />
          <Stat label="Phase-1 iterations" value={result.statistics.phase1_iterations} />
          <Stat label="B&B nodes" value={result.statistics.nodes} />
          <Stat label="Refactorizations" value={result.statistics.refactorizations} />
          <Stat label="Rows" value={result.statistics.rows} />
          <Stat label="Columns" value={result.statistics.cols} />
          <Stat label="Nonzeros" value={result.statistics.nonzeros} />
          <Stat label="Rows removed" value={result.statistics.presolve_rows_removed} />
          <Stat label="Bounds tightened" value={result.statistics.presolve_bounds_tightened} />
          <Stat label="Fixed vars removed" value={result.statistics.presolve_fixed_variables_removed} />
          <Stat label="Backend used" value={result.statistics.backend_used} tone="accent" />
        </div>
      )}

      <div className="mt-6 flex flex-wrap gap-3">
        <Button onClick={() => navigate("/optimizer/explain")}>
          <MessageSquareText size={15} /> Explain This Solution
        </Button>
        <Link to="/scenarios">
          <Button variant="secondary">
            <Workflow size={15} /> What-If Scenarios
            <ArrowRight size={15} />
          </Button>
        </Link>
        <Link to="/optimizer">
          <Button variant="ghost">New Optimization</Button>
        </Link>
        <Link to="/documentation" className="ml-auto">
          <Button variant="ghost">
            <BookOpenCheck size={15} /> How results are verified
          </Button>
        </Link>
      </div>

      <div className="mt-4 flex items-center gap-4 text-[11px] text-ink-3">
        <span className="flex items-center gap-1.5">
          <Clock size={11} /> timing is measured wall-clock
        </span>
        <span className="flex items-center gap-1.5">
          <CheckCircle2 size={11} /> violations reported above are computed on the original model
        </span>
      </div>
    </div>
  );
}
