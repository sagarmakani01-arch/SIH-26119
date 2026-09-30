import { useMemo, useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import {
  ArrowRight,
  GitBranch,
  Play,
  Plus,
  Trash2,
  Workflow,
} from "lucide-react";
import {
  Badge,
  Banner,
  Button,
  EmptyState,
  Field,
  NumberInput,
  PageHeader,
  Panel,
  Spinner,
} from "../components/ui";
import { ScenarioChart } from "../components/charts";
import { solveModel } from "../services/api/solver";
import { getTemplate } from "../templates";
import { useWorkspace } from "../workspace/WorkspaceContext";
import { formatNumber, statusTone } from "../lib/format";
import type { SolveResult } from "../services/api/types";

export default function Scenarios() {
  const { workspace, addScenario, removeScenario, setResult } = useWorkspace();
  const navigate = useNavigate();
  const template = getTemplate(workspace.templateId);
  const [name, setName] = useState("");
  const [inputs, setInputs] = useState<Record<string, number | string> | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const activeInputs = inputs ?? workspace.inputs ?? {};

  const chartData = useMemo(() => {
    const points: { name: string; objective: number }[] = [];
    if (workspace.result) {
      points.push({ name: "Baseline", objective: workspace.result.objective });
    }
    workspace.scenarios.forEach((s) => {
      if (s.result) points.push({ name: s.name, objective: s.result.objective });
    });
    return points;
  }, [workspace.result, workspace.scenarios]);

  if (!workspace.model) {
    return (
      <div>
        <PageHeader
          eyebrow="What-if analysis"
          title="Scenarios"
          description="Compare alternative futures against a solved baseline."
        />
        <EmptyState
          icon={<GitBranch size={24} />}
          title="No baseline model yet"
          body="Run an optimization first — the solved baseline becomes the anchor that every scenario is compared against."
          action={
            <Link to="/optimizer">
              <Button>
                Start an optimization <ArrowRight size={15} />
              </Button>
            </Link>
          }
        />
      </div>
    );
  }

  async function runScenario() {
    if (!template) return;
    setBusy(true);
    setError(null);
    try {
      const built = template.build(activeInputs);
      const result = await solveModel(built.model);
      addScenario({
        name: name.trim() || `Scenario ${workspace.scenarios.length + 1}`,
        templateId: template.id,
        inputs: activeInputs,
        result,
        objectiveUnit: template.objectiveUnit,
      });
      setName("");
      setInputs(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(false);
    }
  }

  async function rerunAll() {
    if (!template) return;
    setBusy(true);
    setError(null);
    try {
      for (const scenario of [...workspace.scenarios]) {
        const built = template.build(scenario.inputs ?? {});
        const result = await solveModel(built.model);
        addScenario({
          name: scenario.name,
          templateId: template.id,
          inputs: scenario.inputs,
          result,
          objectiveUnit: template.objectiveUnit,
        });
        removeScenario(scenario.id);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(false);
    }
  }

  function statusBadge(result: SolveResult) {
    const tone = statusTone(result.status);
    return <Badge tone={tone === "neutral" ? "neutral" : tone}>{result.status}</Badge>;
  }

  return (
    <div>
      <PageHeader
        eyebrow="What-if analysis"
        title="Scenarios"
        description={`Baseline: ${workspace.label || "custom model"} — branch, solve and compare objectives.`}
        right={
          <Badge tone="violet">
            <Workflow size={11} /> {workspace.scenarios.length} scenario
            {workspace.scenarios.length === 1 ? "" : "s"}
          </Badge>
        }
      />

      {!workspace.result && (
        <div className="mb-5">
          <Banner tone="warn" title="Baseline not solved yet">
            Open the{" "}
            <Link to="/optimizer/results" className="underline">
              results screen
            </Link>{" "}
            for the baseline — scenarios will show a side-by-side comparison once it exists.
          </Banner>
        </div>
      )}

      <div className="grid gap-5 lg:grid-cols-3">
        <div className="space-y-4 lg:col-span-2">
          <Panel className="p-5">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <GitBranch size={15} className="text-accent" />
                <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                  Objective comparison
                </h3>
              </div>
              <span className="text-[11px] text-ink-3">
                {workspace.model.objective.sense} · {workspace.objectiveUnit}
              </span>
            </div>
            {chartData.length > 1 ? (
              <div className="mt-4">
                <ScenarioChart
                  data={chartData}
                  sense={workspace.model.objective.sense}
                />
              </div>
            ) : (
              <div className="mt-4 rounded-xl border border-dashed border-line-2 bg-bg-2/50 px-4 py-8 text-center text-sm text-ink-3">
                Save at least one scenario to see the comparison chart.
              </div>
            )}
          </Panel>

          <Panel className="overflow-x-auto p-5">
            <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">Scenario list</h3>
            <table className="mt-3 w-full text-left text-[13px]">
              <thead>
                <tr className="border-b border-line text-[11px] uppercase tracking-wider text-ink-3">
                  <th className="py-2 pr-4 font-semibold">Name</th>
                  <th className="py-2 pr-4 font-semibold">Status</th>
                  <th className="py-2 pr-4 font-semibold">Objective</th>
                  <th className="py-2 pr-4 font-semibold">Δ vs baseline</th>
                  <th className="py-2 pr-4 font-semibold">Solve time</th>
                  <th className="py-2 font-semibold" />
                </tr>
              </thead>
              <tbody className="num">
                {workspace.result && (
                  <tr className="border-b border-accent/30 bg-accent/5 text-ink">
                    <td className="py-2.5 pr-4 font-semibold text-accent">Baseline</td>
                    <td className="py-2.5 pr-4">{statusBadge(workspace.result)}</td>
                    <td className="py-2.5 pr-4 font-semibold">
                      {formatNumber(workspace.result.objective, 2)}
                    </td>
                    <td className="py-2.5 pr-4 text-ink-3">—</td>
                    <td className="py-2.5 pr-4">
                      {formatNumber(workspace.result.statistics.solve_time_sec, 4)} s
                    </td>
                    <td />
                  </tr>
                )}
                {workspace.scenarios.map((s) => {
                  const delta =
                    workspace.result && s.result
                      ? s.result.objective - workspace.result.objective
                      : null;
                  return (
                    <tr key={s.id} className="border-b border-line/60 text-ink-2">
                      <td className="py-2.5 pr-4 text-ink">{s.name}</td>
                      <td className="py-2.5 pr-4">
                        {s.result ? statusBadge(s.result) : <span className="text-ink-3">—</span>}
                      </td>
                      <td className="py-2.5 pr-4">
                        {s.result ? formatNumber(s.result.objective, 2) : "—"}
                      </td>
                      <td
                        className={`py-2.5 pr-4 ${
                          delta === null
                            ? "text-ink-3"
                            : delta > 0
                              ? "text-good"
                              : delta < 0
                                ? "text-bad"
                                : "text-ink-3"
                        }`}
                      >
                        {delta === null ? "—" : `${delta > 0 ? "+" : ""}${formatNumber(delta, 2)}`}
                      </td>
                      <td className="py-2.5 pr-4">
                        {s.result
                          ? `${formatNumber(s.result.statistics.solve_time_sec, 4)} s`
                          : "—"}
                      </td>
                      <td className="py-2.5 text-right">
                        <button
                          onClick={() => removeScenario(s.id)}
                          className="focus-ring rounded-lg p-1.5 text-ink-3 transition-colors hover:bg-bad/10 hover:text-bad"
                          title="Delete scenario"
                        >
                          <Trash2 size={14} />
                        </button>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
            {workspace.scenarios.length === 0 && (
              <p className="mt-3 text-xs text-ink-3">No scenarios saved yet.</p>
            )}
          </Panel>
        </div>

        <div className="space-y-4">
          <Panel className="p-5">
            <div className="flex items-center gap-2">
              <Plus size={15} className="text-accent" />
              <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                New scenario
              </h3>
            </div>

            {template ? (
              <>
                <p className="mt-1.5 text-xs text-ink-3">
                  Change inputs from “{template.name}” — the model is rebuilt and solved against the
                  same formulation.
                </p>
                <div className="mt-3">
                  <Field label="Scenario name">
                    <input
                      className="focus-ring w-full rounded-lg border border-line-2 bg-bg-2/80 px-3 py-2 text-sm text-ink placeholder:text-ink-3"
                      placeholder={`Scenario ${workspace.scenarios.length + 1}`}
                      value={name}
                      onChange={(e) => setName(e.target.value)}
                    />
                  </Field>
                </div>
                <div className="mt-4 max-h-[420px] space-y-4 overflow-y-auto pr-1">
                  {template.sections.map((section) => (
                    <div key={section.title}>
                      <div className="label-cap mb-2">{section.title}</div>
                      <div className="grid grid-cols-2 gap-3">
                        {section.fields.map((f) => (
                          <Field key={f.key} label={f.label} unit={f.unit}>
                            <NumberInput
                              value={String(activeInputs[f.key] ?? 0)}
                              min={f.min}
                              max={f.max}
                              step={f.step}
                              onChange={(e) =>
                                setInputs({
                                  ...activeInputs,
                                  [f.key]: e.target.value === "" ? 0 : Number(e.target.value),
                                })
                              }
                            />
                          </Field>
                        ))}
                      </div>
                    </div>
                  ))}
                </div>
                <Button className="mt-4 w-full" onClick={() => void runScenario()} disabled={busy}>
                  {busy ? <Spinner /> : <Play size={14} />}
                  {busy ? "Solving…" : "Run scenario"}
                </Button>
                {workspace.scenarios.length > 0 && (
                  <Button
                    variant="ghost"
                    className="mt-2 w-full"
                    onClick={() => void rerunAll()}
                    disabled={busy}
                  >
                    Re-solve all scenarios with current inputs
                  </Button>
                )}
              </>
            ) : (
              <Banner tone="info" title="Guided template required">
                Scenario editing works for guided templates (the inputs stay attached to the
                model). Custom JSON models can be re-run from the{" "}
                <Link to="/optimizer/expert" className="underline">
                  expert editor
                </Link>
                . Backend work to snapshot arbitrary JSON scenarios is tracked in Documentation →
                Backend Roadmap.
              </Banner>
            )}

            {error && (
              <div className="mt-4">
                <Banner tone="bad" title="Scenario solve failed">
                  {error}
                </Banner>
              </div>
            )}
          </Panel>

          <Panel className="p-5">
            <div className="label-cap">Next</div>
            <div className="mt-3 flex flex-col gap-2">
              <Button
                variant="secondary"
                onClick={() => {
                  if (workspace.result) setResult(workspace.result);
                  navigate("/optimizer/results");
                }}
              >
                View baseline results <ArrowRight size={15} />
              </Button>
              <Link to="/optimizer">
                <Button variant="ghost" className="w-full">
                  New optimization
                </Button>
              </Link>
            </div>
          </Panel>
        </div>
      </div>
    </div>
  );
}
