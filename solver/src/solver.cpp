#include "solver/solver.hpp"

#include "solver/mip/branch_and_bound.hpp"

namespace solver {

ProblemClass classifyProblem(const OptimizationModel& model) {
  if (model.numIntegerVariables() > 0) return ProblemClass::MixedIntegerLinear;
  return ProblemClass::Linear;
}

Solver::Solver(SolverConfig config) : config_(std::move(config)) {}

namespace {

void collectWarnings(SolverResult& out, const std::vector<std::string>& extra) {
  out.warnings.insert(out.warnings.end(), extra.begin(), extra.end());
}

void collectStats(SolverStatistics& s, const PresolveStatistics& ps, double presolve_sec,
                  double solve_sec, double verify_sec, long iterations, long phase1,
                  int refactorizations, int rows, int cols, int nonzeros) {
  s.presolve_time_sec = presolve_sec;
  s.solve_time_sec = solve_sec;
  s.verify_time_sec = verify_sec;
  s.iterations = iterations;
  s.phase1_iterations = phase1;
  s.refactorizations = refactorizations;
  s.rows = rows;
  s.cols = cols;
  s.nonzeros = nonzeros;
  s.presolve_rows_removed = ps.rows_in - ps.rows_out;
  s.presolve_bounds_tightened = ps.bounds_tightened;
  s.presolve_fixed_variables_removed = ps.fixed_variables_removed;
  s.backend_used = "CPU";
}

}  // namespace

SolverResult Solver::solve(const OptimizationModel& model) const {
  SolverResult out;
  out.problem_class = classifyProblem(model);

  if (out.problem_class == ProblemClass::Quadratic) {
    out.status = SolveStatus::Error;
    out.termination_reason = "QP problems are not yet supported by this build";
    return out;
  }

  if (config_.backend == ComputeBackendKind::GPU) {
    out.warnings.push_back(
        "GPU backend requested but no CUDA build/hardware was detected; executed on CPU");
  }
  if (config_.threads > 1) {
    out.warnings.push_back("multithreading requested but this build runs single-threaded");
  }

  if (out.problem_class == ProblemClass::MixedIntegerLinear) {
    MIPConfig mip_config;
    mip_config.tol = config_.tolerances;
    mip_config.mip_rel_gap = config_.mip_rel_gap;
    mip_config.time_limit_sec = config_.time_limit_sec;
    mip_config.max_lp_iterations = config_.max_iterations;
    mip_config.presolve = config_.presolve;
    mip_config.verify = config_.verify;

    BranchAndBound bb;
    MIPResult mip = bb.solve(model, mip_config);

    out.status = mip.status;
    out.objective = mip.objective;
    out.x = std::move(mip.x);
    out.feasible_point = mip.feasible_point;
    out.verified = mip.verified;
    out.primal_violation = mip.primal_violation;
    out.duality_gap = mip.mip_gap;
    out.termination_reason = std::move(mip.termination_reason);
    collectWarnings(out, mip.warnings);
    if (out.feasible_point) out.constraint_activities = model.rowActivities(out.x);

    if (config_.collect_statistics) {
      out.statistics.nodes = mip.nodes;
      collectStats(out.statistics, mip.presolve_stats, mip.presolve_time_sec,
                   mip.solve_time_sec, mip.verify_time_sec, mip.lp_iterations, 0, 0,
                   model.numConstraints(), model.numVariables(), model.numNonzeros());
    }
    return out;
  }

  LPConfig lp_config;
  lp_config.simplex.tol = config_.tolerances;
  lp_config.simplex.max_iterations = config_.max_iterations;
  lp_config.simplex.time_limit_sec = config_.time_limit_sec;
  lp_config.algorithm = config_.lp_algorithm;
  lp_config.presolve.enabled = config_.presolve;
  lp_config.verify = config_.verify;

  LPSolver lp_solver;
  LPResult lp = lp_solver.solve(model, lp_config);

  out.status = lp.status;
  out.objective = lp.objective;
  out.x = std::move(lp.x);
  out.duals = std::move(lp.duals);
  out.reduced_costs = std::move(lp.reduced_costs);
  out.feasible_point = lp.feasible_point;
  out.verified = lp.verified;
  out.primal_violation = lp.primal_violation;
  out.dual_violation = lp.dual_violation;
  out.duality_gap = lp.duality_gap;
  out.termination_reason = std::move(lp.termination_reason);
  collectWarnings(out, lp.warnings);

  if (out.feasible_point) {
    out.constraint_activities = model.rowActivities(out.x);
  }

  if (config_.collect_statistics) {
    collectStats(out.statistics, lp.presolve_stats, lp.presolve_time_sec, lp.solve_time_sec,
                 lp.verify_time_sec, lp.iterations, lp.phase1_iterations,
                 lp.refactorizations, lp.rows, lp.cols, lp.nonzeros);
  }

  return out;
}

}  // namespace solver
