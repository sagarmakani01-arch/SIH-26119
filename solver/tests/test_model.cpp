#include <cmath>
#include <stdexcept>

#include "solver/model/model.hpp"
#include "testing.hpp"

using namespace solver;

static OptimizationModel makeProductMix() {
  OptimizationModel m;
  m.setName("product_mix");
  int x1 = m.addVariable("x1", VarType::Continuous, 0.0, kInfinity);
  int x2 = m.addVariable("x2", VarType::Continuous, 0.0, kInfinity);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x1, 10.0).add(x2, 15.0));
  m.addConstraint("c1", ConstraintSense::LessEqual,
                  LinearExpression().add(x1, 2.0).add(x2, 3.0), 100.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, LinearExpression().add(x1, 1.0).add(x2, 1.0),
                  40.0);
  m.addConstraint("c3", ConstraintSense::LessEqual, LinearExpression().add(x1, 1.0), 18.0);
  return m;
}

TEST(Model, AddVariablesStoresBoundsAndTypes) {
  OptimizationModel m;
  int a = m.addVariable("x1", VarType::Continuous, -5.0, 7.5);
  int b = m.addVariable("x2", VarType::Integer, 0.0, 10.0);
  int c = m.addVariable("x3", VarType::Binary);
  CHECK_EQ(a, 0);
  CHECK_EQ(b, 1);
  CHECK_EQ(c, 2);
  CHECK_EQ(m.numVariables(), 3);
  CHECK_EQ(m.variable(0).lb, -5.0);
  CHECK_EQ(m.variable(0).ub, 7.5);
  CHECK(m.variable(1).type == VarType::Integer);
  CHECK(m.variable(2).type == VarType::Binary);
  CHECK_EQ(m.variable(2).lb, 0.0);
  CHECK_EQ(m.variable(2).ub, 1.0);
}

TEST(Model, ConstraintBuilderProducesCorrectMatrix) {
  OptimizationModel m = makeProductMix();
  CHECK_EQ(m.numConstraints(), 3);
  CHECK_EQ(m.numNonzeros(), 5);
  const SparseMatrix& A = m.matrix();
  CHECK_EQ(A.rows(), 3);
  CHECK_EQ(A.cols(), 2);
  CHECK_NEAR(A.coeff(0, 0), 2.0, 1e-15);
  CHECK_NEAR(A.coeff(0, 1), 3.0, 1e-15);
  CHECK_NEAR(A.coeff(1, 0), 1.0, 1e-15);
  CHECK_NEAR(A.coeff(1, 1), 1.0, 1e-15);
  CHECK_NEAR(A.coeff(2, 0), 1.0, 1e-15);
  CHECK_NEAR(A.coeff(2, 1), 0.0, 1e-15);
}

TEST(Model, RowCacheMatchesNativeRowStorage) {
  OptimizationModel m = makeProductMix();
  const std::vector<int>& starts = m.rowStarts();
  const std::vector<int>& cols = m.rowCols();
  const std::vector<double>& vals = m.rowVals();
  REQUIRE(starts.size() == 4);
  CHECK_EQ(starts[0], 0);
  CHECK_EQ(starts[3], 5);
  CHECK_EQ(cols.size(), 5);
  CHECK_NEAR(vals[0], 2.0, 1e-15);
  CHECK_NEAR(vals[1], 3.0, 1e-15);
}

TEST(Model, ObjectiveEvaluation) {
  OptimizationModel m = makeProductMix();
  CHECK(m.objectiveSense() == ObjectiveSense::Maximize);
  CHECK_NEAR(m.evaluateObjective({2.0, 3.0}), 65.0, 1e-12);
  CHECK_NEAR(m.evaluateObjective({0.0, 0.0}), 0.0, 1e-12);
}

TEST(Model, ObjectiveConstantOffset) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  m.setObjective(LinearExpression().add(x, 3.0).constant(7.0));
  CHECK_NEAR(m.objectiveConstant(), 7.0, 1e-15);
  CHECK_NEAR(m.evaluateObjective({2.0}), 13.0, 1e-12);
}

