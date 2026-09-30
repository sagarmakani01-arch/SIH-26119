#include <cmath>
#include <vector>

#include "solver/linear_algebra/dense_lu.hpp"
#include "testing.hpp"

using solver::DenseLU;

TEST(DenseLU, SolvesDiagonalSystem) {
  std::vector<double> a = {2.0, 0.0, 0.0, 3.0};
  DenseLU lu;
  CHECK(lu.factorize(a, 2));
  std::vector<double> x;
  CHECK(lu.solve({6.0, 9.0}, x));
  REQUIRE(x.size() == 2);
  CHECK_NEAR(x[0], 3.0, 1e-12);
  CHECK_NEAR(x[1], 3.0, 1e-12);
}

TEST(DenseLU, SolvesPermutedSystem) {
  std::vector<double> a = {0.0, 1.0, 2.0, 3.0};
  DenseLU lu;
  CHECK(lu.factorize(a, 2));
  std::vector<double> x;
  CHECK(lu.solve({5.0, 16.0}, x));
  REQUIRE(x.size() == 2);
  CHECK_NEAR(x[0], 0.5, 1e-12);
  CHECK_NEAR(x[1], 5.0, 1e-12);
}

TEST(DenseLU, SolvesLargerSystem) {
  const int n = 5;
  std::vector<double> a(n * n, 0.0);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      a[static_cast<std::size_t>(i * n + j)] = (i == j) ? 4.0 : 1.0;
    }
  }
  std::vector<double> x_true = {1.0, -2.0, 3.0, 0.5, 7.0};
  std::vector<double> rhs(n, 0.0);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      rhs[static_cast<std::size_t>(i)] +=
          a[static_cast<std::size_t>(i * n + j)] * x_true[static_cast<std::size_t>(j)];
    }
  }
  DenseLU lu;
  CHECK(lu.factorize(a, n));
  std::vector<double> x;
  CHECK(lu.solve(rhs, x));
  for (int i = 0; i < n; ++i) CHECK_NEAR(x[static_cast<std::size_t>(i)], x_true[static_cast<std::size_t>(i)], 1e-10);
}

TEST(DenseLU, SolveTransposeAgreesWithDirect) {
  std::vector<double> a = {4.0, 1.0, 2.0, 3.0, 0.5, 1.5, 0.0, 2.0, 1.0};
  DenseLU lu;
  CHECK(lu.factorize(a, 3));
  std::vector<double> rhs = {1.0, 2.0, 3.0};
  std::vector<double> xt;
  CHECK(lu.solveTranspose(rhs, xt));
  for (int i = 0; i < 3; ++i) {
    double s = 0.0;
    for (int j = 0; j < 3; ++j) {
      s += a[static_cast<std::size_t>(j * 3 + i)] * xt[static_cast<std::size_t>(j)];
    }
    CHECK_NEAR(s, rhs[static_cast<std::size_t>(i)], 1e-10);
  }
}

TEST(DenseLU, InverseRoundTrip) {
  std::vector<double> a = {1.0, 2.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 3.0};
  DenseLU lu;
  CHECK(lu.factorize(a, 3));
  std::vector<double> inv;
  CHECK(lu.invert(inv));
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      double s = 0.0;
      for (int k = 0; k < 3; ++k) {
        s += a[static_cast<std::size_t>(i * 3 + k)] * inv[static_cast<std::size_t>(k * 3 + j)];
      }
      CHECK_NEAR(s, i == j ? 1.0 : 0.0, 1e-10);
    }
  }
}

TEST(DenseLU, RejectsSingularMatrix) {
  std::vector<double> a = {1.0, 2.0, 2.0, 4.0};
  DenseLU lu;
  CHECK_FALSE(lu.factorize(a, 2));
  CHECK_FALSE(lu.valid());
}
