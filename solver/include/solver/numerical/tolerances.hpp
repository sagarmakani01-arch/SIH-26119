#pragma once

#include <cmath>

namespace solver {

struct Tolerances {
  double primal_feasibility = 1e-6;
  double dual_feasibility = 1e-6;
  double integrality = 1e-6;
  double optimality = 1e-8;
  double pivot = 1e-9;
  double zero = 1e-12;
  double numeric_refactor = 1e-9;

  bool isZero(double v) const { return std::fabs(v) <= zero; }
};

inline double scaledTolerance(double base, double scale) {
  double s = std::fabs(scale);
  if (s < 1.0) s = 1.0;
  return base * s;
}

}  // namespace solver
