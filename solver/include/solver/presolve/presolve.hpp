#pragma once

#include <string>
#include <vector>

#include "solver/model/model.hpp"
#include "solver/numerical/tolerances.hpp"

namespace solver {

struct PresolveConfig {
  bool enabled = true;
  int max_passes = 10;
  bool detect_duplicate_rows = true;
  bool tighten_bounds = true;
  bool remove_redundant_rows = true;
  Tolerances tol;
};

struct PresolveStatistics {
  int fixed_variables_removed = 0;
  int rows_removed_empty = 0;
  int rows_removed_redundant = 0;
  int rows_removed_duplicate = 0;
  int bounds_tightened = 0;
  int passes = 0;
  int rows_in = 0;
  int rows_out = 0;
  int vars_in = 0;
  int vars_out = 0;
};

struct PresolveResult {
  OptimizationModel reduced;
  bool infeasible = false;
  std::string infeasibility_reason;
  PresolveStatistics stats;

  std::vector<int> var_of_reduced;
  std::vector<int> reduced_of_var;
  std::vector<unsigned char> var_removed;
  std::vector<double> removed_value;
  std::vector<int> row_of_reduced;
  std::vector<unsigned char> row_removed;
  double objective_adjustment = 0.0;

  std::vector<double> restorePrimal(const std::vector<double>& x_reduced) const;
  std::vector<double> mapDuals(const std::vector<double>& duals_reduced) const;
  int reducedRowForOriginal(int orig_row) const;
};

PresolveResult presolveModel(const OptimizationModel& model, const PresolveConfig& config);

}  // namespace solver
