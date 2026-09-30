#include "solver/model/sparse_matrix.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace solver {

SparseMatrix::SparseMatrix(int rows, int cols)
    : rows_(rows), cols_(cols), col_ptr_(static_cast<std::size_t>(cols) + 1, 0) {
  if (rows < 0 || cols < 0) {
    throw std::invalid_argument("SparseMatrix: negative dimension");
  }
}

SparseMatrix SparseMatrix::fromTriplets(int rows, int cols, std::vector<Triplet> triplets) {
  if (rows < 0 || cols < 0) {
    throw std::invalid_argument("SparseMatrix::fromTriplets: negative dimension");
  }
  triplets.erase(std::remove_if(triplets.begin(), triplets.end(),
                                [](const Triplet& t) { return t.value == 0.0; }),
                 triplets.end());
  for (const Triplet& t : triplets) {
    if (t.row < 0 || t.row >= rows || t.col < 0 || t.col >= cols) {
      throw std::out_of_range("SparseMatrix::fromTriplets: index out of range");
    }
  }
  std::stable_sort(triplets.begin(), triplets.end(), [](const Triplet& a, const Triplet& b) {
    if (a.col != b.col) return a.col < b.col;
    return a.row < b.row;
  });

  SparseMatrix m(rows, cols);
  m.col_ptr_.assign(static_cast<std::size_t>(cols) + 1, 0);
  m.row_idx_.reserve(triplets.size());
  m.values_.reserve(triplets.size());

  std::size_t i = 0;
  for (int c = 0; c < cols; ++c) {
    m.col_ptr_[static_cast<std::size_t>(c)] = static_cast<int>(m.values_.size());
    while (i < triplets.size() && triplets[i].col == c) {
      int r = triplets[i].row;
      double acc = 0.0;
      while (i < triplets.size() && triplets[i].col == c && triplets[i].row == r) {
        acc += triplets[i].value;
        ++i;
      }
      if (acc != 0.0) {
        m.row_idx_.push_back(r);
        m.values_.push_back(acc);
      }
    }
  }
  m.col_ptr_[static_cast<std::size_t>(cols)] = static_cast<int>(m.values_.size());
  return m;
}

void SparseMatrix::clear() {
  rows_ = 0;
  cols_ = 0;
  col_ptr_.assign(1, 0);
  row_idx_.clear();
  values_.clear();
}

double SparseMatrix::coeff(int row, int col) const {
  if (row < 0 || row >= rows_ || col < 0 || col >= cols_) return 0.0;
  int start = col_ptr_[static_cast<std::size_t>(col)];
  int end = col_ptr_[static_cast<std::size_t>(col) + 1];
  while (start < end) {
    int mid = start + (end - start) / 2;
    int r = row_idx_[static_cast<std::size_t>(mid)];
    if (r == row) return values_[static_cast<std::size_t>(mid)];
    if (r < row) start = mid + 1;
    else end = mid;
  }
  return 0.0;
}

std::vector<double> SparseMatrix::column(int col) const {
  std::vector<double> out(static_cast<std::size_t>(rows_), 0.0);
  if (col < 0 || col >= cols_) return out;
  int start = col_ptr_[static_cast<std::size_t>(col)];
  int end = col_ptr_[static_cast<std::size_t>(col) + 1];
  for (int k = start; k < end; ++k) {
    out[static_cast<std::size_t>(row_idx_[static_cast<std::size_t>(k)])] =
        values_[static_cast<std::size_t>(k)];
  }
  return out;
}

std::vector<double> SparseMatrix::row(int row) const {
  std::vector<double> out(static_cast<std::size_t>(cols_), 0.0);
  if (row < 0 || row >= rows_) return out;
  for (int c = 0; c < cols_; ++c) {
    int start = col_ptr_[static_cast<std::size_t>(c)];
    int end = col_ptr_[static_cast<std::size_t>(c) + 1];
    for (int k = start; k < end; ++k) {
      if (row_idx_[static_cast<std::size_t>(k)] == row) {
        out[static_cast<std::size_t>(c)] = values_[static_cast<std::size_t>(k)];
        break;
      }
    }
  }
  return out;
}

