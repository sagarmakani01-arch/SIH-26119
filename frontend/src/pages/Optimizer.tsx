import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import {
  ArrowRight,
  Braces,
  FileCode2,
  Gauge,
  Lock,
  Sparkles,
  Wand2,
} from "lucide-react";
import { Badge, Banner, Button, PageHeader, Panel, TextInput } from "../components/ui";
import { templates } from "../templates";
import { ApiError } from "../services/api/client";

const iconFor: Record<string, typeof Gauge> = {
  refinery: Gauge,
  production: FileCode2,
  logistics: ArrowRight,
  resource: Braces,
  scheduling: Wand2,
};

function AiPanel() {
  const navigate = useNavigate();
  const [brief, setBrief] = useState("");
  const [attempted, setAttempted] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  async function generate() {
    setAttempted(true);
    setError(null);
    setBusy(true);
    try {
      const res = await fetch("/api/ai/generate-model", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ prompt: brief }),
      });
      if (!res.ok) {
        throw new ApiError(res.status, `HTTP ${res.status}`);
      }
      const data = await res.json();
      if (data?.model) {
        navigate("/optimizer/expert", { state: { model: data.model } });
        return;
      }
      setError("The endpoint returned no model.");
    } catch (err) {
      const apiErr = err as ApiError;
      setError(
        apiErr.status === 404
          ? "not-404"
          : `Model generation failed: ${apiErr.message ?? String(err)}`,
      );
    } finally {
      setBusy(false);
    }
  }

  return (
    <Panel className="p-6">
      <div className="flex items-center gap-2">
        <Sparkles size={17} className="text-accent-2" />
        <h3 className="text-base font-bold text-ink">AI Model Generation</h3>
        <Badge tone="violet">Experimental</Badge>
      </div>
      <p className="mt-2 text-[13px] text-ink-2">
        Describe the problem in plain language and the backend will attempt to emit a solver model.
      </p>
      <div className="mt-4">
        <TextInput
          placeholder="e.g. Maximize profit from two products with limited labor hours…"
          value={brief}
          onChange={(e) => setBrief(e.target.value)}
          disabled={busy}
        />
      </div>
      <div className="mt-3">
        <Button variant="secondary" onClick={generate} disabled={busy || !brief.trim()}>
          {busy ? "Contacting /api/ai/generate-model…" : "Generate Model"}
        </Button>
      </div>
      {attempted && error === "not-404" && (
        <div className="mt-4">
          <Banner tone="warn" title="AI generation capability pending">
            The backend for natural-language model generation is not available in this build.
            Required backend work: <code className="num">POST /api/ai/generate-model</code>{" "}
            (documented in Documentation → Backend Roadmap). Guided and Expert modes are fully
            functional — try one of them instead.
          </Banner>
        </div>
      )}
      {attempted && error && error !== "not-404" && (
        <div className="mt-4">
          <Banner tone="bad" title="Generation failed">
            {error}
          </Banner>
        </div>
      )}
      {!attempted && (
        <div className="mt-4 flex items-center gap-2 text-xs text-ink-3">
          <Lock size={13} />
          If the capability is absent you will see its real state — never a simulated result.
        </div>
      )}
    </Panel>
  );
}

export default function Optimizer() {
  return (
    <div>
      <PageHeader
        eyebrow="Step 1 — Choose a path"
        title="Optimization Workspace"
        description="Start from a guided template, ask the assistant to draft a model, or paste expert JSON directly."
      />

      <div className="grid gap-5 lg:grid-cols-3">
        <div className="lg:col-span-2">
          <div className="mb-3 flex items-center justify-between">
            <h2 className="text-sm font-bold uppercase tracking-wider text-ink-2">
              Guided Templates
            </h2>
            <Badge tone="good">Live solves</Badge>
          </div>
          <div className="grid gap-4 sm:grid-cols-2">
            {templates.map((t) => {
              const Icon = iconFor[t.id] ?? Gauge;
              return (
                <Link key={t.id} to={`/optimizer/guided/${t.id}`} className="group">
                  <Panel className="h-full p-5 transition-all group-hover:border-accent/50 group-hover:glow-accent">
                    <div className="flex items-start justify-between">
                      <span className="grid h-10 w-10 place-items-center rounded-xl border border-line-2 bg-panel-2 text-accent">
                        <Icon size={18} />
                      </span>
                      <Badge tone={t.tag === "MILP" ? "violet" : "accent"}>{t.tag}</Badge>
                    </div>
                    <h3 className="mt-3 text-[15px] font-semibold text-ink">{t.name}</h3>
                    <p className="mt-1.5 line-clamp-3 text-[13px] leading-relaxed text-ink-2">
                      {t.description}
                    </p>
                    <div className="mt-4 flex items-center justify-between">
                      {t.syntheticNote ? (
                        <span className="text-[11px] text-warn">Synthetic demo data</span>
                      ) : (
                        <span />
                      )}
                      <span className="flex items-center gap-1 text-[13px] font-semibold text-accent">
                        Configure <ArrowRight size={14} />
                      </span>
                    </div>
                  </Panel>
                </Link>
              );
            })}
            <Link to="/optimizer/expert" className="group">
              <Panel className="h-full p-5 transition-all group-hover:border-line-2">
                <div className="flex items-start justify-between">
                  <span className="grid h-10 w-10 place-items-center rounded-xl border border-line-2 bg-panel-2 text-ink-2">
                    <Braces size={18} />
                  </span>
                  <Badge tone="neutral">JSON</Badge>
                </div>
                <h3 className="mt-3 text-[15px] font-semibold text-ink">Expert Mode</h3>
                <p className="mt-1.5 text-[13px] leading-relaxed text-ink-2">
                  Paste a full model in the open JSON format — variables, constraints, objective and
                  solver config.
                </p>
                <div className="mt-4 flex items-center justify-between">
                  <span className="text-[11px] text-ink-3">Raw model format</span>
                  <span className="flex items-center gap-1 text-[13px] font-semibold text-accent">
                    Open editor <ArrowRight size={14} />
                  </span>
                </div>
              </Panel>
            </Link>
          </div>
        </div>

        <div className="space-y-5">
          <AiPanel />
          <Panel className="p-6">
            <div className="flex items-center gap-2">
              <Gauge size={17} className="text-accent" />
              <h3 className="text-base font-bold text-ink">How a solve works</h3>
            </div>
            <ol className="mt-3 space-y-2.5 text-[13px] text-ink-2">
              <li className="flex gap-2.5">
                <span className="num text-accent">1</span> Build or edit the model JSON
              </li>
              <li className="flex gap-2.5">
                <span className="num text-accent">2</span> Review structure & validation issues
              </li>
              <li className="flex gap-2.5">
                <span className="num text-accent">3</span> POST to{" "}
                <code className="num text-ink">/api/solve</code>
              </li>
              <li className="flex gap-2.5">
                <span className="num text-accent">4</span> C++ core solves + verifies the result
              </li>
              <li className="flex gap-2.5">
                <span className="num text-accent">5</span> Charts, stats and explanations render
              </li>
            </ol>
          </Panel>
        </div>
      </div>
    </div>
  );
}
