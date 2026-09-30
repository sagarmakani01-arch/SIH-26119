#pragma once

#include <cstddef>
#include <vector>

namespace solver {

struct Triplet {
  int row = 0;
  int col = 0;
  double value = 0.0;
};

class SparseMatrix {
 public:
  SparseMatrix() = default;
  SparseMatrix(int rows, int cols);

  static SparseMatrix fromTriplets(int rows, int cols, std::vector<Triplet> triplets);

  int rows() const { return rows_; }
  int cols() const { return cols_; }
  int nnz() const { return static_cast<int>(row_idx_.size()); }

  const std::vector<int>& colPtr() const { return col_ptr_; }
  const std::vector<int>& rowIdx() const { return row_idx_; }
  const std::vector<double>& values() const { return values_; }

  bool empty() const { return nnz() == 0; }
  void clear();

  double coeff(int row, int col) const;
  std::vector<double> column(int col) const;
  std::vector<double> row(int row) const;

  std::vector<double> multiply(const std::vector<double>& x) const;
  std::vector<double> multiplyTranspose(const std::vector<double>& y) const;
  double dotColumn(int col, const std::vector<double>& y) const;

  SparseMatrix transpose() const;
  std::vector<Triplet> triplets() const;

  double absMax() const;
  double frobeniusNorm() const;

 private:
  int rows_ = 0;
  int cols_ = 0;
  std::vector<int> col_ptr_;
  std::vector<int> row_idx_;
  std::vector<double> values_;
};

}  // namespace solver
