#include <cmath>

#include "solver/model/model.hpp"
#include "solver/presolve/presolve.hpp"
#include "testing.hpp"

using namespace solver;

TEST(Presolve, RemovesFixedVariablesAndRestoresThem) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  int f = m.addVariable("f", VarType::Continuous, 5.0, 5.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 10.0);
  m.setObjective(LinearExpression().add(x, 2.0).add(f, 7.0).add(y, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0).add(f, 3.0), 20.0);
  m.addConstraint("c2", ConstraintSense::GreaterEqual, LinearExpression().add(y, 1.0).add(f, 1.0), 4.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(!r.infeasible);
  CHECK_EQ(r.stats.fixed_variables_removed, 1);
  CHECK_EQ(r.reduced.numVariables(), 2);
  CHECK_EQ(r.reduced.numConstraints(), 2);
  CHECK_EQ(r.reduced_of_var[static_cast<std::size_t>(f)], -1);
  CHECK_NEAR(r.reduced_of_var[static_cast<std::size_t>(x)], 0, 1e-12);
  CHECK_NEAR(r.reduced_of_var[static_cast<std::size_t>(y)], 1, 1e-12);

  CHECK_NEAR(r.reduced.constraint(0).rhs, 20.0 - 15.0, 1e-12);
  CHECK_NEAR(r.reduced.constraint(1).rhs, 4.0 - 5.0, 1e-12);
  CHECK_NEAR(r.reduced.objectiveConstant(), 35.0, 1e-12);
  CHECK_NEAR(r.reduced.objective()[0], 2.0, 1e-12);
  CHECK_NEAR(r.reduced.objective()[1], 1.0, 1e-12);

  std::vector<double> x_red = {3.0, 9.0};
  std::vector<double> x_full = r.restorePrimal(x_red);
  CHECK_EQ(static_cast<int>(x_full.size()), 3);
  CHECK_NEAR(x_full[static_cast<std::size_t>(f)], 5.0, 1e-12);
  CHECK_NEAR(x_full[static_cast<std::size_t>(x)], 3.0, 1e-12);
  CHECK_NEAR(x_full[static_cast<std::size_t>(y)], 9.0, 1e-12);

  std::vector<double> dual_red = {0.4, -0.2};
  std::vector<double> dual_full = r.mapDuals(dual_red);
  CHECK_EQ(static_cast<int>(dual_full.size()), 2);
  CHECK_NEAR(dual_full[0], 0.4, 1e-12);
  CHECK_NEAR(dual_full[1], -0.2, 1e-12);
}

TEST(Presolve, DetectsEmptyRowInfeasibility) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  m.addConstraint("bad", ConstraintSense::LessEqual, LinearExpression().add(x, 0.0), -1.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(r.infeasible);
  CHECK(!r.infeasibility_reason.empty());
}

TEST(Presolve, DetectsSatisfiedEmptyRow) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  m.addConstraint("ok", ConstraintSense::LessEqual, LinearExpression().add(x, 0.0), 5.0);
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 4.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(!r.infeasible);
  CHECK_EQ(r.stats.rows_removed_empty, 1);
  CHECK_EQ(r.reduced.numConstraints(), 1);
  CHECK_EQ(r.row_removed[0], 1);
  CHECK_EQ(r.row_of_reduced[0], 1);
}

TEST(Presolve, TightensBoundsFromSingletonRows) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 100.0);
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 30.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(!r.infeasible);
  CHECK(r.stats.bounds_tightened > 0);
  CHECK_EQ(r.reduced.numConstraints(), 1);
  CHECK_NEAR(r.reduced.variable(0).lb, 30.0, 1e-12);
}

TEST(Presolve, DetectsSingletonContradictingBounds) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 5.0);
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 10.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(r.infeasible);
  CHECK(!r.infeasibility_reason.empty());
}

TEST(Presolve, DropsRedundantRow) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 10.0, 20.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 5.0);
  m.addConstraint("weak", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 10.0);
  m.addConstraint("real", ConstraintSense::LessEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 20.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(!r.infeasible);
  CHECK_EQ(r.stats.rows_removed_redundant, 1);
  CHECK_EQ(r.reduced.numConstraints(), 1);
  CHECK_EQ(r.row_removed[0], 1);
  CHECK_EQ(r.row_removed[1], 0);
}

TEST(Presolve, DropsDuplicateRows) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 20.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 20.0);
  m.addConstraint("a", ConstraintSense::LessEqual,
                  LinearExpression().add(x, 2.0).add(y, 1.0), 8.0);
  m.addConstraint("b", ConstraintSense::LessEqual,
                  LinearExpression().add(x, 2.0).add(y, 1.0), 8.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(!r.infeasible);
  CHECK_EQ(r.stats.rows_removed_duplicate, 1);
  CHECK_EQ(r.reduced.numConstraints(), 1);
  CHECK_EQ(r.row_removed[1], 1);
}

TEST(Presolve, DisabledReturnsOriginalModel) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 5.0, 5.0);
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 9.0);

  PresolveConfig cfg;
  cfg.enabled = false;
  PresolveResult r = presolveModel(m, cfg);
  CHECK(!r.infeasible);
  CHECK_EQ(r.reduced.numVariables(), 1);
  CHECK_EQ(r.reduced.numConstraints(), 1);
  CHECK_EQ(r.stats.fixed_variables_removed, 0);
  std::vector<double> full = r.restorePrimal({7.0});
  CHECK_NEAR(full[0], 7.0, 1e-12);
}

TEST(Presolve, RoundsIntegerBounds) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Integer, 0.0, 10.0);
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 0.5);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(!r.infeasible);
  CHECK_NEAR(r.reduced.variable(0).lb, 1.0, 1e-12);
}

TEST(Presolve, RejectsIntegerPinnedFractional) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Binary, 0.0, 1.0);
  m.addConstraint("lo", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 0.5);
  m.addConstraint("hi", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 0.5);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(r.infeasible);
  CHECK(!r.infeasibility_reason.empty());
}

TEST(Presolve, DualRowIndexMapping) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 20.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 20.0);
  m.addConstraint("keep0", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 10.0);
  m.addConstraint("drop1", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 0.0);
  m.addConstraint("keep2", ConstraintSense::LessEqual, LinearExpression().add(y, 1.0), 10.0);

  PresolveResult r = presolveModel(m, PresolveConfig());
  CHECK(!r.infeasible);
  CHECK_EQ(r.reduced.numConstraints(), 2);
  CHECK_EQ(r.reducedRowForOriginal(1), -1);
  CHECK_EQ(r.reducedRowForOriginal(0), 0);
  CHECK_EQ(r.reducedRowForOriginal(2), 1);

  std::vector<double> dual_red = {-1.0, -2.0};
  std::vector<double> dual_full = r.mapDuals(dual_red);
  CHECK_EQ(static_cast<int>(dual_full.size()), 3);
  CHECK_NEAR(dual_full[0], -1.0, 1e-12);
  CHECK_NEAR(dual_full[1], 0.0, 1e-12);
  CHECK_NEAR(dual_full[2], -2.0, 1e-12);
}