std::vector<double> SparseMatrix::multiply(const std::vector<double>& x) const {
  if (static_cast<int>(x.size()) != cols_) {
    throw std::invalid_argument("SparseMatrix::multiply: dimension mismatch");
  }
  std::vector<double> y(static_cast<std::size_t>(rows_), 0.0);
  for (int c = 0; c < cols_; ++c) {
    double xc = x[static_cast<std::size_t>(c)];
    if (xc == 0.0) continue;
    int start = col_ptr_[static_cast<std::size_t>(c)];
    int end = col_ptr_[static_cast<std::size_t>(c) + 1];
    for (int k = start; k < end; ++k) {
      y[static_cast<std::size_t>(row_idx_[static_cast<std::size_t>(k)])] +=
          values_[static_cast<std::size_t>(k)] * xc;
    }
  }
  return y;
}

std::vector<double> SparseMatrix::multiplyTranspose(const std::vector<double>& y) const {
  if (static_cast<int>(y.size()) != rows_) {
    throw std::invalid_argument("SparseMatrix::multiplyTranspose: dimension mismatch");
  }
  std::vector<double> x(static_cast<std::size_t>(cols_), 0.0);
  for (int c = 0; c < cols_; ++c) {
    double acc = 0.0;
    int start = col_ptr_[static_cast<std::size_t>(c)];
    int end = col_ptr_[static_cast<std::size_t>(c) + 1];
    for (int k = start; k < end; ++k) {
      acc += values_[static_cast<std::size_t>(k)] *
             y[static_cast<std::size_t>(row_idx_[static_cast<std::size_t>(k)])];
    }
    x[static_cast<std::size_t>(c)] = acc;
  }
  return x;
}

double SparseMatrix::dotColumn(int col, const std::vector<double>& y) const {
  double acc = 0.0;
  int start = col_ptr_[static_cast<std::size_t>(col)];
  int end = col_ptr_[static_cast<std::size_t>(col) + 1];
  for (int k = start; k < end; ++k) {
    acc += values_[static_cast<std::size_t>(k)] *
           y[static_cast<std::size_t>(row_idx_[static_cast<std::size_t>(k)])];
  }
  return acc;
}

SparseMatrix SparseMatrix::transpose() const {
  std::vector<Triplet> ts;
  ts.reserve(static_cast<std::size_t>(nnz()));
  for (int c = 0; c < cols_; ++c) {
    int start = col_ptr_[static_cast<std::size_t>(c)];
    int end = col_ptr_[static_cast<std::size_t>(c) + 1];
    for (int k = start; k < end; ++k) {
      ts.push_back({c, row_idx_[static_cast<std::size_t>(k)], values_[static_cast<std::size_t>(k)]});
    }
  }
  return fromTriplets(cols_, rows_, std::move(ts));
}

std::vector<Triplet> SparseMatrix::triplets() const {
  std::vector<Triplet> ts;
  ts.reserve(static_cast<std::size_t>(nnz()));
  for (int c = 0; c < cols_; ++c) {
    int start = col_ptr_[static_cast<std::size_t>(c)];
    int end = col_ptr_[static_cast<std::size_t>(c) + 1];
    for (int k = start; k < end; ++k) {
      ts.push_back({row_idx_[static_cast<std::size_t>(k)], c, values_[static_cast<std::size_t>(k)]});
    }
  }
  return ts;
}

double SparseMatrix::absMax() const {
  double m = 0.0;
  for (double v : values_) m = std::max(m, std::abs(v));
  return m;
}

double SparseMatrix::frobeniusNorm() const {
  double s = 0.0;
  for (double v : values_) s += v * v;
  return std::sqrt(s);
}

}  // namespace solver