TEST(Model, RowActivities) {
  OptimizationModel m = makeProductMix();
  std::vector<double> act = m.rowActivities({10.0, 10.0});
  REQUIRE(act.size() == 3);
  CHECK_NEAR(act[0], 50.0, 1e-12);
  CHECK_NEAR(act[1], 20.0, 1e-12);
  CHECK_NEAR(act[2], 10.0, 1e-12);
}

TEST(Model, FindByName) {
  OptimizationModel m = makeProductMix();
  CHECK_EQ(m.findVariable("x2"), 1);
  CHECK_EQ(m.findVariable("missing"), -1);
  CHECK_EQ(m.findConstraint("c3"), 2);
  CHECK_EQ(m.findConstraint("nope"), -1);
}

TEST(Model, SetObjectiveReplacesCoefficients) {
  OptimizationModel m;
  int x = m.addVariable();
  int y = m.addVariable();
  m.setObjective(LinearExpression().add(x, 1.0).add(y, 2.0));
  m.setObjective(LinearExpression().add(y, 5.0));
  CHECK_NEAR(m.objective()[0], 0.0, 1e-15);
  CHECK_NEAR(m.objective()[1], 5.0, 1e-15);
}

TEST(Model, ExpressionMergeSumsDuplicates) {
  OptimizationModel m;
  int x = m.addVariable();
  int y = m.addVariable();
  LinearExpression e = LinearExpression().add(x, 1.0).add(y, 3.0).add(x, 4.0);
  e.merge();
  CHECK_EQ(e.size(), 2);
  m.setObjective(e);
  CHECK_NEAR(m.objective()[0], 5.0, 1e-15);
  CHECK_NEAR(m.objective()[1], 3.0, 1e-15);
}

TEST(Model, Statistics) {
  OptimizationModel m = makeProductMix();
  m.addVariable("bin", VarType::Binary);
  m.addVariable("int", VarType::Integer, 0.0, 5.0);
  ModelStatistics s = m.statistics();
  CHECK_EQ(s.num_variables, 4);
  CHECK_EQ(s.num_constraints, 3);
  CHECK_EQ(s.num_nonzeros, 5);
  CHECK_EQ(s.num_integer_variables, 2);
  CHECK_EQ(s.num_binary_variables, 1);
  CHECK(s.density > 0.0);
  CHECK(s.density <= 1.0);
}

TEST(Model, FeasibilityHelpers) {
  OptimizationModel m = makeProductMix();
  m.addVariable("bin", VarType::Binary);
  CHECK(m.isBoundFeasible({1.0, 2.0, 1.0}, 1e-9));
  CHECK_FALSE(m.isBoundFeasible({1.0, 2.0, 1.5}, 1e-9));
  CHECK_FALSE(m.isBoundFeasible({-1.0, 0.0, 0.0}, 1e-9));
  CHECK(m.isIntegralFeasible({1.5, 2.0, 1.0}, 1e-9));
  CHECK_FALSE(m.isIntegralFeasible({1.0, 2.0, 0.5}, 1e-9));
}

TEST(Model, MetadataRoundTrip) {
  OptimizationModel m;
  m.setMeta("source", "unit-test");
  m.setMeta("synthetic", "true");
  CHECK_EQ(m.getMeta("source"), "unit-test");
  CHECK_EQ(m.getMeta("missing", "fallback"), "fallback");
}

TEST(Model, InvalidIndexThrows) {
  OptimizationModel m;
  bool threw = false;
  try {
    m.setObjectiveCoefficient(3, 1.0);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(Model, MatrixInvalidatesAfterMutation) {
  OptimizationModel m = makeProductMix();
  CHECK_NEAR(m.matrix().coeff(0, 0), 2.0, 1e-15);
  m.addConstraint("c4", ConstraintSense::GreaterEqual, LinearExpression().add(1, 1.0), 5.0);
  CHECK_EQ(m.matrix().rows(), 4);
  CHECK_NEAR(m.matrix().coeff(3, 1), 1.0, 1e-15);
}
