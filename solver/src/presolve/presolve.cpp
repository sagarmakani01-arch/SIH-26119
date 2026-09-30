#include "solver/presolve/presolve.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace solver {
namespace {

struct WorkVar {
  double lb = 0.0;
  double ub = 0.0;
  bool removed = false;
  double removed_value = 0.0;
  VarType type = VarType::Continuous;
};

struct WorkRow {
  int orig = -1;
  ConstraintSense sense = ConstraintSense::LessEqual;
  double rhs = 0.0;
  std::vector<int> cols;
  std::vector<double> vals;
  bool dropped = false;
  std::string drop_reason;
};

void removeColumnEntries(std::vector<WorkRow>& rows, int var, double value, double* rhs_delta) {
  for (WorkRow& row : rows) {
    if (row.dropped) continue;
    for (std::size_t k = 0; k < row.cols.size();) {
      if (row.cols[k] == var) {
        row.rhs -= row.vals[k] * value;
        if (rhs_delta) *rhs_delta += row.vals[k] * value;
        row.cols.erase(row.cols.begin() + static_cast<std::ptrdiff_t>(k));
        row.vals.erase(row.vals.begin() + static_cast<std::ptrdiff_t>(k));
      } else {
        ++k;
      }
    }
  }
}

void activityRange(const WorkRow& row, const std::vector<WorkVar>& vars, double& lo, double& hi) {
  lo = 0.0;
  hi = 0.0;
  for (std::size_t k = 0; k < row.cols.size(); ++k) {
    const WorkVar& v = vars[static_cast<std::size_t>(row.cols[k])];
    const double a = row.vals[k];
    if (a >= 0.0) {
      lo += a * v.lb;
      hi += a * v.ub;
    } else {
      lo += a * v.ub;
      hi += a * v.lb;
    }
  }
}

enum class TightenOutcome { NoChange, Tightened, Contradiction };

TightenOutcome singletonTighten(WorkRow& row, std::vector<WorkVar>& vars,
                                const Tolerances& tol) {
  if (row.cols.size() != 1) return TightenOutcome::NoChange;
  const int j = row.cols[0];
  const double a = row.vals[0];
  WorkVar& v = vars[static_cast<std::size_t>(j)];
  if (a == 0.0) return TightenOutcome::NoChange;

  double lo = v.lb;
  double hi = v.ub;
  const double bound = row.rhs / a;
  if (row.sense == ConstraintSense::LessEqual) {
    if (a > 0.0) hi = std::min(hi, bound);
    else lo = std::max(lo, bound);
  } else if (row.sense == ConstraintSense::GreaterEqual) {
    if (a > 0.0) lo = std::max(lo, bound);
    else hi = std::min(hi, bound);
  } else {
    if (a > 0.0) {
      lo = std::max(lo, bound);
      hi = std::min(hi, bound);
    } else {
      hi = std::min(hi, bound);
      lo = std::max(lo, bound);
    }
  }
  if (v.type != VarType::Continuous) {
    if (std::isfinite(lo)) lo = std::ceil(lo - tol.integrality);
    if (std::isfinite(hi)) hi = std::floor(hi + tol.integrality);
  }
  if (lo > hi + 1e-12) return TightenOutcome::Contradiction;
  const bool changed = (lo > v.lb + 1e-15) || (hi < v.ub - 1e-15);
  v.lb = lo;
  v.ub = hi;
  return changed ? TightenOutcome::Tightened : TightenOutcome::NoChange;
}

std::string rowSignature(const WorkRow& row) {
  std::vector<std::pair<int, double>> terms;
  terms.reserve(row.cols.size());
  for (std::size_t k = 0; k < row.cols.size(); ++k) terms.emplace_back(row.cols[k], row.vals[k]);
  std::sort(terms.begin(), terms.end());
  std::ostringstream os;
  os << static_cast<int>(row.sense) << '|' << row.rhs << '|';
  for (const auto& t : terms) os << t.first << ':' << t.second << ';';
  return os.str();
}

}  // namespace

