#include "solver/lp/standard_problem.hpp"

#include <cmath>
#include <stdexcept>

#include "solver/model/model.hpp"

namespace solver {
namespace {

enum class Transform { Fixed, Shift, Reflect, Split };

Transform transformOf(const Variable& v) {
  bool lb_finite = std::isfinite(v.lb);
  bool ub_finite = std::isfinite(v.ub);
  if (lb_finite && ub_finite && v.lb == v.ub) return Transform::Fixed;
  if (!lb_finite && !ub_finite) return Transform::Split;
  if (lb_finite) return Transform::Shift;
  return Transform::Reflect;
}

}  // namespace

StandardizationResult standardize(const OptimizationModel& model) {
  StandardizationResult result;
  StandardLP& p = result.problem;
  p.original_sense = model.objectiveSense();
  const int m = model.numConstraints();
  const int n_orig = model.numVariables();
  p.m = m;
  p.fixed_value.assign(static_cast<std::size_t>(n_orig), 0.0);
  p.is_fixed.assign(static_cast<std::size_t>(n_orig), 0);

  std::vector<Transform> tr(static_cast<std::size_t>(n_orig));
  std::vector<int> col_of(static_cast<std::size_t>(n_orig), -1);
  std::vector<int> split_partner(static_cast<std::size_t>(n_orig), -1);
  double offset_objective = 0.0;

  for (int j = 0; j < n_orig; ++j) {
    const Variable& v = model.variable(j);
    if (std::isnan(v.lb) || std::isnan(v.ub)) {
      result.ok = false;
      result.message = "variable '" + v.name + "' has NaN bounds";
      return result;
    }
    if (v.lb == kInfinity || v.ub == -kInfinity) {
      result.ok = false;
      result.message = "variable '" + v.name + "' has an invalid infinite bound";
      return result;
    }
    if (v.lb > v.ub) {
      result.ok = false;
      result.message = "variable '" + v.name + "' has lower bound greater than upper bound";
      return result;
    }
    tr[static_cast<std::size_t>(j)] = transformOf(v);
    switch (tr[static_cast<std::size_t>(j)]) {
      case Transform::Fixed:
        p.fixed_value[static_cast<std::size_t>(j)] = v.lb;
        p.is_fixed[static_cast<std::size_t>(j)] = 1;
        offset_objective += model.objective()[static_cast<std::size_t>(j)] * v.lb;
        break;
      case Transform::Shift:
        col_of[static_cast<std::size_t>(j)] = p.structural_cols;
        ++p.structural_cols;
        offset_objective += model.objective()[static_cast<std::size_t>(j)] * v.lb;
        break;
      case Transform::Reflect:
        col_of[static_cast<std::size_t>(j)] = p.structural_cols;
        ++p.structural_cols;
        offset_objective += model.objective()[static_cast<std::size_t>(j)] * v.ub;
        break;
      case Transform::Split: {
        int first = p.structural_cols;
        p.structural_cols += 2;
        col_of[static_cast<std::size_t>(j)] = first;
        split_partner[static_cast<std::size_t>(j)] = first + 1;
        break;
      }
    }
  }

  p.cols.resize(static_cast<std::size_t>(p.structural_cols));
  for (int j = 0; j < n_orig; ++j) {
    const Variable& v = model.variable(j);
    const int col = col_of[static_cast<std::size_t>(j)];
    if (col < 0) continue;
    switch (tr[static_cast<std::size_t>(j)]) {
      case Transform::Shift: {
        StandardColumn& sc = p.cols[static_cast<std::size_t>(col)];
        sc.orig_var = j;
        sc.kind = ColumnKind::StructuralShifted;
        sc.sign = 1.0;
        sc.offset = v.lb;
        break;
      }
      case Transform::Reflect: {
        StandardColumn& sc = p.cols[static_cast<std::size_t>(col)];
        sc.orig_var = j;
        sc.kind = ColumnKind::StructuralReflected;
        sc.sign = -1.0;
        sc.offset = v.ub;
        break;
      }
      case Transform::Split: {
        StandardColumn& a = p.cols[static_cast<std::size_t>(col)];
        StandardColumn& b = p.cols[static_cast<std::size_t>(split_partner[static_cast<std::size_t>(j)])];
        a.orig_var = j;
        a.kind = ColumnKind::StructuralSplitPositive;
        a.sign = 1.0;
        a.offset = 0.0;
        b.orig_var = j;
        b.kind = ColumnKind::StructuralSplitNegative;
        b.sign = -1.0;
        b.offset = 0.0;
        break;
      }
      default:
        break;
    }
  }

  p.lb.assign(static_cast<std::size_t>(p.structural_cols), 0.0);
  p.ub.assign(static_cast<std::size_t>(p.structural_cols), kInfinity);
  for (int j = 0; j < n_orig; ++j) {
    const Variable& v = model.variable(j);
    const int col = col_of[static_cast<std::size_t>(j)];
    if (col < 0) continue;
    if (tr[static_cast<std::size_t>(j)] == Transform::Shift) {
      p.ub[static_cast<std::size_t>(col)] =
          std::isfinite(v.ub) ? (v.ub - v.lb) : kInfinity;
    }
  }

  const double obj_scale = model.objectiveSense() == ObjectiveSense::Maximize ? -1.0 : 1.0;
  p.c.assign(static_cast<std::size_t>(p.structural_cols), 0.0);
  for (int j = 0; j < n_orig; ++j) {
    const int col = col_of[static_cast<std::size_t>(j)];
    if (col < 0) continue;
    const double cj = model.objective()[static_cast<std::size_t>(j)];
    switch (tr[static_cast<std::size_t>(j)]) {
      case Transform::Shift:
        p.c[static_cast<std::size_t>(col)] = obj_scale * cj;
        break;
      case Transform::Reflect:
        p.c[static_cast<std::size_t>(col)] = obj_scale * (-cj);
        break;
      case Transform::Split:
        p.c[static_cast<std::size_t>(col)] = obj_scale * cj;
        p.c[static_cast<std::size_t>(split_partner[static_cast<std::size_t>(j)])] = obj_scale * (-cj);
        break;
      default:
        break;
    }
  }
  p.obj_offset = obj_scale * (offset_objective + model.objectiveConstant());

  std::vector<Triplet> triplets;
  p.b.assign(static_cast<std::size_t>(m), 0.0);
  p.std_row_orig.assign(static_cast<std::size_t>(m), -1);
  p.std_row_sign.assign(static_cast<std::size_t>(m), 1.0);

  std::vector<ConstraintSense> sense(static_cast<std::size_t>(m));
  for (int i = 0; i < m; ++i) {
    const Constraint& con = model.constraint(i);
    double bi = con.rhs;
    for (std::size_t k = 0; k < con.cols.size(); ++k) {
      const int j = con.cols[k];
      if (j < 0 || j >= n_orig) {
        result.ok = false;
        result.message = "constraint '" + con.name + "' references invalid variable index";
        return result;
      }
      const double a = con.vals[k];
      switch (tr[static_cast<std::size_t>(j)]) {
        case Transform::Fixed:
          bi -= a * p.fixed_value[static_cast<std::size_t>(j)];
          break;
        case Transform::Shift:
          triplets.push_back({i, col_of[static_cast<std::size_t>(j)], a});
          bi -= a * model.variable(j).lb;
          break;
        case Transform::Reflect:
          triplets.push_back({i, col_of[static_cast<std::size_t>(j)], -a});
          bi -= a * model.variable(j).ub;
          break;
        case Transform::Split:
          triplets.push_back({i, col_of[static_cast<std::size_t>(j)], a});
          triplets.push_back({i, split_partner[static_cast<std::size_t>(j)], -a});
          break;
      }
    }
    p.b[static_cast<std::size_t>(i)] = bi;
    p.std_row_orig[static_cast<std::size_t>(i)] = i;
    p.std_row_sign[static_cast<std::size_t>(i)] = 1.0;
    sense[static_cast<std::size_t>(i)] = con.sense;
  }

  for (int i = 0; i < m; ++i) {
    if (p.b[static_cast<std::size_t>(i)] < 0.0) {
      p.b[static_cast<std::size_t>(i)] = -p.b[static_cast<std::size_t>(i)];
      p.std_row_sign[static_cast<std::size_t>(i)] = -1.0;
      switch (sense[static_cast<std::size_t>(i)]) {
        case ConstraintSense::LessEqual: sense[static_cast<std::size_t>(i)] = ConstraintSense::GreaterEqual; break;
        case ConstraintSense::GreaterEqual: sense[static_cast<std::size_t>(i)] = ConstraintSense::LessEqual; break;
        default: break;
      }
      for (Triplet& t : triplets) {
        if (t.row == i) t.value = -t.value;
      }
    }
  }

  p.slack_col.assign(static_cast<std::size_t>(m), -1);
  p.artificial_col.assign(static_cast<std::size_t>(m), -1);
  int slack_count = 0;
  int artificial_count = 0;
  for (int i = 0; i < m; ++i) {
    if (sense[static_cast<std::size_t>(i)] != ConstraintSense::Equal) ++slack_count;
    if (sense[static_cast<std::size_t>(i)] != ConstraintSense::LessEqual) ++artificial_count;
  }

  const int slack_start = p.structural_cols;
  int slack_index = 0;
  for (int i = 0; i < m; ++i) {
    if (sense[static_cast<std::size_t>(i)] == ConstraintSense::Equal) continue;
    const int col = slack_start + slack_index;
    p.slack_col[static_cast<std::size_t>(i)] = col;
    StandardColumn sc;
    sc.orig_var = -1;
    sc.kind = ColumnKind::Slack;
    sc.sign = 1.0;
    sc.offset = 0.0;
    if (static_cast<int>(p.cols.size()) <= col) p.cols.resize(static_cast<std::size_t>(col) + 1);
    p.cols[static_cast<std::size_t>(col)] = sc;
    const double coeff = sense[static_cast<std::size_t>(i)] == ConstraintSense::LessEqual ? 1.0 : -1.0;
    triplets.push_back({i, col, coeff});
    ++slack_index;
  }

  const int artificial_start = slack_start + slack_count;
  p.first_artificial_col = artificial_count > 0 ? artificial_start : -1;
  int artificial_index = 0;
  for (int i = 0; i < m; ++i) {
    if (sense[static_cast<std::size_t>(i)] == ConstraintSense::LessEqual) continue;
    const int col = artificial_start + artificial_index;
    p.artificial_col[static_cast<std::size_t>(i)] = col;
    StandardColumn sc;
    sc.orig_var = -1;
    sc.kind = ColumnKind::Artificial;
    sc.sign = 1.0;
    sc.offset = 0.0;
    if (static_cast<int>(p.cols.size()) <= col) p.cols.resize(static_cast<std::size_t>(col) + 1);
    p.cols[static_cast<std::size_t>(col)] = sc;
    triplets.push_back({i, col, 1.0});
    ++artificial_index;
  }

  p.n = static_cast<int>(p.cols.size());
  p.lb.resize(static_cast<std::size_t>(p.n), 0.0);
  p.ub.resize(static_cast<std::size_t>(p.n), kInfinity);
  p.c.resize(static_cast<std::size_t>(p.n), 0.0);

  p.A = SparseMatrix::fromTriplets(m, p.n, std::move(triplets));
  return result;
}

}  // namespace solver
