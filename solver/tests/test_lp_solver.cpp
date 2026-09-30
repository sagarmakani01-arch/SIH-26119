#include <cmath>

#include "solver/lp/lp_solver.hpp"
#include "solver/solver.hpp"
#include "testing.hpp"

using namespace solver;

namespace {

OptimizationModel productMixModel() {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 4.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 6.0);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x, 3.0).add(y, 5.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 4.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, LinearExpression().add(y, 1.0), 12.0);
  m.addConstraint("c3", ConstraintSense::LessEqual,
                  LinearExpression().add(x, 3.0).add(y, 2.0), 18.0);
  return m;
}

OptimizationModel phase1Model() {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 100.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 100.0);
  m.setObjective(LinearExpression().add(x, 2.0).add(y, 1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual,
                  LinearExpression().add(x, 1.0).add(y, 1.0), 10.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 8.0);
  return m;
}

OptimizationModel infeasibleModel() {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1.0);
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 3.0);
  return m;
}

OptimizationModel unboundedModel() {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, kInfinity);
  m.setObjective(LinearExpression().add(x, -1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 0.0);
  return m;
}

}  // namespace

TEST(LPSolver, ProductMixKnownAnswerIsVerified) {
  OptimizationModel m = productMixModel();
  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.feasible_point);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 36.0, 1e-7);
  CHECK_NEAR(r.x[0], 2.0, 1e-7);
  CHECK_NEAR(r.x[1], 6.0, 1e-7);
  CHECK(r.primal_violation <= 1e-6);
  CHECK(r.dual_violation <= 1e-6);
  CHECK_EQ(static_cast<int>(r.duals.size()), 3);
  CHECK_EQ(static_cast<int>(r.reduced_costs.size()), 2);
  CHECK(r.iterations > 0);
}

TEST(LPSolver, PhaseOneGreaterEqualIsVerified) {
  OptimizationModel m = phase1Model();
  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 10.0, 1e-7);
  CHECK_NEAR(r.x[1], 10.0, 1e-7);
}

TEST(LPSolver, PresolvedTightBoundsStillCertified) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 100.0);
  m.setObjective(LinearExpression().add(x, 2.0));
  m.addConstraint("cover", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 30.0);

  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 60.0, 1e-7);
  CHECK_NEAR(r.x[0], 30.0, 1e-7);
}

TEST(LPSolver, InfeasibleModelDetected) {
  OptimizationModel m = infeasibleModel();
  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Infeasible);
  CHECK(!r.feasible_point);
  CHECK(!r.termination_reason.empty());
}

TEST(LPSolver, UnboundedModelDetected) {
  OptimizationModel m = unboundedModel();
  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Unbounded);
}

TEST(LPSolver, FixedVariableContributesObjective) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  int f = m.addVariable("f", VarType::Continuous, 3.0, 3.0);
  m.setObjectiveSense(ObjectiveSense::Maximize);
  m.setObjective(LinearExpression().add(x, 1.0).add(f, 4.0));
  m.addConstraint("cap", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 5.0);

  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.x[static_cast<std::size_t>(f)], 3.0, 1e-9);
  CHECK_NEAR(r.x[static_cast<std::size_t>(x)], 5.0, 1e-7);
  CHECK_NEAR(r.objective, 5.0 + 12.0, 1e-7);
}

TEST(LPSolver, FreeVariableEqualitySystem) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, -kInfinity, kInfinity);
  int y = m.addVariable("y", VarType::Continuous, -kInfinity, kInfinity);
  m.setObjective(LinearExpression().add(x, 1.0).add(y, 1.0));
  m.addConstraint("e1", ConstraintSense::Equal, LinearExpression().add(x, 1.0).add(y, 1.0), 6.0);
  m.addConstraint("e2", ConstraintSense::Equal, LinearExpression().add(x, 1.0).add(y, -1.0), 2.0);

  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.x[0], 4.0, 1e-7);
  CHECK_NEAR(r.x[1], 2.0, 1e-7);
  CHECK_NEAR(r.objective, 6.0, 1e-7);
}

TEST(LPSolver, NegativeRhsRowsAreCertified) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 10.0);
  m.setObjective(LinearExpression().add(x, 3.0).add(y, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual,
                  LinearExpression().add(x, -2.0).add(y, -1.0), -4.0);
  m.addConstraint("c2", ConstraintSense::LessEqual,
                  LinearExpression().add(x, -1.0).add(y, 1.0), -3.0);

  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 9.0, 1e-6);
  CHECK_NEAR(r.x[0], 3.0, 1e-6);
  CHECK_NEAR(r.x[1], 0.0, 1e-6);
}

TEST(LPSolver, PresolveStatisticsReported) {
  OptimizationModel m = productMixModel();
  int f = m.addVariable("f", VarType::Continuous, 7.0, 7.0);
  (void)f;
  LPSolver solver;
  LPResult r = solver.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.presolve_stats.fixed_variables_removed == 1);
  CHECK(r.cols == 3);
}

TEST(LPSolver, IterationLimitStopsEarly) {
  OptimizationModel m = productMixModel();
  LPConfig cfg;
  cfg.simplex.max_iterations = 1;
  LPSolver solver;
  LPResult r = solver.solve(m, cfg);

  CHECK(r.status == SolveStatus::IterationLimit);
}

TEST(LPSolver, UnimplementedAlgorithmRejected) {
  OptimizationModel m = productMixModel();
  LPConfig cfg;
  cfg.algorithm = LPAlgorithm::InteriorPoint;
  LPSolver solver;
  LPResult r = solver.solve(m, cfg);

  CHECK(r.status == SolveStatus::Error);
  CHECK(!r.termination_reason.empty());
}

TEST(Solver, DispatchesLpAndCollectsStatistics) {
  OptimizationModel m = productMixModel();
  SolverConfig cfg;
  cfg.collect_statistics = true;
  Solver s(cfg);
  SolverResult r = s.solve(m);

  CHECK(r.problem_class == ProblemClass::Linear);
  CHECK(r.status == SolveStatus::Optimal);
  CHECK(r.verified);
  CHECK_NEAR(r.objective, 36.0, 1e-7);
  CHECK_EQ(static_cast<int>(r.constraint_activities.size()), 3);
  CHECK_NEAR(r.constraint_activities[0], 2.0, 1e-7);
  CHECK(r.statistics.rows == 3);
  CHECK(r.statistics.cols == 2);
  CHECK(r.statistics.nonzeros > 0);
  CHECK(r.statistics.iterations > 0);
  CHECK(r.statistics.backend_used == "CPU");
}

TEST(Solver, GpuRequestFallsBackToCpuWithWarning) {
  OptimizationModel m = productMixModel();
  SolverConfig cfg;
  cfg.backend = ComputeBackendKind::GPU;
  Solver s(cfg);
  SolverResult r = s.solve(m);

  CHECK(r.status == SolveStatus::Optimal);
  bool warned = false;
  for (const std::string& w : r.warnings) {
    if (w.find("GPU") != std::string::npos) warned = true;
  }
  CHECK(warned);
}

TEST(Solver, ClassifyDetectsIntegerVariables) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  CHECK(classifyProblem(m) == ProblemClass::Linear);
  m.setVariableType(x, VarType::Integer);
  CHECK(classifyProblem(m) == ProblemClass::MixedIntegerLinear);
}
