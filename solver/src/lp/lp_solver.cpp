#include "solver/lp/lp_solver.hpp"

#include <chrono>
#include <cmath>
#include <sstream>

#include "solver/lp/standard_problem.hpp"
#include "solver/numerical/verification.hpp"

namespace solver {
namespace {

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

SolveStatus mapStatus(SimplexStatus s) {
  switch (s) {
    case SimplexStatus::Optimal: return SolveStatus::Optimal;
    case SimplexStatus::Infeasible: return SolveStatus::Infeasible;
    case SimplexStatus::Unbounded: return SolveStatus::Unbounded;
    case SimplexStatus::IterationLimit: return SolveStatus::IterationLimit;
    case SimplexStatus::TimeLimit: return SolveStatus::TimeLimit;
    case SimplexStatus::NumericalError: return SolveStatus::NumericalError;
  }
  return SolveStatus::Error;
}

void fillDimensions(LPResult& out, const OptimizationModel& model) {
  out.rows = model.numConstraints();
  out.cols = model.numVariables();
  out.nonzeros = model.numNonzeros();
}

}  // namespace

LPResult LPSolver::solve(const OptimizationModel& model, const LPConfig& config) {
  LPResult out;
  fillDimensions(out, model);

  const auto t_start = Clock::now();

  const ValidationResult validation = model.validate();
  if (validation.hasErrors()) {
    out.status = SolveStatus::Error;
    out.termination_reason = "model validation failed: " + validation.toString();
    out.solve_time_sec = secondsSince(t_start);
    return out;
  }
  for (const ValidationIssue& w : validation.warnings()) out.warnings.push_back(w.message);

  if (config.algorithm != LPAlgorithm::PrimalSimplex) {
    out.status = SolveStatus::Error;
    out.termination_reason = std::string("algorithm ") + toString(config.algorithm) +
                             " is not implemented in this build";
    out.solve_time_sec = secondsSince(t_start);
    return out;
  }

  PresolveConfig presolve_cfg = config.presolve;
  presolve_cfg.tol = config.simplex.tol;
  const auto t_presolve = Clock::now();
  PresolveResult presolved = presolveModel(model, presolve_cfg);
  out.presolve_time_sec = secondsSince(t_presolve);
  out.presolve_stats = presolved.stats;

  if (presolved.infeasible) {
    out.status = SolveStatus::Infeasible;
    out.termination_reason = "detected during presolve: " + presolved.infeasibility_reason;
    out.solve_time_sec = secondsSince(t_start);
    return out;
  }

  StandardizationResult std_result = standardize(presolved.reduced);
  if (!std_result.ok) {
    out.status = SolveStatus::Error;
    out.termination_reason = "standardization failed: " + std_result.message;
    out.solve_time_sec = secondsSince(t_start);
    return out;
  }

  SimplexSolver simplex(std_result.problem, config.simplex);
  const auto t_solve = Clock::now();
  SimplexSolution sol = simplex.solve();
  out.solve_time_sec = secondsSince(t_solve);
  out.iterations = sol.iterations;
  out.phase1_iterations = sol.phase1_iterations;
  out.refactorizations = sol.refactorizations;
  out.duality_gap = sol.gap;
  out.termination_reason = sol.message;
  out.status = mapStatus(sol.status);

  if (sol.feasible_point) {
    std::vector<double> x_reduced;
    std_result.expand(sol.x, x_reduced);
    out.x = presolved.restorePrimal(x_reduced);
    out.feasible_point = true;
    out.objective = model.evaluateObjective(out.x);

    std::vector<double> duals_reduced(presolved.reduced.numConstraints(), 0.0);
    for (int k = 0; k < presolved.reduced.numConstraints(); ++k) {
      const int std_row = std_result.problem.std_row_orig[static_cast<std::size_t>(k)];
      if (std_row < 0 || std_row >= static_cast<int>(sol.y.size())) continue;
      duals_reduced[static_cast<std::size_t>(k)] =
          std_result.problem.std_row_sign[static_cast<std::size_t>(std_row)] *
          sol.y[static_cast<std::size_t>(std_row)];
    }
    out.duals = presolved.mapDuals(duals_reduced);
    out.reduced_costs = computeReducedCosts(model, out.duals);
  }

  if (config.verify) {
    const auto t_verify = Clock::now();
    if (out.feasible_point) {
      const VerificationReport primal = verifyPrimal(model, out.x, config.simplex.tol);
      out.primal_violation = primal.max_primal_violation;

      if (out.status == SolveStatus::Optimal) {
        if (!primal.primal_ok) {
          out.status = SolveStatus::NumericalError;
          out.termination_reason = "optimality reported but original-space check failed: " +
                                   primal.detail;
        } else if (!out.duals.empty()) {
          const VerificationReport dual = verifyDual(model, out.x, out.duals, config.simplex.tol);
          out.dual_violation = dual.max_dual_violation;
          if (dual.dual_ok) {
            out.verified = true;
          } else {
            out.status = SolveStatus::Feasible;
            out.verified = false;
            out.warnings.push_back(
                "dual certificate failed verification in original space, status downgraded to "
                "FEASIBLE: " +
                dual.detail);
          }
        } else {
          out.status = SolveStatus::Feasible;
          out.warnings.push_back("no dual solution available to certify optimality");
        }
      } else if (out.status == SolveStatus::Feasible) {
        if (!primal.primal_ok) {
          out.status = SolveStatus::NumericalError;
          out.termination_reason = "feasible point failed original-space check: " + primal.detail;
        }
      }
    }
    out.verify_time_sec = secondsSince(t_verify);
  }

  return out;
}

}  // namespace solver
