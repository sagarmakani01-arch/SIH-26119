#include "solver/model/validation.hpp"

#include <cmath>
#include <set>
#include <sstream>

namespace solver {
namespace {

void addIssue(ValidationResult& r, ValidationIssue::Severity sev, std::string code,
              std::string context, std::string message) {
  r.issues.push_back({sev, std::move(code), std::move(context), std::move(message)});
}

bool isBadNumber(double v) { return std::isnan(v); }

}  // namespace

ValidationResult validateModel(const OptimizationModel& model) {
  ValidationResult result;

  if (model.numVariables() == 0 && model.numConstraints() > 0) {
    addIssue(result, ValidationIssue::Severity::Error, "EMPTY_MODEL", "model",
             "model has constraints but no variables");
    return result;
  }

  for (int i = 0; i < model.numVariables(); ++i) {
    const Variable& v = model.variable(i);
    std::string ctx = v.name.empty() ? ("x" + std::to_string(i)) : v.name;

    if (isBadNumber(v.lb) || isBadNumber(v.ub)) {
      addIssue(result, ValidationIssue::Severity::Error, "NON_FINITE_BOUND", ctx,
               "variable bound is NaN");
    }
    if (!std::isfinite(v.lb) && v.lb != -kInfinity) {
      addIssue(result, ValidationIssue::Severity::Error, "NON_FINITE_BOUND", ctx,
               "lower bound is not a valid number");
    }
    if (v.lb > v.ub) {
      std::ostringstream os;
      os << "lower bound (" << v.lb << ") exceeds upper bound (" << v.ub << ")";
      addIssue(result, ValidationIssue::Severity::Error, "INVALID_BOUNDS", ctx, os.str());
    }
    if (isBadNumber(v.obj)) {
      addIssue(result, ValidationIssue::Severity::Error, "NON_FINITE_OBJECTIVE", ctx,
               "objective coefficient is NaN");
    }
    if (v.type == VarType::Binary) {
      if (v.lb < -1e-12 || v.ub > 1.0 + 1e-12 || v.lb > v.ub) {
        std::ostringstream os;
        os << "binary variable must satisfy 0 <= lb <= ub <= 1, got [" << v.lb << ", " << v.ub
           << "]";
        addIssue(result, ValidationIssue::Severity::Error, "BINARY_BOUND_VIOLATION", ctx, os.str());
      } else if (v.lb > 0.0 || v.ub < 1.0) {
        addIssue(result, ValidationIssue::Severity::Warning, "BINARY_BOUND_TIGHTENED", ctx,
                 "binary variable bounds are tighter than [0, 1]");
      }
    }
    if (v.type != VarType::Continuous && (!std::isfinite(v.lb) || !std::isfinite(v.ub))) {
      addIssue(result, ValidationIssue::Severity::Warning, "UNBOUNDED_INTEGER", ctx,
               "integer variable has infinite bounds; branch-and-bound will require finite bounds");
    }
    if (std::abs(v.obj) > 1e15) {
      addIssue(result, ValidationIssue::Severity::Warning, "LARGE_COEFFICIENT", ctx,
               "objective coefficient magnitude exceeds 1e15; numerical risk");
    }
  }

  std::set<std::string> var_names;
  for (int i = 0; i < model.numVariables(); ++i) {
    const std::string& n = model.variable(i).name;
    if (n.empty()) continue;
    if (!var_names.insert(n).second) {
      addIssue(result, ValidationIssue::Severity::Warning, "DUPLICATE_VARIABLE_NAME", n,
               "multiple variables share this name; lookups return the first match");
    }
  }

  std::set<int> used;
  std::set<std::string> con_names;
  for (int r = 0; r < model.numConstraints(); ++r) {
    const Constraint& c = model.constraint(r);
    std::string ctx = c.name.empty() ? ("c" + std::to_string(r)) : c.name;

    if (!c.name.empty() && !con_names.insert(c.name).second) {
      addIssue(result, ValidationIssue::Severity::Warning, "DUPLICATE_CONSTRAINT_NAME", ctx,
               "multiple constraints share this name; lookups return the first match");
    }
    if (isBadNumber(c.rhs)) {
      addIssue(result, ValidationIssue::Severity::Error, "NON_FINITE_RHS", ctx,
               "right-hand side is NaN");
    }
    if (c.cols.size() != c.vals.size()) {
      addIssue(result, ValidationIssue::Severity::Error, "INDEX_MISMATCH", ctx,
               "coefficient index/value arrays differ in length");
      continue;
    }
    if (c.cols.empty()) {
      std::ostringstream os;
      os << "constraint has no variables; it is " << toString(c.sense) << " " << c.rhs;
      addIssue(result, ValidationIssue::Severity::Warning, "EMPTY_ROW", ctx, os.str());
      continue;
    }
    for (std::size_t k = 0; k < c.cols.size(); ++k) {
      int col = c.cols[k];
      if (col < 0 || col >= model.numVariables()) {
        addIssue(result, ValidationIssue::Severity::Error, "INDEX_OUT_OF_RANGE", ctx,
                 "coefficient references variable index " + std::to_string(col));
        continue;
      }
      used.insert(col);
      if (isBadNumber(c.vals[k])) {
        addIssue(result, ValidationIssue::Severity::Error, "NON_FINITE_COEFFICIENT", ctx,
                 "coefficient is NaN");
      }
      if (std::isinf(c.vals[k])) {
        addIssue(result, ValidationIssue::Severity::Error, "NON_FINITE_COEFFICIENT", ctx,
                 "coefficient is infinite");
      }
      if (std::abs(c.vals[k]) > 1e15) {
        addIssue(result, ValidationIssue::Severity::Warning, "LARGE_COEFFICIENT", ctx,
                 "coefficient magnitude exceeds 1e15; numerical risk");
      }
    }
    std::set<int> local(c.cols.begin(), c.cols.end());
    if (local.size() != c.cols.size()) {
      addIssue(result, ValidationIssue::Severity::Warning, "DUPLICATE_ENTRY", ctx,
               "constraint repeats a variable index; entries are summed when assembled");
    }
  }

  bool any_objective = false;
  for (int i = 0; i < model.numVariables(); ++i) {
    if (model.objective()[static_cast<std::size_t>(i)] != 0.0) any_objective = true;
  }
  if (!any_objective && model.numVariables() > 0) {
    addIssue(result, ValidationIssue::Severity::Warning, "ZERO_OBJECTIVE", "objective",
             "objective has no non-zero coefficients; any feasible point is optimal");
  }

  for (int i = 0; i < model.numVariables(); ++i) {
    if (used.count(i)) continue;
    const Variable& v = model.variable(i);
    std::string ctx = v.name.empty() ? ("x" + std::to_string(i)) : v.name;
    addIssue(result, ValidationIssue::Severity::Warning, "UNUSED_VARIABLE", ctx,
             "variable does not appear in any constraint");
  }

  return result;
}

}  // namespace solver
