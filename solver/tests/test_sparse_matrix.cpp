#include <stdexcept>

#include "solver/model/sparse_matrix.hpp"
#include "testing.hpp"

using solver::SparseMatrix;
using solver::Triplet;

TEST(SparseMatrix, ConstructsWithDimensions) {
  SparseMatrix m(3, 4);
  CHECK_EQ(m.rows(), 3);
  CHECK_EQ(m.cols(), 4);
  CHECK_EQ(m.nnz(), 0);
}

TEST(SparseMatrix, BuildsFromTriplets) {
  std::vector<Triplet> ts = {{0, 0, 2.0}, {1, 0, -1.0}, {0, 2, 5.0}};
  SparseMatrix m = SparseMatrix::fromTriplets(2, 3, ts);
  CHECK_EQ(m.rows(), 2);
  CHECK_EQ(m.cols(), 3);
  CHECK_EQ(m.nnz(), 3);
  CHECK_NEAR(m.coeff(0, 0), 2.0, 1e-15);
  CHECK_NEAR(m.coeff(1, 0), -1.0, 1e-15);
  CHECK_NEAR(m.coeff(0, 2), 5.0, 1e-15);
  CHECK_NEAR(m.coeff(1, 2), 0.0, 1e-15);
}

TEST(SparseMatrix, SumsDuplicateEntries) {
  std::vector<Triplet> ts = {{0, 0, 1.5}, {0, 0, 2.5}, {0, 0, -4.0}};
  SparseMatrix m = SparseMatrix::fromTriplets(1, 1, ts);
  CHECK_EQ(m.nnz(), 0);
  CHECK_NEAR(m.coeff(0, 0), 0.0, 1e-15);
}

TEST(SparseMatrix, RejectsOutOfRangeTriplets) {
  bool threw = false;
  try {
    SparseMatrix::fromTriplets(2, 2, {{5, 0, 1.0}});
  } catch (const std::out_of_range&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(SparseMatrix, MultipliesDenseVector) {
  std::vector<Triplet> ts = {{0, 0, 1.0}, {1, 0, 2.0}, {1, 1, 3.0}, {2, 2, 4.0}};
  SparseMatrix m = SparseMatrix::fromTriplets(3, 3, ts);
  std::vector<double> x = {1.0, 2.0, 3.0};
  std::vector<double> y = m.multiply(x);
  REQUIRE(y.size() == 3);
  CHECK_NEAR(y[0], 1.0, 1e-15);
  CHECK_NEAR(y[1], 8.0, 1e-15);
  CHECK_NEAR(y[2], 12.0, 1e-15);
}

TEST(SparseMatrix, MultipliesTranspose) {
  std::vector<Triplet> ts = {{0, 0, 1.0}, {1, 0, 2.0}, {1, 1, 3.0}};
  SparseMatrix m = SparseMatrix::fromTriplets(2, 2, ts);
  std::vector<double> y = {1.0, 1.0};
  std::vector<double> x = m.multiplyTranspose(y);
  REQUIRE(x.size() == 2);
  CHECK_NEAR(x[0], 3.0, 1e-15);
  CHECK_NEAR(x[1], 3.0, 1e-15);
}

TEST(SparseMatrix, TransposeRoundTrip) {
  std::vector<Triplet> ts = {{0, 1, 7.0}, {2, 0, -3.0}, {2, 3, 0.5}};
  SparseMatrix m = SparseMatrix::fromTriplets(3, 4, ts);
  SparseMatrix t = m.transpose();
  CHECK_EQ(t.rows(), 4);
  CHECK_EQ(t.cols(), 3);
  CHECK_EQ(t.nnz(), 3);
  CHECK_NEAR(t.coeff(1, 0), 7.0, 1e-15);
  CHECK_NEAR(t.coeff(0, 2), -3.0, 1e-15);
  CHECK_NEAR(t.coeff(3, 2), 0.5, 1e-15);
  SparseMatrix back = t.transpose();
  CHECK_NEAR(back.coeff(2, 3), 0.5, 1e-15);
}

TEST(SparseMatrix, DimensionMismatchThrows) {
  SparseMatrix m = SparseMatrix::fromTriplets(2, 3, {{0, 0, 1.0}});
  bool threw = false;
  try {
    m.multiply({1.0, 2.0});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(SparseMatrix, AbsMaxAndNorm) {
  SparseMatrix m = SparseMatrix::fromTriplets(2, 2, {{0, 0, -3.0}, {1, 1, 4.0}});
  CHECK_NEAR(m.absMax(), 4.0, 1e-15);
  CHECK_NEAR(m.frobeniusNorm(), 5.0, 1e-15);
}
