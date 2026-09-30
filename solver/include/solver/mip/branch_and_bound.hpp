#pragma once

#include <string>
#include <vector>

#include "solver/core/types.hpp"
#include "solver/lp/lp_solver.hpp"
#include "solver/model/model.hpp"

namespace solver {

struct MIPConfig {
  Tolerances tol;
  double mip_rel_gap = 1e-4;
  double time_limit_sec = 60.0;
  long max_nodes = 1000000;
  long max_lp_iterations = 10000000;
  bool presolve = true;
  bool verify = true;
};

struct MIPResult {
  SolveStatus status = SolveStatus::NotSolved;
  double objective = 0.0;
  std::vector<double> x;
  bool feasible_point = false;
  bool verified = false;
  double primal_violation = 0.0;
  double mip_gap = kInfinity;
  double best_bound = kNaNDouble;
  long nodes = 0;
  long lp_iterations = 0;
  int lp_relaxations = 0;
  PresolveStatistics presolve_stats;
  double presolve_time_sec = 0.0;
  double solve_time_sec = 0.0;
  double verify_time_sec = 0.0;
  std::string termination_reason;
  std::vector<std::string> warnings;
};

class BranchAndBound {
 public:
  MIPResult solve(const OptimizationModel& model, const MIPConfig& config = MIPConfig());
};

}  // namespace solver
