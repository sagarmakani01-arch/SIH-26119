import { request } from "./client";
import type { HealthResponse, SolveResult, SolverModel, ValidationReport } from "./types";

export async function solveModel(model: SolverModel, timeoutMs = 130000): Promise<SolveResult> {
  return request<SolveResult>(
    "/api/solve",
    { method: "POST", body: JSON.stringify(model) },
    timeoutMs,
  );
}

export async function validateModel(model: SolverModel): Promise<ValidationReport> {
  return request<ValidationReport>("/api/validate", {
    method: "POST",
    body: JSON.stringify(model),
  });
}

export async function getHealth(): Promise<HealthResponse> {
  return request<HealthResponse>("/api/health", { method: "GET" }, 8000);
}

export async function getDemoModel(): Promise<SolverModel> {
  return request<SolverModel>("/api/demo", { method: "GET" }, 8000);
}
