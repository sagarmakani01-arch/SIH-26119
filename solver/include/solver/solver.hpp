#pragma once

#include <string>
#include <vector>

#include "solver/core/types.hpp"
#include "solver/lp/lp_solver.hpp"
#include "solver/model/model.hpp"

namespace solver {

enum class ProblemClass { Linear, MixedIntegerLinear, Quadratic };

inline const char* toString(ProblemClass c) {
  switch (c) {
    case ProblemClass::Linear: return "LP";
    case ProblemClass::MixedIntegerLinear: return "MILP";
    case ProblemClass::Quadratic: return "QP";
  }
  return "UNKNOWN";
}

struct SolverConfig {
  double time_limit_sec = 60.0;
  long max_iterations = 1000000;
  double mip_rel_gap = 1e-4;
  Tolerances tolerances;
  ComputeBackendKind backend = ComputeBackendKind::Auto;
  LPAlgorithm lp_algorithm = LPAlgorithm::PrimalSimplex;
  bool presolve = true;
  bool verify = true;
  int threads = 1;
  bool collect_statistics = true;
};

struct SolverStatistics {
  double solve_time_sec = 0.0;
  double presolve_time_sec = 0.0;
  double verify_time_sec = 0.0;
  long iterations = 0;
  long phase1_iterations = 0;
  long nodes = 0;
  int refactorizations = 0;
  int rows = 0;
  int cols = 0;
  int nonzeros = 0;
  int presolve_rows_removed = 0;
  int presolve_bounds_tightened = 0;
  int presolve_fixed_variables_removed = 0;
  std::string backend_used = "CPU";
  double memory_usage_mb = 0.0;
};

struct SolverResult {
  SolveStatus status = SolveStatus::NotSolved;
  ProblemClass problem_class = ProblemClass::Linear;
  double objective = 0.0;
  std::vector<double> x;
  std::vector<double> duals;
  std::vector<double> reduced_costs;
  std::vector<double> constraint_activities;
  bool feasible_point = false;
  bool verified = false;
  double primal_violation = 0.0;
  double dual_violation = 0.0;
  double duality_gap = kInfinity;
  SolverStatistics statistics;
  std::string termination_reason;
  std::vector<std::string> warnings;
};

ProblemClass classifyProblem(const OptimizationModel& model);

class Solver {
 public:
  explicit Solver(SolverConfig config = SolverConfig{});

  SolverResult solve(const OptimizationModel& model) const;

  const SolverConfig& config() const { return config_; }
  void setConfig(const SolverConfig& config) { config_ = config; }

 private:
  SolverConfig config_;
};

}  // namespace solver
