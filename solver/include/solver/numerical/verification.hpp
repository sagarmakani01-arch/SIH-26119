#pragma once

#include <string>
#include <vector>

#include "solver/model/model.hpp"
#include "solver/numerical/tolerances.hpp"

namespace solver {

struct VerificationReport {
  bool primal_ok = false;
  bool dual_ok = false;
  bool certified = false;
  double max_primal_violation = 0.0;
  double max_bound_violation = 0.0;
  double max_dual_violation = 0.0;
  double duality_gap = 0.0;
  std::string detail;
};

VerificationReport verifyPrimal(const OptimizationModel& model, const std::vector<double>& x,
                                const Tolerances& tol);

VerificationReport verifyDual(const OptimizationModel& model, const std::vector<double>& x,
                              const std::vector<double>& duals, const Tolerances& tol);

std::vector<double> computeReducedCosts(const OptimizationModel& model,
                                        const std::vector<double>& duals);

}  // namespace solver
