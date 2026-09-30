#include "solver/lp/standard_problem.hpp"
#include "solver/model/model.hpp"
#include "testing.hpp"

using namespace solver;

TEST(Standardize, BuildsSlacksForLessEqualRows) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 4.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 6.0);
  m.setObjective(LinearExpression().add(x, 3.0).add(y, 5.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 4.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, LinearExpression().add(y, 2.0), 12.0);
  m.addConstraint("c3", ConstraintSense::LessEqual,
                  LinearExpression().add(x, 3.0).add(y, 2.0), 18.0);

  StandardizationResult r = standardize(m);
  REQUIRE(r.ok);
  CHECK_EQ(r.problem.m, 3);
  CHECK_EQ(r.problem.n, 5);
  CHECK_EQ(r.problem.first_artificial_col, -1);
  CHECK_NEAR(r.problem.b[0], 4.0, 1e-12);
  CHECK_NEAR(r.problem.ub[0], 4.0, 1e-12);
  CHECK_NEAR(r.problem.ub[1], 6.0, 1e-12);
  CHECK(r.problem.slack_col[0] == 2);
  CHECK(r.problem.slack_col[2] == 4);
}

TEST(Standardize, MaximizeNegatesCosts) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x, 7.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 5.0);

  StandardizationResult r = standardize(m);
  REQUIRE(r.ok);
  CHECK_NEAR(r.problem.c[0], -7.0, 1e-12);
  CHECK_NEAR(r.problem.obj_offset, 0.0, 1e-12);
}

TEST(Standardize, ShiftsLowerBoundIntoRhs) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 3.0, 10.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 2.0), 8.0);

  StandardizationResult r = standardize(m);
  REQUIRE(r.ok);
  CHECK_NEAR(r.problem.b[0], 8.0 - 2.0 * 3.0, 1e-12);
  CHECK_NEAR(r.problem.lb[0], 0.0, 1e-12);
  CHECK_NEAR(r.problem.ub[0], 7.0, 1e-12);
  CHECK_NEAR(r.problem.obj_offset, 3.0, 1e-12);

  std::vector<double> x_orig;
  r.expand({4.0}, x_orig);
  REQUIRE(x_orig.size() == 1);
  CHECK_NEAR(x_orig[0], 7.0, 1e-12);
}

TEST(Standardize, ReflectsUpperBoundedFreeVariable) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, -kInfinity, 5.0);
  m.setObjective(LinearExpression().add(x, 2.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 4.0);

  StandardizationResult r = standardize(m);
  REQUIRE(r.ok);
  CHECK_NEAR(r.problem.obj_offset, 2.0 * 5.0, 1e-12);
  CHECK_NEAR(r.problem.c[0], -2.0, 1e-12);
  std::vector<double> x_orig;
  r.expand({1.0}, x_orig);
  REQUIRE(x_orig.size() == 1);
  CHECK_NEAR(x_orig[0], 4.0, 1e-12);
}

TEST(Standardize, SplitsFreeVariable) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, -kInfinity, kInfinity);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 2.0);

  StandardizationResult r = standardize(m);
  REQUIRE(r.ok);
  CHECK_EQ(r.problem.n, 4);
  std::vector<double> x_orig;
  r.expand({7.0, 3.0, 0.0, 0.0}, x_orig);
  REQUIRE(x_orig.size() == 1);
  CHECK_NEAR(x_orig[0], 4.0, 1e-12);
}

TEST(Standardize, SubstitutesFixedVariable) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 2.0, 2.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 10.0);
  m.setObjective(LinearExpression().add(x, 5.0).add(y, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 10.0);

  StandardizationResult r = standardize(m);
  REQUIRE(r.ok);
  CHECK(r.problem.is_fixed[0] == 1);
  CHECK_NEAR(r.problem.b[0], 8.0, 1e-12);
  CHECK_NEAR(r.problem.obj_offset, 10.0, 1e-12);
  std::vector<double> x_orig;
  r.expand({3.0, 0.0}, x_orig);
  REQUIRE(x_orig.size() == 2);
  CHECK_NEAR(x_orig[0], 2.0, 1e-12);
  CHECK_NEAR(x_orig[1], 3.0, 1e-12);
}

TEST(Standardize, NegatesRowsWithNegativeRhs) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, -kInfinity, kInfinity);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), -4.0);

  StandardizationResult r = standardize(m);
  REQUIRE(r.ok);
  CHECK_NEAR(r.problem.b[0], 4.0, 1e-12);
  CHECK_NEAR(r.problem.std_row_sign[0], -1.0, 1e-12);
  CHECK(r.problem.artificial_col[0] == -1);
  CHECK(r.problem.slack_col[0] >= 0);
}

TEST(Standardize, RejectsInvalidBounds) {
  OptimizationModel m;
  m.addVariable("bad", VarType::Continuous, 5.0, 1.0);
  StandardizationResult r = standardize(m);
  CHECK_FALSE(r.ok);
}
