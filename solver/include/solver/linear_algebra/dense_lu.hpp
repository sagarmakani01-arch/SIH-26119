#pragma once

#include <vector>

namespace solver {

class DenseLU {
 public:
  DenseLU() = default;

  bool factorize(const std::vector<double>& matrix, int n, double pivot_tolerance = 1e-12);
  bool solve(const std::vector<double>& rhs, std::vector<double>& solution) const;
  bool solveTranspose(const std::vector<double>& rhs, std::vector<double>& solution) const;
  bool invert(std::vector<double>& inverse) const;

  int size() const { return n_; }
  bool valid() const { return valid_; }
  double pivotMin() const { return pivot_min_; }
  int determinantSign() const { return det_sign_; }

 private:
  int n_ = 0;
  bool valid_ = false;
  double pivot_min_ = 0.0;
  int det_sign_ = 1;
  std::vector<double> lu_;
  std::vector<int> piv_;
};

}  // namespace solver
