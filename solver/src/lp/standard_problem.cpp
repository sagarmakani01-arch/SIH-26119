#include "solver/lp/standard_problem.hpp"

namespace solver {

void StandardizationResult::expand(const std::vector<double>& x_std,
                                   std::vector<double>& x_out) const {
  x_out.assign(problem.fixed_value.size(), 0.0);
  for (std::size_t j = 0; j < problem.fixed_value.size(); ++j) {
    if (problem.is_fixed[j]) x_out[j] = problem.fixed_value[j];
  }
  for (int col = 0; col < problem.n; ++col) {
    const StandardColumn& sc = problem.cols[static_cast<std::size_t>(col)];
    if (sc.orig_var < 0) continue;
    x_out[static_cast<std::size_t>(sc.orig_var)] +=
        sc.sign * x_std[static_cast<std::size_t>(col)] + sc.offset;
  }
}

}  // namespace solver
