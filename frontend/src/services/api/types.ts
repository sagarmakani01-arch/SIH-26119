export type VarType = "continuous" | "integer" | "binary";
export type ConstraintSense = "<=" | ">=" | "=";
export type ObjectiveSense = "minimize" | "maximize";

export interface SolverVariable {
  name: string;
  type?: VarType;
  lb?: number | null;
  ub?: number | null;
}

export interface SolverConstraint {
  name?: string;
  sense: ConstraintSense;
  coefficients: number[];
  rhs: number;
}

export interface SolverObjective {
  sense: ObjectiveSense;
  coefficients: number[];
  constant?: number;
}

export interface SolverConfig {
  time_limit_sec?: number;
  max_iterations?: number;
  mip_rel_gap?: number;
  presolve?: boolean;
  verify?: boolean;
  backend?: "auto" | "cpu" | "gpu";
  tolerances?: {
    primal_feasibility?: number;
    dual_feasibility?: number;
    integrality?: number;
  };
}

export interface SolverModel {
  name?: string;
  objective: SolverObjective;
  variables: SolverVariable[];
  constraints: SolverConstraint[];
  config?: SolverConfig;
}

export type SolveStatus =
  | "NOT_SOLVED"
  | "OPTIMAL"
  | "FEASIBLE"
  | "INFEASIBLE"
  | "UNBOUNDED"
  | "TIME_LIMIT"
  | "ITERATION_LIMIT"
  | "NUMERICAL_ERROR"
  | "ERROR";

export interface SolveStatistics {
  solve_time_sec: number;
  presolve_time_sec: number;
  verify_time_sec: number;
  iterations: number;
  phase1_iterations: number;
  nodes: number;
  refactorizations: number;
  rows: number;
  cols: number;
  nonzeros: number;
  presolve_rows_removed: number;
  presolve_bounds_tightened: number;
  presolve_fixed_variables_removed: number;
  backend_used: string;
}

export interface SolveResult {
  status: SolveStatus;
  problem_class: "LP" | "MILP" | "QP";
  objective: number;
  feasible_point: boolean;
  verified: boolean;
  primal_violation: number;
  dual_violation: number;
  duality_gap: number | null;
  x: number[];
  duals: number[];
  reduced_costs: number[];
  constraint_activities: number[];
  termination_reason: string;
  warnings: string[];
  statistics: SolveStatistics;
}

export interface ValidationIssue {
  severity: "ERROR" | "WARNING";
  code: string;
  context: string;
  message: string;
}

export interface ModelStatistics {
  num_variables: number;
  num_constraints: number;
  num_nonzeros: number;
  num_integer_variables: number;
  num_binary_variables: number;
  num_free_variables: number;
  num_fixed_variables: number;
  density: number;
}

export interface ValidationReport {
  valid: boolean;
  status: "OK" | "ERROR";
  issues: ValidationIssue[];
  statistics: ModelStatistics;
}

export interface HealthResponse {
  status: string;
  api_version: string;
  solver_cli: { path: string | null; available: boolean };
  solver_version: string | null;
}
