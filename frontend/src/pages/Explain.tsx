import { Link, useNavigate } from "react-router-dom";
import {
  ArrowRight,
  BrainCircuit,
  CheckCircle2,
  Database,
  Info,
  Lightbulb,
  Workflow,
} from "lucide-react";
import { Badge, Banner, Button, PageHeader, Panel } from "../components/ui";
import { useWorkspace } from "../workspace/WorkspaceContext";
import { formatNumber } from "../lib/format";

export default function Explain() {
  const { workspace } = useWorkspace();
  const navigate = useNavigate();
  const model = workspace.model!;
  const result = workspace.result!;

  const facts: string[] = [];
  const interpretation: string[] = [];

  facts.push(
    `Solver status is ${result.status} (termination: ${result.termination_reason}); problem class ${result.problem_class}.`,
  );
  facts.push(
    `Objective (${model.objective.sense}) = ${formatNumber(result.objective, 4)} ${workspace.objectiveUnit}.`,
  );
  facts.push(
    `Independent verification: ${result.verified ? "passed" : "not passed"} · primal violation ${formatNumber(result.primal_violation, 8)} · dual violation ${formatNumber(result.dual_violation, 8)}.`,
  );
  facts.push(
    `Measured compute: ${formatNumber(result.statistics.solve_time_sec, 4)} s solve + ${formatNumber(result.statistics.presolve_time_sec, 4)} s presolve, ${result.statistics.iterations} iterations${result.statistics.nodes > 0 ? `, ${result.statistics.nodes} branch-&-bound nodes` : ""}, backend ${result.statistics.backend_used}.`,
  );

  model.constraints.forEach((c, i) => {
    const activity = result.constraint_activities[i];
    if (activity === undefined) return;
    const slack = c.sense === "<=" ? c.rhs - activity : c.sense === ">=" ? activity - c.rhs : Math.abs(c.rhs - activity);
    if (Math.abs(slack) < 1e-6) {
      facts.push(`Constraint "${c.name ?? `c${i + 1}`}" is binding: activity ${formatNumber(activity, 4)} exactly meets its RHS ${c.rhs}.`);
    }
  });

  model.variables.forEach((v, i) => {
    const value = result.x[i];
    if (value === undefined) return;
    if (v.ub !== null && v.ub !== undefined && Math.abs(value - v.ub) < 1e-6) {
      facts.push(`Variable "${v.name}" sits at its upper bound ${v.ub}.`);
    }
  });

  const bindingCount = model.constraints.filter((c, i) => {
    const activity = result.constraint_activities[i];
    if (activity === undefined) return false;
    const slack = c.sense === "<=" ? c.rhs - activity : c.sense === ">=" ? activity - c.rhs : Math.abs(c.rhs - activity);
    return Math.abs(slack) < 1e-6;
  }).length;

  const saturated = model.variables.filter((v, i) => {
    const value = result.x[i];
    if (value === undefined) return false;
    return (
      (v.ub !== null && v.ub !== undefined && Math.abs(value - v.ub) < 1e-6) ||
      (v.lb !== null && v.lb !== undefined && Math.abs(value - v.lb) < 1e-6)
    );
  }).length;

  if (result.status === "OPTIMAL") {
    interpretation.push(
      "Because status is OPTIMAL, no other feasible assignment can improve this objective — the reported point is the best possible for these constraints.",
    );
  } else if (result.status === "INFEASIBLE") {
    interpretation.push(
      "Because status is INFEASIBLE, the conflicts are structural: look for demand or capacity figures that together exceed availability — no variable choice can fix them.",
    );
  } else if (result.status === "FEASIBLE" || result.status === "TIME_LIMIT") {
    interpretation.push(
      "Because the solve ended before proving optimality, treat the objective as a lower bound (maximize) or upper bound (minimize) on what is achievable.",
    );
  }

  if (bindingCount > 0) {
    interpretation.push(
      `With ${bindingCount} binding constraint${bindingCount > 1 ? "s" : ""}, the solution is pressing against the limits of the model — relaxing the most relevant of them (shadow price λ reported on the Constraints tab) would move the objective.`,
    );
  }
  if (saturated > 0) {
    interpretation.push(
      `${saturated} variable${saturated > 1 ? "s are" : " is"} pinned at a bound, which usually means the bound — not the objective — is the deciding factor for that decision.`,
    );
  }
  if (result.duality_gap !== null && Math.abs(result.duality_gap) < 1e-6) {
    interpretation.push(
      "The duality gap is numerically zero, which is the classic signature of a consistent primal/dual pair at an LP optimum.",
    );
  }

  return (
    <div>
      <PageHeader
        eyebrow="Step 5 — Interpretation"
        title="Explain This Solution"
        description="Facts are read directly from the solver output. Interpretations are heuristic statements derived from those facts — no invented numbers."
        right={
          <Button variant="ghost" onClick={() => navigate("/optimizer/results")}>
            <ArrowRight size={14} /> Back to results
          </Button>
        }
      />

      <div className="grid gap-5 lg:grid-cols-2">
        <Panel className="p-6">
          <div className="flex items-center gap-2">
            <Database size={16} className="text-accent" />
            <h3 className="text-base font-bold text-ink">Facts</h3>
            <Badge tone="good">From solver output</Badge>
          </div>
          <ul className="mt-4 space-y-3">
            {facts.map((f, i) => (
              <li key={i} className="flex gap-3 text-[13px] leading-relaxed text-ink-2">
                <CheckCircle2 size={15} className="mt-0.5 shrink-0 text-good" />
                <span className="num">{f}</span>
              </li>
            ))}
          </ul>
        </Panel>

        <Panel className="p-6">
          <div className="flex items-center gap-2">
            <Lightbulb size={16} className="text-accent-2" />
            <h3 className="text-base font-bold text-ink">Interpretation</h3>
            <Badge tone="violet">Heuristic</Badge>
          </div>
          <ul className="mt-4 space-y-3">
            {interpretation.map((s, i) => (
              <li key={i} className="flex gap-3 text-[13px] leading-relaxed text-ink-2">
                <Info size={15} className="mt-0.5 shrink-0 text-[#9333ea]" />
                <span>{s}</span>
              </li>
            ))}
          </ul>
          <div className="mt-5 rounded-xl border border-line bg-bg-2/60 p-4">
            <div className="flex items-start gap-2 text-[12px] text-ink-3">
              <BrainCircuit size={15} className="mt-0.5 shrink-0 text-ink-3" />
              <span>
                AI-assisted narrative generation is a documented roadmap item — these statements
                are deterministic rules over the solver output, not a language model.
              </span>
            </div>
          </div>
        </Panel>

        {result.warnings.length > 0 && (
          <div className="lg:col-span-2">
            <Banner tone="warn" title="Solver warnings">
              <ul className="mt-1 list-inside list-disc space-y-0.5">
                {result.warnings.map((w, i) => (
                  <li key={i}>{w}</li>
                ))}
              </ul>
            </Banner>
          </div>
        )}
      </div>

      <div className="mt-6 flex flex-wrap gap-3">
        <Link to="/scenarios">
          <Button>
            <Workflow size={15} /> Branch into scenarios
            <ArrowRight size={15} />
          </Button>
        </Link>
        <Button variant="secondary" onClick={() => navigate("/optimizer")}>
          New optimization
        </Button>
      </div>
    </div>
  );
}
