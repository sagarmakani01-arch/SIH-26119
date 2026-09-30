import { useMemo, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import { AlertTriangle, ArrowRight, FlaskConical, RotateCcw } from "lucide-react";
import {
  Badge,
  Banner,
  Button,
  Field,
  NumberInput,
  PageHeader,
  Panel,
  TextInput,
} from "../components/ui";
import { getTemplate } from "../templates";
import { useWorkspace } from "../workspace/WorkspaceContext";
import type { FieldType } from "../templates";

function Control({
  type,
  value,
  min,
  max,
  step,
  options,
  onChange,
}: {
  type: FieldType;
  value: number | string;
  min?: number;
  max?: number;
  step?: number;
  options?: { value: string; label: string }[];
  onChange: (v: number | string) => void;
}) {
  if (type === "select" && options) {
    return (
      <select
        className="focus-ring num w-full rounded-lg border border-line-2 bg-bg-2/80 px-3 py-2 text-sm text-ink"
        value={String(value)}
        onChange={(e) => onChange(e.target.value)}
      >
        {options.map((o) => (
          <option key={o.value} value={o.value}>
            {o.label}
          </option>
        ))}
      </select>
    );
  }
  if (type === "text") {
    return (
      <TextInput value={String(value)} onChange={(e) => onChange(e.target.value)} />
    );
  }
  return (
    <NumberInput
      value={Number.isFinite(Number(value)) ? String(value) : ""}
      min={min}
      max={max}
      step={step}
      onChange={(e) => {
        const parsed = e.target.value === "" ? 0 : Number(e.target.value);
        onChange(Number.isFinite(parsed) ? parsed : 0);
      }}
    />
  );
}

export default function Guided() {
  const { templateId } = useParams<{ templateId: string }>();
  const navigate = useNavigate();
  const template = getTemplate(templateId);
  const { workspace, setModelFromBuild } = useWorkspace();
  const [inputs, setInputs] = useState<Record<string, number | string>>(() => {
    if (workspace.templateId === templateId && workspace.inputs) return workspace.inputs;
    return template ? template.defaults() : {};
  });
  const [error, setError] = useState<string | null>(null);

  const objectiveSummary = useMemo(() => {
    if (!template) return "";
    return template.tag === "LP" ? "Linear Program" : "Mixed-Integer Program";
  }, [template]);

  if (!template) {
    return (
      <Banner tone="bad" title="Unknown template">
        No template matches <code>{templateId}</code>.{" "}
        <Link to="/optimizer" className="underline">
          Back to Optimizer
        </Link>
      </Banner>
    );
  }

  function set(key: string, v: number | string) {
    setInputs((prev) => ({ ...prev, [key]: v }));
  }

  function build() {
    setError(null);
    try {
      const out = template!.build(inputs);
      if (
        !Number.isFinite(Number(out.model.objective.coefficients[0])) ||
        out.model.variables.length === 0
      ) {
        throw new Error("Model contains invalid numeric data");
      }
      setModelFromBuild({
        templateId: template!.id,
        inputs,
        model: out.model,
        narrative: out.narrative,
        objectiveUnit: template!.objectiveUnit,
        label: template!.name,
      });
      navigate("/optimizer/review");
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    }
  }

  function resetDefaults() {
    setInputs(template!.defaults());
    setError(null);
  }

  return (
    <div>
      <PageHeader
        eyebrow={`Step 1 — Guided setup · ${objectiveSummary}`}
        title={template.name}
        description={template.description}
        right={
          <div className="flex items-center gap-2">
            <Badge tone={template.tag === "MILP" ? "violet" : "accent"}>{template.tag}</Badge>
            <Button variant="ghost" onClick={resetDefaults}>
              <RotateCcw size={14} /> Reset defaults
            </Button>
          </div>
        }
      />

      {template.syntheticNote && (
        <div className="mb-5">
          <Banner tone="info" title="Demo / Synthetic Data — not MRPL data">
            Every number below is an artificial example chosen to demonstrate the engine. Replace
            inputs with real plant data through the same fields or by editing JSON in Expert Mode.
          </Banner>
        </div>
      )}

      <div className="grid gap-5 lg:grid-cols-4">
        <div className="space-y-4 lg:col-span-3">
          {template.sections.map((section) => (
            <Panel key={section.title} className="p-5">
              <div className="flex items-center gap-2">
                <FlaskConical size={15} className="text-accent" />
                <h3 className="text-sm font-bold uppercase tracking-wider text-ink-2">
                  {section.title}
                </h3>
              </div>
              {section.description ? (
                <p className="mt-1.5 text-xs text-ink-3">{section.description}</p>
              ) : null}
              <div className="mt-4 grid gap-4 sm:grid-cols-2 xl:grid-cols-3">
                {section.fields.map((f) => (
                  <Field key={f.key} label={f.label} unit={f.unit} hint={f.hint}>
                    <Control
                      type={f.type}
                      value={inputs[f.key] ?? 0}
                      min={f.min}
                      max={f.max}
                      step={f.step}
                      options={f.options}
                      onChange={(v) => set(f.key, v)}
                    />
                  </Field>
                ))}
              </div>
            </Panel>
          ))}
          {error && (
            <Banner tone="bad" title="Could not build model">
              {error}
            </Banner>
          )}
        </div>

        <div className="lg:col-span-1">
          <Panel className="sticky top-20 p-5">
            <div className="label-cap">Next step</div>
            <h3 className="mt-1 text-base font-bold text-ink">Build Optimization Model</h3>
            <p className="mt-2 text-[13px] leading-relaxed text-ink-2">
              Converts these inputs into the open JSON model — variables, constraints, objective and
              solver configuration — and opens the review screen.
            </p>
            <div className="mt-4 space-y-2">
              <Button className="w-full" onClick={build}>
                Build Model <ArrowRight size={15} />
              </Button>
              <Link to="/optimizer" className="block">
                <Button variant="ghost" className="w-full">
                  Back to paths
                </Button>
              </Link>
            </div>
            <div className="mt-4 rounded-lg border border-line bg-bg-2/60 p-3 text-[11px] text-ink-3">
              <div className="flex items-center gap-1.5 text-warn">
                <AlertTriangle size={12} /> Infeasible inputs are allowed — the solver will report
                them honestly on the results screen.
              </div>
            </div>
          </Panel>
        </div>
      </div>
    </div>
  );
}
