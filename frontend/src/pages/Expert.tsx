import { useEffect, useState } from "react";
import { Link, useLocation, useNavigate } from "react-router-dom";
import { AlertTriangle, CheckCircle2, FileJson, Play, Wand2 } from "lucide-react";
import {
  Badge,
  Banner,
  Button,
  PageHeader,
  Panel,
  Spinner,
} from "../components/ui";
import { getDemoModel, validateModel } from "../services/api/solver";
import type { ValidationReport, SolverModel } from "../services/api/types";
import { useWorkspace } from "../workspace/WorkspaceContext";

const sampleModel = `{
  "name": "Custom model",
  "objective": { "sense": "maximize", "coefficients": [3, 5] },
  "variables": [
    { "name": "x1", "type": "continuous", "lb": 0, "ub": 4 },
    { "name": "x2", "type": "continuous", "lb": 0, "ub": 6 }
  ],
  "constraints": [
    { "name": "labor", "sense": "<=", "coefficients": [1, 2], "rhs": 8 },
    { "name": "material", "sense": "<=", "coefficients": [3, 2], "rhs": 12 }
  ],
  "config": { "presolve": true, "verify": true, "backend": "auto" }
}`;

export default function Expert() {
  const navigate = useNavigate();
  const location = useLocation();
  const { workspace, setModelFromBuild } = useWorkspace();
  const initial = (() => {
    const stateModel = (location.state as { model?: SolverModel } | null)?.model;
    if (stateModel) return JSON.stringify(stateModel, null, 2);
    if (workspace.model) return JSON.stringify(workspace.model, null, 2);
    return sampleModel;
  })();
  const [text, setText] = useState(initial);
  const [parseError, setParseError] = useState<string | null>(null);
  const [report, setReport] = useState<ValidationReport | null>(null);
  const [busy, setBusy] = useState<"validate" | "demo" | null>(null);
  const [actionError, setActionError] = useState<string | null>(null);

  useEffect(() => {
    setParseError(null);
    setReport(null);
    setActionError(null);
  }, [text]);

  function parse(): SolverModel | null {
    setParseError(null);
    try {
      const parsed = JSON.parse(text) as SolverModel;
      if (!parsed || typeof parsed !== "object" || !parsed.objective || !Array.isArray(parsed.variables)) {
        setParseError("JSON is not a solver model — expected objective, variables, constraints.");
        return null;
      }
      return parsed;
    } catch (err) {
      setParseError(err instanceof Error ? err.message : String(err));
      return null;
    }
  }

  async function validate() {
    const model = parse();
    if (!model) return;
    setBusy("validate");
    setActionError(null);
    try {
      setReport(await validateModel(model));
    } catch (err) {
      setActionError(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(null);
    }
  }

  async function loadDemo() {
    setBusy("demo");
    setActionError(null);
    try {
      const model = await getDemoModel();
      setText(JSON.stringify(model, null, 2));
    } catch (err) {
      setActionError(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(null);
    }
  }

  function review() {
    const model = parse();
    if (!model) return;
    setModelFromBuild({
      templateId: null,
      inputs: null,
      model,
      narrative: [],
      objectiveUnit: "₹",
      label: model.name ?? "Custom JSON model",
    });
    navigate("/optimizer/review");
  }

  return (
    <div>
      <PageHeader
        eyebrow="Step 1 — Expert mode"
        title="JSON Model Editor"
        description="Author the full model in the open JSON format. Validate it against the backend, then push it through the standard review → solve flow."
        right={<Badge tone="neutral">Raw model format</Badge>}
      />

      <div className="grid gap-5 lg:grid-cols-3">
        <div className="lg:col-span-2">
          <Panel className="p-4">
            <div className="mb-3 flex items-center justify-between">
              <div className="flex items-center gap-2">
                <FileJson size={15} className="text-accent" />
                <span className="text-sm font-bold uppercase tracking-wider text-ink-2">
                  model.json
                </span>
              </div>
              <div className="flex gap-2">
                <Button variant="ghost" onClick={() => void loadDemo()} disabled={busy !== null}>
                  {busy === "demo" ? <Spinner /> : <Wand2 size={14} />} Load API demo
                </Button>
              </div>
            </div>
            <textarea
              spellCheck={false}
              value={text}
              onChange={(e) => setText(e.target.value)}
              className="num focus-ring h-[440px] w-full resize-y rounded-xl border border-line-2 bg-bg-2/90 p-4 text-[12.5px] leading-relaxed text-ink-2 focus:border-accent/60"
            />
            {parseError && (
              <div className="mt-3">
                <Banner tone="bad" title="Parse error">
                  <span className="num">{parseError}</span>
                </Banner>
              </div>
            )}
            {actionError && (
              <div className="mt-3">
                <Banner tone="bad" title="Backend error">
                  {actionError}
                </Banner>
              </div>
            )}
            <div className="mt-4 flex flex-wrap gap-3">
              <Button variant="secondary" onClick={() => void validate()} disabled={busy !== null}>
                {busy === "validate" ? <Spinner /> : <CheckCircle2 size={15} />} Validate
              </Button>
              <Button onClick={review} disabled={busy !== null}>
                <Play size={15} /> Build & Review
              </Button>
            </div>
          </Panel>
        </div>

        <div className="space-y-4">
          <Panel className="p-5">
            <div className="flex items-center justify-between">
              <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                Validation report
              </h3>
              {report && (
                <Badge tone={report.valid ? "good" : "bad"}>{report.valid ? "VALID" : "ERRORS"}</Badge>
              )}
            </div>
            {report ? (
              <div className="mt-3 space-y-3">
                <div className={`rounded-lg border px-3 py-2 text-[13px] ${
                  report.valid ? "border-good/40 bg-good/8 text-good" : "border-bad/40 bg-bad/8 text-bad"
                }`}>
                  {report.valid
                    ? "Structural checks passed."
                    : `${report.issues.filter((i) => i.severity === "ERROR").length} error(s) found.`}
                </div>
                <ul className="space-y-2">
                  {report.issues.map((issue, i) => (
                    <li
                      key={i}
                      className={`rounded-lg border px-3 py-2 text-xs ${
                        issue.severity === "ERROR"
                          ? "border-bad/40 bg-bad/8 text-bad"
                          : "border-warn/40 bg-warn/8 text-warn"
                      }`}
                    >
                      <span className="num font-semibold">{issue.code}</span> — {issue.message}
                    </li>
                  ))}
                </ul>
                <div className="num grid grid-cols-2 gap-2 text-[12px] text-ink-2">
                  <div>variables: {report.statistics.num_variables}</div>
                  <div>constraints: {report.statistics.num_constraints}</div>
                  <div>nonzeros: {report.statistics.num_nonzeros}</div>
                  <div>integer: {report.statistics.num_integer_variables}</div>
                </div>
              </div>
            ) : (
              <p className="mt-3 text-[13px] text-ink-3">
                Not validated yet — press <span className="text-ink-2">Validate</span> to run{" "}
                <code className="num text-accent">solver_cli --validate</code> on the backend.
              </p>
            )}
          </Panel>

          <Panel className="p-5">
            <div className="flex items-center gap-2">
              <AlertTriangle size={15} className="text-warn" />
              <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">Format help</h3>
            </div>
            <pre className="num mt-3 overflow-auto rounded-lg border border-line bg-bg-2/70 p-3 text-[11px] leading-relaxed text-ink-3">{`{
  "name": "...",
  "objective": {
    "sense": "maximize|minimize",
    "coefficients": [ ... ]
  },
  "variables": [
    { "name", "type":
      "continuous|integer|binary",
      "lb", "ub" }
  ],
  "constraints": [
    { "name", "sense":
      "<=|>=|=",
      "coefficients": [...],
      "rhs": number }
  ],
  "config": {
    "time_limit_sec", "mip_rel_gap",
    "presolve", "verify", "backend"
  }
}`}</pre>
            <Link to="/documentation" className="mt-3 inline-block text-[13px] text-accent hover:underline">
              Full format reference →
            </Link>
          </Panel>
        </div>
      </div>
    </div>
  );
}
