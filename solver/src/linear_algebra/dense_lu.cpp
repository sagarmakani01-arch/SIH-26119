#include "solver/linear_algebra/dense_lu.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace solver {

bool DenseLU::factorize(const std::vector<double>& matrix, int n, double pivot_tolerance) {
  valid_ = false;
  n_ = n;
  if (n <= 0) return false;
  if (static_cast<int>(matrix.size()) != n * n) return false;

  lu_ = matrix;
  piv_.resize(static_cast<std::size_t>(n));
  det_sign_ = 1;
  pivot_min_ = 0.0;

  for (int k = 0; k < n; ++k) {
    int pivot_row = k;
    double best = std::fabs(lu_[static_cast<std::size_t>(k * n + k)]);
    for (int i = k + 1; i < n; ++i) {
      double v = std::fabs(lu_[static_cast<std::size_t>(i * n + k)]);
      if (v > best) {
        best = v;
        pivot_row = i;
      }
    }
    if (best <= pivot_tolerance) {
      pivot_min_ = best;
      return false;
    }
    if (pivot_row != k) {
      for (int j = 0; j < n; ++j) {
        std::swap(lu_[static_cast<std::size_t>(k * n + j)], lu_[static_cast<std::size_t>(pivot_row * n + j)]);
      }
      piv_[static_cast<std::size_t>(k)] = pivot_row;
      det_sign_ = -det_sign_;
    } else {
      piv_[static_cast<std::size_t>(k)] = k;
    }

    double pivot = lu_[static_cast<std::size_t>(k * n + k)];
    pivot_min_ = (k == 0) ? std::fabs(pivot) : std::min(pivot_min_, std::fabs(pivot));

    for (int i = k + 1; i < n; ++i) {
      double factor = lu_[static_cast<std::size_t>(i * n + k)] / pivot;
      lu_[static_cast<std::size_t>(i * n + k)] = factor;
      if (factor == 0.0) continue;
      for (int j = k + 1; j < n; ++j) {
        lu_[static_cast<std::size_t>(i * n + j)] -= factor * lu_[static_cast<std::size_t>(k * n + j)];
      }
    }
  }

  valid_ = true;
  return true;
}

bool DenseLU::solve(const std::vector<double>& rhs, std::vector<double>& solution) const {
  if (!valid_ || static_cast<int>(rhs.size()) != n_) return false;
  solution = rhs;
  for (int k = 0; k < n_; ++k) {
    int p = piv_[static_cast<std::size_t>(k)];
    if (p != k) std::swap(solution[static_cast<std::size_t>(k)], solution[static_cast<std::size_t>(p)]);
  }
  for (int i = 1; i < n_; ++i) {
    double sum = solution[static_cast<std::size_t>(i)];
    for (int j = 0; j < i; ++j) {
      sum -= lu_[static_cast<std::size_t>(i * n_ + j)] * solution[static_cast<std::size_t>(j)];
    }
    solution[static_cast<std::size_t>(i)] = sum;
  }
  for (int i = n_ - 1; i >= 0; --i) {
    double sum = solution[static_cast<std::size_t>(i)];
    for (int j = i + 1; j < n_; ++j) {
      sum -= lu_[static_cast<std::size_t>(i * n_ + j)] * solution[static_cast<std::size_t>(j)];
    }
    solution[static_cast<std::size_t>(i)] = sum / lu_[static_cast<std::size_t>(i * n_ + i)];
  }
  return true;
}

bool DenseLU::solveTranspose(const std::vector<double>& rhs, std::vector<double>& solution) const {
  if (!valid_ || static_cast<int>(rhs.size()) != n_) return false;
  solution = rhs;
  for (int i = 0; i < n_; ++i) {
    double sum = solution[static_cast<std::size_t>(i)];
    for (int j = 0; j < i; ++j) {
      sum -= lu_[static_cast<std::size_t>(j * n_ + i)] * solution[static_cast<std::size_t>(j)];
    }
    solution[static_cast<std::size_t>(i)] = sum / lu_[static_cast<std::size_t>(i * n_ + i)];
  }
  for (int i = n_ - 1; i >= 0; --i) {
    double sum = solution[static_cast<std::size_t>(i)];
    for (int j = i + 1; j < n_; ++j) {
      sum -= lu_[static_cast<std::size_t>(j * n_ + i)] * solution[static_cast<std::size_t>(j)];
    }
    solution[static_cast<std::size_t>(i)] = sum;
  }
  for (int k = n_ - 1; k >= 0; --k) {
    int p = piv_[static_cast<std::size_t>(k)];
    if (p != k) std::swap(solution[static_cast<std::size_t>(k)], solution[static_cast<std::size_t>(p)]);
  }
  return true;
}

bool DenseLU::invert(std::vector<double>& inverse) const {
  if (!valid_) return false;
  inverse.assign(static_cast<std::size_t>(n_) * static_cast<std::size_t>(n_), 0.0);
  std::vector<double> e(static_cast<std::size_t>(n_), 0.0);
  std::vector<double> col(static_cast<std::size_t>(n_), 0.0);
  for (int j = 0; j < n_; ++j) {
    std::fill(e.begin(), e.end(), 0.0);
    e[static_cast<std::size_t>(j)] = 1.0;
    if (!solve(e, col)) return false;
    for (int i = 0; i < n_; ++i) {
      inverse[static_cast<std::size_t>(i * n_ + j)] = col[static_cast<std::size_t>(i)];
    }
  }
  return true;
}

}  // namespace solver
