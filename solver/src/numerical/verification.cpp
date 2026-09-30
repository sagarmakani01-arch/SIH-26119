#include "solver/numerical/verification.hpp"

#include <cmath>
#include <sstream>

namespace solver {

VerificationReport verifyPrimal(const OptimizationModel& model, const std::vector<double>& x,
                                const Tolerances& tol) {
  VerificationReport report;
  if (static_cast<int>(x.size()) != model.numVariables()) {
    report.detail = "solution dimension mismatch";
    return report;
  }
  double max_bound = 0.0;
  for (int j = 0; j < model.numVariables(); ++j) {
    const Variable& v = model.variable(j);
    const double xv = x[static_cast<std::size_t>(j)];
    if (!std::isfinite(xv)) {
      report.max_bound_violation = kInfinity;
      report.detail = "variable x" + std::to_string(j) + " is not finite";
      return report;
    }
    if (std::isfinite(v.lb)) max_bound = std::max(max_bound, v.lb - xv);
    if (std::isfinite(v.ub)) max_bound = std::max(max_bound, xv - v.ub);
  }
  max_bound = std::max(0.0, max_bound);
  report.max_bound_violation = max_bound;

  double max_row = 0.0;
  int worst_row = -1;
  for (int i = 0; i < model.numConstraints(); ++i) {
    const Constraint& c = model.constraint(i);
    const double act = model.rowActivity(i, x);
    double viol = 0.0;
    if (c.sense == ConstraintSense::LessEqual) viol = act - c.rhs;
    else if (c.sense == ConstraintSense::GreaterEqual) viol = c.rhs - act;
    else viol = std::fabs(act - c.rhs);
    if (viol > max_row) {
      max_row = viol;
      worst_row = i;
    }
  }
  max_row = std::max(0.0, max_row);
  report.max_primal_violation = std::max(max_bound, max_row);

  if (report.max_primal_violation > tol.primal_feasibility) {
    std::ostringstream os;
    os << "primal infeasibility " << report.max_primal_violation << " exceeds tolerance "
       << tol.primal_feasibility;
    if (worst_row >= 0) {
      os << "; worst constraint: " << model.constraint(worst_row).name
         << " (index " << worst_row << ")";
    }
    report.detail = os.str();
    report.primal_ok = false;
  } else {
    report.primal_ok = true;
  }
  return report;
}

VerificationReport verifyDual(const OptimizationModel& model, const std::vector<double>& x,
                              const std::vector<double>& duals, const Tolerances& tol) {
  VerificationReport report;
  if (static_cast<int>(duals.size()) != model.numConstraints() ||
      static_cast<int>(x.size()) != model.numVariables()) {
    report.detail = "dual/primal dimension mismatch";
    return report;
  }

  const double scale = model.objectiveSense() == ObjectiveSense::Maximize ? -1.0 : 1.0;
  double max_viol = 0.0;

  for (int i = 0; i < model.numConstraints(); ++i) {
    const Constraint& c = model.constraint(i);
    const double lam = duals[static_cast<std::size_t>(i)];
    if (!std::isfinite(lam)) {
      report.max_dual_violation = kInfinity;
      report.detail = "dual for constraint " + c.name + " is not finite";
      return report;
    }
    double viol = 0.0;
    if (c.sense == ConstraintSense::LessEqual) viol = std::max(0.0, lam);
    else if (c.sense == ConstraintSense::GreaterEqual) viol = std::max(0.0, -lam);
    max_viol = std::max(max_viol, viol);
  }

  std::vector<double> r(static_cast<std::size_t>(model.numVariables()), 0.0);
  for (int j = 0; j < model.numVariables(); ++j) {
    double grad = scale * model.objective()[static_cast<std::size_t>(j)];
    for (int i = 0; i < model.numConstraints(); ++i) {
      const Constraint& c = model.constraint(i);
      for (std::size_t k = 0; k < c.cols.size(); ++k) {
        if (c.cols[k] == j) {
          grad -= duals[static_cast<std::size_t>(i)] * c.vals[k];
        }
      }
    }
    r[static_cast<std::size_t>(j)] = grad;
  }

  for (int j = 0; j < model.numVariables(); ++j) {
    const Variable& v = model.variable(j);
    const double xv = x[static_cast<std::size_t>(j)];
    const double rr = r[static_cast<std::size_t>(j)];
    double grad_scale = 1.0 + std::fabs(scale * model.objective()[static_cast<std::size_t>(j)]);
    for (int i = 0; i < model.numConstraints(); ++i) {
      const Constraint& c = model.constraint(i);
      for (std::size_t k = 0; k < c.cols.size(); ++k) {
        if (c.cols[k] == j) {
          grad_scale += std::fabs(duals[static_cast<std::size_t>(i)] * c.vals[k]);
        }
      }
    }
    const double rtol = tol.dual_feasibility * grad_scale;
    const bool at_lower = std::isfinite(v.lb) && xv <= v.lb + tol.primal_feasibility;
    const bool at_upper = std::isfinite(v.ub) && xv >= v.ub - tol.primal_feasibility;
    const bool fixed = std::isfinite(v.lb) && std::isfinite(v.ub) && v.lb == v.ub;

    double viol = 0.0;
    if (fixed) {
      viol = 0.0;
    } else if (at_lower && !at_upper) {
      viol = std::max(0.0, -rr - rtol);
    } else if (at_upper && !at_lower) {
      viol = std::max(0.0, rr - rtol);
    } else {
      viol = std::fabs(rr) - rtol;
      viol = std::max(0.0, viol);
    }
    if (viol > max_viol) max_viol = viol;
  }

  double cost_scale = 1.0;
  for (int j = 0; j < model.numVariables(); ++j) {
    cost_scale += std::fabs(scale * model.objective()[static_cast<std::size_t>(j)]) *
                  (1.0 + std::fabs(x[static_cast<std::size_t>(j)]));
  }
  const double comp_tol = tol.dual_feasibility * cost_scale;
  double max_comp = 0.0;
  for (int i = 0; i < model.numConstraints(); ++i) {
    const Constraint& c = model.constraint(i);
    const double lam = duals[static_cast<std::size_t>(i)];
    const double act = model.rowActivity(i, x);
    double slack = 0.0;
    if (c.sense == ConstraintSense::LessEqual) slack = c.rhs - act;
    else if (c.sense == ConstraintSense::GreaterEqual) slack = act - c.rhs;
    else slack = 0.0;
    max_comp = std::max(max_comp, std::fabs(lam * slack));
  }
  if (max_comp > comp_tol) max_viol = std::max(max_viol, (max_comp - comp_tol) / cost_scale);

  report.max_dual_violation = max_viol;
  report.dual_ok = max_viol <= tol.dual_feasibility;
  if (!report.dual_ok) {
    std::ostringstream os;
    os << "dual feasibility violation " << max_viol << " exceeds tolerance " << tol.dual_feasibility;
    report.detail = os.str();
  }
  return report;
}

std::vector<double> computeReducedCosts(const OptimizationModel& model,
                                        const std::vector<double>& duals) {
  std::vector<double> r(static_cast<std::size_t>(model.numVariables()), 0.0);
  if (static_cast<int>(duals.size()) != model.numConstraints()) return r;
  const double scale = model.objectiveSense() == ObjectiveSense::Maximize ? -1.0 : 1.0;
  for (int j = 0; j < model.numVariables(); ++j) {
    double grad = scale * model.objective()[static_cast<std::size_t>(j)];
    for (int i = 0; i < model.numConstraints(); ++i) {
      const Constraint& c = model.constraint(i);
      for (std::size_t k = 0; k < c.cols.size(); ++k) {
        if (c.cols[k] == j) grad -= duals[static_cast<std::size_t>(i)] * c.vals[k];
      }
    }
    r[static_cast<std::size_t>(j)] = grad;
  }
  return r;
}

}  // namespace solver