std::vector<double> PresolveResult::restorePrimal(const std::vector<double>& x_reduced) const {
  std::vector<double> out(reduced_of_var.size(), 0.0);
  for (std::size_t j = 0; j < reduced_of_var.size(); ++j) {
    if (var_removed[j]) {
      out[j] = removed_value[j];
    } else {
      const int r = reduced_of_var[j];
      if (r >= 0 && r < static_cast<int>(x_reduced.size())) out[j] = x_reduced[static_cast<std::size_t>(r)];
    }
  }
  return out;
}

std::vector<double> PresolveResult::mapDuals(const std::vector<double>& duals_reduced) const {
  std::vector<double> out(row_removed.size(), 0.0);
  for (std::size_t r = 0; r < row_removed.size(); ++r) {
    if (row_removed[r]) out[r] = 0.0;
  }
  for (std::size_t k = 0; k < row_of_reduced.size(); ++k) {
    const int orig = row_of_reduced[k];
    if (orig >= 0 && orig < static_cast<int>(out.size()) && k < duals_reduced.size()) {
      out[static_cast<std::size_t>(orig)] = duals_reduced[k];
    }
  }
  return out;
}

int PresolveResult::reducedRowForOriginal(int orig_row) const {
  if (orig_row < 0 || orig_row >= static_cast<int>(row_removed.size())) return -1;
  if (row_removed[static_cast<std::size_t>(orig_row)]) return -1;
  for (std::size_t k = 0; k < row_of_reduced.size(); ++k) {
    if (row_of_reduced[k] == orig_row) return static_cast<int>(k);
  }
  return -1;
}

