import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import type { SolveResult, SolverModel } from "../services/api/types";

export interface WorkspaceData {
  templateId: string | null;
  inputs: Record<string, number | string> | null;
  model: SolverModel | null;
  narrative: string[];
  objectiveUnit: string;
  label: string;
  result: SolveResult | null;
  scenarios: Scenario[];
}

export interface Scenario {
  id: string;
  name: string;
  templateId: string | null;
  inputs: Record<string, number | string> | null;
  result: SolveResult | null;
  objectiveUnit: string;
}

const emptyWorkspace: WorkspaceData = {
  templateId: null,
  inputs: null,
  model: null,
  narrative: [],
  objectiveUnit: "₹",
  label: "",
  result: null,
  scenarios: [],
};

const STORAGE_KEY = "sovereign-workspace-v1";

interface WorkspaceContextValue {
  workspace: WorkspaceData;
  setModelFromBuild: (payload: {
    templateId: string | null;
    inputs: Record<string, number | string> | null;
    model: SolverModel;
    narrative: string[];
    objectiveUnit: string;
    label: string;
  }) => void;
  setResult: (result: SolveResult) => void;
  clearResult: () => void;
  reset: () => void;
  addScenario: (scenario: Omit<Scenario, "id">) => void;
  removeScenario: (id: string) => void;
}

const WorkspaceContext = createContext<WorkspaceContextValue | null>(null);

function loadWorkspace(): WorkspaceData {
  if (typeof window === "undefined") return emptyWorkspace;
  try {
    const raw = window.sessionStorage.getItem(STORAGE_KEY);
    if (!raw) return emptyWorkspace;
    const parsed = JSON.parse(raw) as Partial<WorkspaceData>;
    return { ...emptyWorkspace, ...parsed };
  } catch {
    return emptyWorkspace;
  }
}

export function WorkspaceProvider({ children }: { children: ReactNode }) {
  const [workspace, setWorkspace] = useState<WorkspaceData>(loadWorkspace);

  useEffect(() => {
    try {
      window.sessionStorage.setItem(STORAGE_KEY, JSON.stringify(workspace));
    } catch {
      // storage full or unavailable — workspace still works in memory
    }
  }, [workspace]);

  const setModelFromBuild = useCallback<WorkspaceContextValue["setModelFromBuild"]>((payload) => {
    setWorkspace((prev) => ({
      ...prev,
      templateId: payload.templateId,
      inputs: payload.inputs,
      model: payload.model,
      narrative: payload.narrative,
      objectiveUnit: payload.objectiveUnit,
      label: payload.label,
      result: null,
      scenarios: prev.scenarios,
    }));
  }, []);

  const setResult = useCallback((result: SolveResult) => {
    setWorkspace((prev) => ({ ...prev, result }));
  }, []);

  const clearResult = useCallback(() => {
    setWorkspace((prev) => ({ ...prev, result: null }));
  }, []);

  const reset = useCallback(() => setWorkspace({ ...emptyWorkspace }), []);

  const addScenario = useCallback((scenario: Omit<Scenario, "id">) => {
    setWorkspace((prev) => ({
      ...prev,
      scenarios: [
        ...prev.scenarios,
        { ...scenario, id: `sc_${Date.now()}_${prev.scenarios.length}` },
      ],
    }));
  }, []);

  const removeScenario = useCallback((id: string) => {
    setWorkspace((prev) => ({
      ...prev,
      scenarios: prev.scenarios.filter((s) => s.id !== id),
    }));
  }, []);

  const value = useMemo(
    () => ({
      workspace,
      setModelFromBuild,
      setResult,
      clearResult,
      reset,
      addScenario,
      removeScenario,
    }),
    [workspace, setModelFromBuild, setResult, clearResult, reset, addScenario, removeScenario],
  );

  return <WorkspaceContext.Provider value={value}>{children}</WorkspaceContext.Provider>;
}

export function useWorkspace(): WorkspaceContextValue {
  const ctx = useContext(WorkspaceContext);
  if (!ctx) throw new Error("useWorkspace must be used inside WorkspaceProvider");
  return ctx;
}
