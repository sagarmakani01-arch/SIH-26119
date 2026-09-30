#include <cmath>

#include "solver/mip/branch_and_bound.hpp"
#include "solver/solver.hpp"
#include "testing.hpp"

using namespace solver;

namespace {

OptimizationModel knapsackModel() {
  OptimizationModel m;
  int x1 = m.addVariable("x1", VarType::Binary, 0.0, 1.0);
  int x2 = m.addVariable("x2", VarType::Binary, 0.0, 1.0);
  int x3 = m.addVariable("x3", VarType::Binary, 0.0, 1.0);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x1, 10.0).add(x2, 6.0).add(x3, 4.0));
  m.addConstraint("capacity", ConstraintSense::LessEqual,
                  LinearExpression().add(x1, 1.0).add(x2, 2.0).add(x3, 3.0), 4.0);
  return m;
}

}  // namespace

TEST(MIP, KnapsackBinaryKnownAnswer) {
  OptimizationModel m = knapsackModel();
  BranchAndBound bb;
  MIPResult r = bb.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.feasible_point);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 16.0, 1e-7);
  CHECK_NEAR(r.x[0], 1.0, 1e-9);
  CHECK_NEAR(r.x[1], 1.0, 1e-9);
  CHECK_NEAR(r.x[2], 0.0, 1e-9);
  CHECK_NEAR(r.mip_gap, 0.0, 1e-12);
  CHECK(r.nodes >= 1);
}

TEST(MIP, IntegerMinNeedsBranching) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Integer, 0.0, 100.0);
  int y = m.addVariable("y", VarType::Integer, 0.0, 100.0);
  m.setObjective(LinearExpression().add(x, 5.0).add(y, 8.0));
  m.addConstraint("cover", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 3.5);

  BranchAndBound bb;
  MIPResult r = bb.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 20.0, 1e-7);
  CHECK_NEAR(r.x[0], 4.0, 1e-9);
  CHECK_NEAR(r.x[1], 0.0, 1e-9);
}

TEST(MIP, PresolveRejectsFractionalPinnedInteger) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Binary, 0.0, 1.0);
  m.addConstraint("lo", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 0.5);
  m.addConstraint("hi", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 0.5);

  BranchAndBound bb;
  MIPResult r = bb.solve(m);

  CHECK(r.status == SolveStatus::Infeasible);
  CHECK(!r.feasible_point);
  CHECK(!r.termination_reason.empty());
}

TEST(MIP, InfeasibleDetectedByBranching) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Binary, 0.0, 1.0);
  int y = m.addVariable("y", VarType::Binary, 0.0, 1.0);
  m.setObjective(LinearExpression().add(x, 1.0).add(y, 1.0));
  m.addConstraint("eq_lo", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 1.4);
  m.addConstraint("eq_hi", ConstraintSense::LessEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 1.4);

  BranchAndBound bb;
  MIPResult r = bb.solve(m);

  CHECK(r.status == SolveStatus::Infeasible);
  CHECK(!r.feasible_point);
  CHECK(!r.termination_reason.empty());
}

TEST(MIP, UnboundedRelaxationReported) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Integer, 0.0, kInfinity);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("nonneg", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 0.0);

  BranchAndBound bb;
  MIPResult r = bb.solve(m);

  CHECK(r.status == SolveStatus::Unbounded);
  CHECK(!r.warnings.empty());
}

TEST(MIP, NodeLimitReturnsIncumbentWithGap) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Integer, 0.0, 100.0);
  int y = m.addVariable("y", VarType::Integer, 0.0, 100.0);
  m.setObjective(LinearExpression().add(x, 5.0).add(y, 8.0));
  m.addConstraint("cover", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 3.5);

  MIPConfig cfg;
  cfg.max_nodes = 1;
  BranchAndBound bb;
  MIPResult r = bb.solve(m, cfg);

  CHECK(r.status == SolveStatus::Feasible);
  CHECK(r.feasible_point);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 20.0, 1e-7);
  CHECK(r.mip_gap > 0.0);
  CHECK(!r.termination_reason.empty());
}

TEST(MIP, FixedIntegerVariableContributes) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Integer, 0.0, 10.0);
  int f = m.addVariable("f", VarType::Integer, 3.0, 3.0);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x, 1.0).add(f, 2.0));
  m.addConstraint("cap", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 5.0);

  BranchAndBound bb;
  MIPResult r = bb.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.x[1], 3.0, 1e-9);
  CHECK_NEAR(r.objective, 5.0 + 6.0, 1e-7);
}

TEST(MIP, SolverFacadeDispatchesMilp) {
  OptimizationModel m = knapsackModel();
  SolverConfig cfg;
  cfg.collect_statistics = true;
  Solver s(cfg);
  SolverResult r = s.solve(m);

  CHECK(r.problem_class == ProblemClass::MixedIntegerLinear);
  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 16.0, 1e-7);
  CHECK(r.statistics.nodes >= 1);
  CHECK(r.statistics.iterations >= 0);
  CHECK(r.statistics.backend_used == "CPU");
  CHECK_EQ(static_cast<int>(r.duals.size()), 0);
  CHECK_EQ(static_cast<int>(r.constraint_activities.size()), 1);
}
