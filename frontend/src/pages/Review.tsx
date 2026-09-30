import { useCallback, useEffect, useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import {
  AlertCircle,
  ArrowRight,
  CheckCircle2,
  Code2,
  Database,
  Edit3,
  FileJson,
  RefreshCw,
  ShieldCheck,
} from "lucide-react";
import {
  Badge,
  Banner,
  Button,
  PageHeader,
  Panel,
  Spinner,
  Stat,
} from "../components/ui";
import { validateModel } from "../services/api/solver";
import type { ValidationReport } from "../services/api/types";
import { useWorkspace } from "../workspace/WorkspaceContext";
import { formatPercent } from "../lib/format";

export default function Review() {
  const { workspace } = useWorkspace();
  const navigate = useNavigate();
  const model = workspace.model!;
  const [report, setReport] = useState<ValidationReport | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [showJson, setShowJson] = useState(false);

  const runValidation = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const res = await validateModel(model);
      setReport(res);
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setLoading(false);
    }
  }, [model]);

  useEffect(() => {
    void runValidation();
  }, [runValidation]);

  const jsonText = JSON.stringify(model, null, 2);

  return (
    <div>
      <PageHeader
        eyebrow="Step 2 — Review"
        title="Review & Validate"
        description="Inspect the generated model before sending it to the solver core. Validation runs on the backend via solver_cli --validate."
        right={
          <div className="flex items-center gap-2">
            <Badge tone="neutral">{model.name ?? "Untitled model"}</Badge>
            <Button variant="ghost" onClick={() => navigate(-1)}>
              <Edit3 size={14} /> Edit inputs
            </Button>
          </div>
        }
      />

      <div className="grid gap-5 lg:grid-cols-3">
        <div className="space-y-4 lg:col-span-2">
          <Panel className="p-5">
            <div className="flex items-center gap-2">
              <FileJson size={15} className="text-accent" />
              <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                What this model says
              </h3>
            </div>
            <ul className="mt-3 space-y-2 text-[13px] leading-relaxed text-ink-2">
              {workspace.narrative.length > 0 ? (
                workspace.narrative.map((line, i) => (
                  <li key={i} className="flex gap-2">
                    <span className="num mt-0.5 text-[11px] text-accent">{i + 1}</span>
                    <span>{line}</span>
                  </li>
                ))
              ) : (
                <li className="text-ink-3">Custom model — see structure below.</li>
              )}
            </ul>
          </Panel>

          <Panel className="p-5">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <Database size={15} className="text-accent" />
                <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                  Variables ({model.variables.length})
                </h3>
              </div>
              <Badge tone="neutral">
                {model.objective.sense === "maximize" ? "MAXIMIZE" : "MINIMIZE"}
              </Badge>
            </div>
            <div className="mt-3 overflow-x-auto">
              <table className="w-full text-left text-[13px]">
                <thead>
                  <tr className="border-b border-line text-[11px] uppercase tracking-wider text-ink-3">
                    <th className="py-2 pr-4 font-semibold">#</th>
                    <th className="py-2 pr-4 font-semibold">Name</th>
                    <th className="py-2 pr-4 font-semibold">Type</th>
                    <th className="py-2 pr-4 font-semibold">Lower bound</th>
                    <th className="py-2 font-semibold">Upper bound</th>
                  </tr>
                </thead>
                <tbody className="num">
                  {model.variables.map((v, i) => (
                    <tr key={i} className="border-b border-line/60 text-ink-2">
                      <td className="py-2 pr-4 text-ink-3">{i + 1}</td>
                      <td className="py-2 pr-4 text-ink">{v.name}</td>
                      <td className="py-2 pr-4">
                        <span
                          className={
                            v.type === "binary" || v.type === "integer"
                              ? "text-accent-2"
                              : "text-ink-3"
                          }
                        >
                          {v.type ?? "continuous"}
                        </span>
                      </td>
                      <td className="py-2 pr-4">{v.lb ?? "-∞"}</td>
                      <td className="py-2">{v.ub ?? "+∞"}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </Panel>

          <Panel className="p-5">
            <div className="flex items-center gap-2">
              <Code2 size={15} className="text-accent" />
              <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                Constraints ({model.constraints.length})
              </h3>
            </div>
            <div className="mt-3 overflow-x-auto">
              <table className="w-full text-left text-[13px]">
                <thead>
                  <tr className="border-b border-line text-[11px] uppercase tracking-wider text-ink-3">
                    <th className="py-2 pr-4 font-semibold">#</th>
                    <th className="py-2 pr-4 font-semibold">Name</th>
                    <th className="py-2 pr-4 font-semibold">Sense</th>
                    <th className="py-2 pr-4 font-semibold">RHS</th>
                    <th className="py-2 font-semibold">Non-zero coefficients</th>
                  </tr>
                </thead>
                <tbody className="num">
                  {model.constraints.map((c, i) => (
                    <tr key={i} className="border-b border-line/60 text-ink-2">
                      <td className="py-2 pr-4 text-ink-3">{i + 1}</td>
                      <td className="py-2 pr-4 text-ink">{c.name ?? `c${i + 1}`}</td>
                      <td className="py-2 pr-4 text-accent">{c.sense}</td>
                      <td className="py-2 pr-4">{c.rhs}</td>
                      <td className="py-2 text-ink-3">
                        {c.coefficients
                          .map((coef, j) => (coef !== 0 ? `${coef}·x${j + 1}` : null))
                          .filter(Boolean)
                          .join("  +  ") || "0"}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </Panel>
        </div>

        <div className="space-y-4">
          <Panel className="p-5">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <ShieldCheck size={15} className="text-accent" />
                <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                  Validation
                </h3>
              </div>
              <Button variant="ghost" onClick={() => void runValidation()} disabled={loading}>
                {loading ? <Spinner /> : <RefreshCw size={14} />} Re-check
              </Button>
            </div>

            {loading && !report && (
              <div className="mt-4 flex items-center gap-2 text-sm text-ink-2">
                <Spinner /> Validating on backend…
              </div>
            )}
            {error && (
              <div className="mt-4">
                <Banner tone="bad" title="Validation unavailable">
                  {error}
                </Banner>
              </div>
            )}
            {report && (
              <>
                <div className="mt-4">
                  {report.valid ? (
                    <Banner tone="good" title="Model is valid">
                      Structural checks passed — ready to solve.
                    </Banner>
                  ) : (
                    <Banner tone="bad" title="Model has errors">
                      Fix the issues below before solving.
                    </Banner>
                  )}
                </div>
                {report.issues.length > 0 && (
                  <ul className="mt-3 space-y-2">
                    {report.issues.map((issue, i) => (
                      <li
                        key={i}
                        className={`flex gap-2 rounded-lg border px-3 py-2 text-xs ${
                          issue.severity === "ERROR"
                            ? "border-bad/40 bg-bad/8 text-bad"
                            : "border-warn/40 bg-warn/8 text-warn"
                        }`}
                      >
                        {issue.severity === "ERROR" ? (
                          <AlertCircle size={14} className="mt-0.5 shrink-0" />
                        ) : (
                          <AlertCircle size={14} className="mt-0.5 shrink-0" />
                        )}
                        <span>
                          <span className="num font-semibold">{issue.code}</span> — {issue.message}
                        </span>
                      </li>
                    ))}
                  </ul>
                )}
                <div className="mt-4 grid grid-cols-2 gap-2">
                  <Stat label="Variables" value={report.statistics.num_variables} />
                  <Stat label="Constraints" value={report.statistics.num_constraints} />
                  <Stat label="Non-zeros" value={report.statistics.num_nonzeros} />
                  <Stat label="Density" value={formatPercent(report.statistics.density)} />
                  <Stat
                    label="Integer vars"
                    value={report.statistics.num_integer_variables}
                    tone={report.statistics.num_integer_variables > 0 ? "violet" : "neutral"}
                  />
                  <Stat
                    label="Binary vars"
                    value={report.statistics.num_binary_variables}
                    tone={report.statistics.num_binary_variables > 0 ? "violet" : "neutral"}
                  />
                </div>
              </>
            )}
          </Panel>

          <Panel className="p-5">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <FileJson size={15} className="text-ink-3" />
                <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                  Model JSON
                </h3>
              </div>
              <Button variant="ghost" onClick={() => setShowJson((s) => !s)}>
                {showJson ? "Hide" : "Show"}
              </Button>
            </div>
            {showJson && (
              <pre className="num mt-3 max-h-72 overflow-auto rounded-lg border border-line bg-bg-2/80 p-3 text-[11px] leading-relaxed text-ink-2">
                {jsonText}
              </pre>
            )}
          </Panel>

          <Panel className="sticky top-20 p-5">
            <div className="label-cap">Step 3</div>
            <h3 className="mt-1 text-base font-bold text-ink">Ready to optimize</h3>
            <p className="mt-2 text-[13px] text-ink-2">
              The model is posted to <code className="num text-accent">/api/solve</code> and solved
              by the C++ core with independent verification.
            </p>
            <Button
              className="mt-4 w-full"
              onClick={() => navigate("/optimizer/solving")}
              disabled={(report !== null && !report.valid) || loading}
            >
              <CheckCircle2 size={15} /> Optimize
              <ArrowRight size={15} />
            </Button>
            {!report && !loading && (
              <p className="mt-2 text-center text-[11px] text-ink-3">
                Waiting for first validation — you can optimize anyway.
              </p>
            )}
            <div className="mt-3">
              <Link
                to="/documentation"
                className="text-center text-[11px] text-ink-3 hover:text-accent"
              >
                Model format reference →
              </Link>
            </div>
          </Panel>
        </div>
      </div>
    </div>
  );
}
