#pragma once

#include <string>
#include <vector>

#include "solver/core/types.hpp"
#include "solver/lp/simplex.hpp"
#include "solver/model/model.hpp"
#include "solver/presolve/presolve.hpp"

namespace solver {

enum class LPAlgorithm { PrimalSimplex, DualSimplex, InteriorPoint };

inline const char* toString(LPAlgorithm a) {
  switch (a) {
    case LPAlgorithm::PrimalSimplex: return "PRIMAL_SIMPLEX";
    case LPAlgorithm::DualSimplex: return "DUAL_SIMPLEX";
    case LPAlgorithm::InteriorPoint: return "INTERIOR_POINT";
  }
  return "UNKNOWN";
}

struct LPConfig {
  SimplexConfig simplex;
  PresolveConfig presolve;
  LPAlgorithm algorithm = LPAlgorithm::PrimalSimplex;
  bool verify = true;
};

struct LPResult {
  SolveStatus status = SolveStatus::NotSolved;
  double objective = 0.0;
  std::vector<double> x;
  std::vector<double> duals;
  std::vector<double> reduced_costs;
  bool feasible_point = false;
  bool verified = false;
  double primal_violation = 0.0;
  double dual_violation = 0.0;
  double duality_gap = kInfinity;
  long iterations = 0;
  long phase1_iterations = 0;
  int refactorizations = 0;
  int rows = 0;
  int cols = 0;
  int nonzeros = 0;
  PresolveStatistics presolve_stats;
  double presolve_time_sec = 0.0;
  double solve_time_sec = 0.0;
  double verify_time_sec = 0.0;
  std::string termination_reason;
  std::vector<std::string> warnings;
};

class LPSolver {
 public:
  LPResult solve(const OptimizationModel& model, const LPConfig& config = LPConfig());
};

}  // namespace solver