PresolveResult presolveModel(const OptimizationModel& model, const PresolveConfig& config) {
  PresolveResult result;
  const int n = model.numVariables();
  const int m = model.numConstraints();
  result.stats.vars_in = n;
  result.stats.rows_in = m;
  result.reduced_of_var.assign(static_cast<std::size_t>(n), -1);
  result.var_removed.assign(static_cast<std::size_t>(n), 0);
  result.removed_value.assign(static_cast<std::size_t>(n), 0.0);
  result.row_removed.assign(static_cast<std::size_t>(m), 0);

  if (!config.enabled) {
    result.reduced = model;
    result.var_of_reduced.resize(static_cast<std::size_t>(n));
    for (int j = 0; j < n; ++j) {
      result.var_of_reduced[static_cast<std::size_t>(j)] = j;
      result.reduced_of_var[static_cast<std::size_t>(j)] = j;
    }
    result.row_of_reduced.resize(static_cast<std::size_t>(m));
    for (int i = 0; i < m; ++i) result.row_of_reduced[static_cast<std::size_t>(i)] = i;
    result.stats.rows_out = m;
    result.stats.vars_out = n;
    return result;
  }

  std::vector<WorkVar> vars(static_cast<std::size_t>(n));
  for (int j = 0; j < n; ++j) {
    const Variable& v = model.variable(j);
    WorkVar& wv = vars[static_cast<std::size_t>(j)];
    wv.lb = v.lb;
    wv.ub = v.ub;
    wv.type = v.type;
    if (v.type != VarType::Continuous) {
      if (std::isfinite(wv.lb)) wv.lb = std::ceil(wv.lb - config.tol.integrality);
      if (std::isfinite(wv.ub)) wv.ub = std::floor(wv.ub + config.tol.integrality);
    }
  }

  std::vector<WorkRow> rows(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    const Constraint& c = model.constraint(i);
    rows[static_cast<std::size_t>(i)].orig = i;
    rows[static_cast<std::size_t>(i)].sense = c.sense;
    rows[static_cast<std::size_t>(i)].rhs = c.rhs;
    rows[static_cast<std::size_t>(i)].cols = c.cols;
    rows[static_cast<std::size_t>(i)].vals = c.vals;
  }

  std::vector<double> objective(static_cast<std::size_t>(n));
  for (int j = 0; j < n; ++j) objective[static_cast<std::size_t>(j)] = model.objective()[static_cast<std::size_t>(j)];
  double objective_constant = model.objectiveConstant();

  for (int j = 0; j < n; ++j) {
    WorkVar& v = vars[static_cast<std::size_t>(j)];
    if (std::isfinite(v.lb) && v.lb == v.ub) {
      if (v.type != VarType::Continuous &&
          std::fabs(v.lb - std::round(v.lb)) > config.tol.integrality) {
        result.infeasible = true;
        std::ostringstream os;
        os << "integer variable x" << j << " is pinned to non-integral value " << v.lb;
        result.infeasibility_reason = os.str();
        return result;
      }
      v.removed = true;
      v.removed_value = v.lb;
      result.var_removed[static_cast<std::size_t>(j)] = 1;
      result.removed_value[static_cast<std::size_t>(j)] = v.lb;
      objective_constant += objective[static_cast<std::size_t>(j)] * v.lb;
      removeColumnEntries(rows, j, v.lb, nullptr);
      ++result.stats.fixed_variables_removed;
    } else if (v.lb > v.ub) {
      result.infeasible = true;
      result.infeasibility_reason = "variable x" + std::to_string(j) +
                                    " has lower bound greater than upper bound";
      return result;
    }
  }

  bool changed = true;
  int pass = 0;
  while (changed && pass < config.max_passes) {
    changed = false;
    ++pass;

    for (WorkRow& row : rows) {
      if (row.dropped || !row.cols.empty()) continue;
      bool violated = false;
      if (row.sense == ConstraintSense::LessEqual)
        violated = 0.0 > row.rhs + config.tol.primal_feasibility;
      else if (row.sense == ConstraintSense::GreaterEqual)
        violated = 0.0 < row.rhs - config.tol.primal_feasibility;
      else violated = std::fabs(row.rhs) > config.tol.primal_feasibility;
      if (violated) {
        result.infeasible = true;
        std::ostringstream os;
        os << "constraint c" << row.orig << " has no variables and 0 " << toString(row.sense)
           << " " << row.rhs << " is unsatisfiable";
        result.infeasibility_reason = os.str();
        return result;
      }
      row.dropped = true;
      row.drop_reason = "empty";
      ++result.stats.rows_removed_empty;
      changed = true;
    }

    if (config.tighten_bounds) {
      for (WorkRow& row : rows) {
        if (row.dropped) continue;
        if (row.cols.size() == 1) {
          const int j = row.cols[0];
          WorkVar& v = vars[static_cast<std::size_t>(j)];
          const TightenOutcome outcome = singletonTighten(row, vars, config.tol);
          if (outcome == TightenOutcome::Contradiction) {
            result.infeasible = true;
            std::ostringstream os;
            os << "constraint c" << row.orig << " contradicts the bounds of x" << j;
            result.infeasibility_reason = os.str();
            return result;
          }
          if (outcome == TightenOutcome::Tightened) {
            ++result.stats.bounds_tightened;
            changed = true;
            if (v.lb > v.ub + config.tol.primal_feasibility) {
              result.infeasible = true;
              std::ostringstream os;
              os << "constraint c" << row.orig << " contradicts the bounds of x" << j;
              result.infeasibility_reason = os.str();
              return result;
            }
          }
        }
      }
    }

    if (config.remove_redundant_rows) {
      for (WorkRow& row : rows) {
        if (row.dropped || row.cols.empty()) continue;
        if (row.cols.size() == 1) continue;
        double lo = 0.0;
        double hi = 0.0;
        activityRange(row, vars, lo, hi);
        bool redundant = false;
        if (row.sense == ConstraintSense::LessEqual && hi <= row.rhs + config.tol.primal_feasibility)
          redundant = true;
        else if (row.sense == ConstraintSense::GreaterEqual &&
                 lo >= row.rhs - config.tol.primal_feasibility)
          redundant = true;
        else if (row.sense == ConstraintSense::Equal &&
                 std::fabs(hi - row.rhs) <= config.tol.primal_feasibility &&
                 std::fabs(lo - row.rhs) <= config.tol.primal_feasibility)
          redundant = true;
        if (redundant && !row.cols.empty()) {
          row.dropped = true;
          row.drop_reason = "redundant";
          ++result.stats.rows_removed_redundant;
          changed = true;
        }
      }
    }

    if (config.detect_duplicate_rows) {
      std::map<std::string, int> seen;
      for (WorkRow& row : rows) {
        if (row.dropped) continue;
        std::string sig = rowSignature(row);
        auto it = seen.find(sig);
        if (it == seen.end()) {
          seen.emplace(std::move(sig), row.orig);
        } else {
          row.dropped = true;
          row.drop_reason = "duplicate of c" + std::to_string(it->second);
          ++result.stats.rows_removed_duplicate;
          changed = true;
        }
      }
    }

    if (config.tighten_bounds) {
      for (int j = 0; j < n; ++j) {
        WorkVar& v = vars[static_cast<std::size_t>(j)];
        if (std::isfinite(v.lb) && v.lb == v.ub && !v.removed) {
          if (v.type != VarType::Continuous &&
              std::fabs(v.lb - std::round(v.lb)) > config.tol.integrality) {
            result.infeasible = true;
            std::ostringstream os;
            os << "integer variable x" << j << " is pinned to non-integral value " << v.lb;
            result.infeasibility_reason = os.str();
            return result;
          }
          v.removed = true;
          v.removed_value = v.lb;
          changed = true;
        }
      }
      for (int j = 0; j < n; ++j) {
        WorkVar& v = vars[static_cast<std::size_t>(j)];
        if (v.removed && !result.var_removed[static_cast<std::size_t>(j)]) {
          result.var_removed[static_cast<std::size_t>(j)] = 1;
          result.removed_value[static_cast<std::size_t>(j)] = v.removed_value;
          objective_constant += objective[static_cast<std::size_t>(j)] * v.removed_value;
          removeColumnEntries(rows, j, v.removed_value, nullptr);
          ++result.stats.fixed_variables_removed;
        }
      }
    }
  }
  result.stats.passes = pass;

  if (result.infeasible) return result;

  int reduced_vars = 0;
  for (int j = 0; j < n; ++j) {
    if (vars[static_cast<std::size_t>(j)].removed) {
      result.reduced_of_var[static_cast<std::size_t>(j)] = -1;
    } else {
      result.reduced_of_var[static_cast<std::size_t>(j)] = reduced_vars;
      result.var_of_reduced.push_back(j);
      ++reduced_vars;
    }
  }
  result.stats.vars_out = reduced_vars;

  result.reduced.setName(model.name());
  result.reduced.setObjectiveSense(model.objectiveSense());
  for (const auto& kv : model.meta()) result.reduced.setMeta(kv.first, kv.second);

  std::vector<int> new_index(static_cast<std::size_t>(n), -1);
  for (int j = 0; j < n; ++j) {
    if (vars[static_cast<std::size_t>(j)].removed) continue;
    const Variable& v = model.variable(j);
    const WorkVar& wv = vars[static_cast<std::size_t>(j)];
    const int nj = result.reduced.addVariable(v.name, v.type, wv.lb, wv.ub);
    new_index[static_cast<std::size_t>(j)] = nj;
    (void)nj;
  }

  LinearExpression obj_expr;
  obj_expr.constant(objective_constant);
  for (int j = 0; j < n; ++j) {
    if (vars[static_cast<std::size_t>(j)].removed) continue;
    const double cj = model.objective()[static_cast<std::size_t>(j)];
    if (cj != 0.0) obj_expr.add(new_index[static_cast<std::size_t>(j)], cj);
  }
  result.reduced.setObjective(obj_expr);

  for (WorkRow& row : rows) {
    if (row.dropped) {
      result.row_removed[static_cast<std::size_t>(row.orig)] = 1;
      continue;
    }
    std::vector<int> cols;
    std::vector<double> vals;
    for (std::size_t k = 0; k < row.cols.size(); ++k) {
      const int nj = new_index[static_cast<std::size_t>(row.cols[k])];
      if (nj < 0) continue;
      if (row.vals[k] == 0.0) continue;
      cols.push_back(nj);
      vals.push_back(row.vals[k]);
    }
    const Constraint& orig = model.constraint(row.orig);
    const int nr = result.reduced.addConstraint(orig.name, row.sense, row.rhs, std::move(cols),
                                                 std::move(vals));
    result.row_of_reduced.push_back(row.orig);
    (void)nr;
  }

  result.stats.rows_out = static_cast<int>(result.row_of_reduced.size());
  return result;
}

}  // namespace solver
