#include <cmath>

#include "solver/lp/simplex.hpp"
#include "solver/lp/standard_problem.hpp"
#include "solver/model/model.hpp"
#include "testing.hpp"

using namespace solver;

namespace {

struct RawSolve {
  StandardizationResult std_result;
  SimplexSolution sol;
  std::vector<double> x;
  double objective = 0.0;
};

RawSolve solveRaw(OptimizationModel& m, SimplexConfig cfg = SimplexConfig()) {
  RawSolve out;
  out.std_result = standardize(m);
  if (!out.std_result.ok) return out;
  SimplexSolver solver(out.std_result.problem, cfg);
  out.sol = solver.solve();
  if (out.sol.feasible_point) {
    out.std_result.expand(out.sol.x, out.x);
    out.objective = out.std_result.problem.toOriginalObjective(out.sol.primal_objective);
  }
  return out;
}

bool primalFeasible(const OptimizationModel& m, const std::vector<double>& x, double tol = 1e-6) {
  if (!m.isBoundFeasible(x, tol)) return false;
  for (int i = 0; i < m.numConstraints(); ++i) {
    const Constraint& c = m.constraint(i);
    double act = m.rowActivity(i, x);
    if (c.sense == ConstraintSense::LessEqual && act > c.rhs + tol) return false;
    if (c.sense == ConstraintSense::GreaterEqual && act < c.rhs - tol) return false;
    if (c.sense == ConstraintSense::Equal && std::fabs(act - c.rhs) > tol) return false;
  }
  return true;
}

}  // namespace

TEST(Simplex, ProductMixMaximize) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 4.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 6.0);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x, 3.0).add(y, 5.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 4.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, LinearExpression().add(y, 2.0), 12.0);
  m.addConstraint("c3", ConstraintSense::LessEqual, LinearExpression().add(x, 3.0).add(y, 2.0), 18.0);

  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Optimal);
  CHECK(r.sol.certificate);
  CHECK(primalFeasible(m, r.x));
  CHECK_NEAR(r.objective, 36.0, 1e-7);
  CHECK_NEAR(r.x[0], 2.0, 1e-7);
  CHECK_NEAR(r.x[1], 6.0, 1e-7);
}

TEST(Simplex, MinimizeWithGreaterEqualNeedsPhase1) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 100.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 100.0);
  m.setObjective(LinearExpression().add(x, 2.0).add(y, 1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 10.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 8.0);

  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Optimal);
  CHECK(primalFeasible(m, r.x));
  CHECK_NEAR(r.objective, 10.0, 1e-7);
  CHECK_NEAR(r.x[0], 0.0, 1e-7);
  CHECK_NEAR(r.x[1], 10.0, 1e-7);
}

TEST(Simplex, EqualitySystem) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, kInfinity);
  int y = m.addVariable("y", VarType::Continuous, 0.0, kInfinity);
  m.setObjective(LinearExpression().add(x, 1.0).add(y, 1.0));
  m.addConstraint("eq", ConstraintSense::Equal, LinearExpression().add(x, 1.0).add(y, 2.0), 10.0);

  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Optimal);
  CHECK(primalFeasible(m, r.x));
  CHECK_NEAR(r.objective, 5.0, 1e-7);
  CHECK_NEAR(r.x[0], 0.0, 1e-7);
  CHECK_NEAR(r.x[1], 5.0, 1e-7);
}

TEST(Simplex, DetectsInfeasible) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, kInfinity);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 1.0);
  m.addConstraint("c2", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 2.0);

  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Infeasible);
  CHECK_FALSE(r.sol.message.empty());
}

TEST(Simplex, DetectsInfeasibleBounds) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 5.0, 1.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  RawSolve r = solveRaw(m);
  CHECK_FALSE(r.std_result.ok);
}

TEST(Simplex, DetectsUnbounded) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 1.0, kInfinity);
  m.setObjective(LinearExpression().add(x, -1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 1.0);

  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Unbounded);
}

TEST(Simplex, FreeVariableWithLowerBound) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, -kInfinity, kInfinity);
  int y = m.addVariable("y", VarType::Continuous, 0.0, kInfinity);
  m.setObjective(LinearExpression().add(x, 1.0).add(y, 1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, -1.0), 4.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 10.0);

  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Optimal);
  CHECK(primalFeasible(m, r.x));
  CHECK_NEAR(r.objective, 4.0, 1e-7);
}

TEST(Simplex, BoundedVariableHitsUpperBound) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 1.0, 4.0);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x, 2.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 10.0);

  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Optimal);
  CHECK_NEAR(r.objective, 8.0, 1e-7);
  CHECK_NEAR(r.x[0], 4.0, 1e-7);
}

TEST(Simplex, NoConstraintsPicksBounds) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 2.0, 5.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  RawSolve r = solveRaw(m);
  CHECK(r.sol.status == SimplexStatus::Optimal);
  CHECK_NEAR(r.x[0], 2.0, 1e-9);
}

TEST(Simplex, TimeLimitRespected) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1000.0);
  m.setObjective(LinearExpression().add(x, -1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 999.0);
  SimplexConfig cfg;
  cfg.max_iterations = 0;
  RawSolve r = solveRaw(m, cfg);
  CHECK(r.sol.status == SimplexStatus::IterationLimit);
}
