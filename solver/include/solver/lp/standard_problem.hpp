#pragma once

#include <string>
#include <vector>

#include "solver/core/types.hpp"
#include "solver/model/sparse_matrix.hpp"

namespace solver {

class OptimizationModel;

enum class ColumnKind {
  StructuralShifted,
  StructuralReflected,
  StructuralSplitPositive,
  StructuralSplitNegative,
  Slack,
  Artificial
};

struct StandardColumn {
  int orig_var = -1;
  ColumnKind kind = ColumnKind::StructuralShifted;
  double sign = 1.0;
  double offset = 0.0;
};

struct StandardLP {
  int m = 0;
  int n = 0;
  SparseMatrix A;
  std::vector<double> c;
  std::vector<double> b;
  std::vector<double> lb;
  std::vector<double> ub;
  std::vector<StandardColumn> cols;

  int first_artificial_col = -1;
  int structural_cols = 0;
  std::vector<int> slack_col;
  std::vector<int> artificial_col;
  std::vector<int> std_row_orig;
  std::vector<double> std_row_sign;
  std::vector<double> fixed_value;
  std::vector<unsigned char> is_fixed;
  double obj_offset = 0.0;
  ObjectiveSense original_sense = ObjectiveSense::Minimize;

  bool isArtificial(int col) const {
    return cols[static_cast<std::size_t>(col)].kind == ColumnKind::Artificial;
  }
  bool isNonArtificial(int col) const {
    return first_artificial_col < 0 || col < first_artificial_col;
  }

  double toOriginalObjective(double std_value) const {
    const double v = std_value + obj_offset;
    return original_sense == ObjectiveSense::Maximize ? -v : v;
  }
};

struct StandardizationResult {
  StandardLP problem;
  bool ok = true;
  std::string message;

  void expand(const std::vector<double>& x_std, std::vector<double>& x_out) const;
};

StandardizationResult standardize(const OptimizationModel& model);

}  // namespace solver
