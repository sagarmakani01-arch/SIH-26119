import { useCallback, useEffect, useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { ArrowRight, Database, Download, Layers, Play, Plus, Trash2 } from "lucide-react";
import {
  Badge,
  Banner,
  Button,
  EmptyState,
  PageHeader,
  Panel,
} from "../components/ui";
import { useWorkspace } from "../workspace/WorkspaceContext";
import type { SolverModel } from "../services/api/types";

interface StoredModel {
  id: string;
  name: string;
  savedAt: string;
  model: SolverModel;
  templateId: string | null;
  inputs: Record<string, number | string> | null;
  narrative: string[];
  objectiveUnit: string;
  label: string;
}

const STORAGE_KEY = "sovereign-models-v1";

function loadModels(): StoredModel[] {
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? (parsed as StoredModel[]) : [];
  } catch {
    return [];
  }
}

function saveModels(models: StoredModel[]) {
  try {
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(models));
  } catch {
    // storage unavailable — session-only
  }
}

export default function Models() {
  const { workspace, setModelFromBuild } = useWorkspace();
  const navigate = useNavigate();
  const [models, setModels] = useState<StoredModel[]>(loadModels);

  useEffect(() => {
    saveModels(models);
  }, [models]);

  const saveCurrent = useCallback(() => {
    if (!workspace.model) return;
    const entry: StoredModel = {
      id: `m_${Date.now()}`,
      name: workspace.model.name ?? workspace.label ?? "Untitled model",
      savedAt: new Date().toISOString(),
      model: workspace.model,
      templateId: workspace.templateId,
      inputs: workspace.inputs,
      narrative: workspace.narrative,
      objectiveUnit: workspace.objectiveUnit,
      label: workspace.label,
    };
    setModels((prev) => [entry, ...prev]);
  }, [workspace]);

  function open(m: StoredModel) {
    setModelFromBuild({
      templateId: m.templateId,
      inputs: m.inputs,
      model: m.model,
      narrative: m.narrative,
      objectiveUnit: m.objectiveUnit,
      label: m.label,
    });
    navigate("/optimizer/review");
  }

  function run(m: StoredModel) {
    setModelFromBuild({
      templateId: m.templateId,
      inputs: m.inputs,
      model: m.model,
      narrative: m.narrative,
      objectiveUnit: m.objectiveUnit,
      label: m.label,
    });
    navigate("/optimizer/solving");
  }

  return (
    <div>
      <PageHeader
        eyebrow="Library"
        title="Models"
        description="Your saved optimization models — open, re-run or delete them."
        right={
          <div className="flex gap-2">
            <Button variant="secondary" onClick={saveCurrent} disabled={!workspace.model}>
              <Plus size={15} /> Save current model
            </Button>
            <Link to="/optimizer">
              <Button>
                <Plus size={15} /> New model
              </Button>
            </Link>
          </div>
        }
      />

      <div className="mb-5">
        <Banner tone="info" title="Stored in this browser (local workspace)">
          Model persistence uses browser storage on this device. Server-side model registry with
          database storage is tracked in Documentation → Backend Roadmap.
        </Banner>
      </div>

      {!workspace.model && models.length === 0 ? (
        <EmptyState
          icon={<Layers size={24} />}
          title="No models yet"
          body="Build one from a guided template, author JSON in expert mode, then save it here."
          action={
            <Link to="/optimizer">
              <Button>
                Open the Optimizer <ArrowRight size={15} />
              </Button>
            </Link>
          }
        />
      ) : (
        <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-3">
          {workspace.model && (
            <Panel className="border-accent/40 p-5">
              <div className="flex items-center justify-between">
                <Badge tone="accent">In workspace</Badge>
                <span className="text-[11px] text-ink-3">unsaved</span>
              </div>
              <h3 className="mt-3 text-[15px] font-semibold text-ink">
                {workspace.model.name ?? workspace.label ?? "Untitled model"}
              </h3>
              <div className="num mt-2 text-xs text-ink-3">
                {workspace.model.variables.length} vars · {workspace.model.constraints.length}{" "}
                constraints · {workspace.model.objective.sense}
              </div>
              <div className="mt-4 flex gap-2">
                <Button variant="secondary" className="flex-1" onClick={() => navigate("/optimizer/review")}>
                  Review
                </Button>
                <Button variant="ghost" onClick={saveCurrent} disabled={!workspace.model}>
                  <Download size={14} /> Save
                </Button>
              </div>
            </Panel>
          )}

          {models.map((m) => (
            <Panel key={m.id} className="p-5">
              <div className="flex items-center justify-between">
                <Badge tone="neutral">{m.model.objective.sense}</Badge>
                <span className="text-[11px] text-ink-3">
                  {new Date(m.savedAt).toLocaleString()}
                </span>
              </div>
              <h3 className="mt-3 truncate text-[15px] font-semibold text-ink">{m.name}</h3>
              <div className="num mt-2 text-xs text-ink-3">
                {m.model.variables.length} vars · {m.model.constraints.length} constraints
                {m.templateId ? ` · template: ${m.templateId}` : " · custom"}
              </div>
              <div className="mt-4 flex gap-2">
                <Button variant="secondary" className="flex-1" onClick={() => open(m)}>
                  <Database size={14} /> Open
                </Button>
                <Button className="flex-1" onClick={() => run(m)}>
                  <Play size={14} /> Run
                </Button>
                <button
                  onClick={() => setModels((prev) => prev.filter((x) => x.id !== m.id))}
                  className="focus-ring rounded-lg border border-line-2 px-2.5 text-ink-3 transition-colors hover:border-bad/50 hover:text-bad"
                  title="Delete"
                >
                  <Trash2 size={14} />
                </button>
              </div>
            </Panel>
          ))}
        </div>
      )}
    </div>
  );
}
