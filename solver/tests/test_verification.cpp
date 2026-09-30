#include "solver/numerical/verification.hpp"
#include "solver/model/model.hpp"
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

}  // namespace

TEST(Verification, DetectsPrimalViolations) {
  OptimizationModel m = productMixModel();
  Tolerances tol;

  VerificationReport good = verifyPrimal(m, {2.0, 6.0}, tol);
  CHECK(good.primal_ok);
  CHECK_NEAR(good.max_primal_violation, 0.0, 1e-12);

  VerificationReport bad = verifyPrimal(m, {5.0, 6.0}, tol);
  CHECK(!bad.primal_ok);
  CHECK(bad.max_primal_violation > 0.0);
  CHECK(!bad.detail.empty());

  VerificationReport wrong_size = verifyPrimal(m, {1.0}, tol);
  CHECK(!wrong_size.primal_ok);
}

TEST(Verification, AcceptsKnownOptimalDual) {
  OptimizationModel m = productMixModel();
  Tolerances tol;
  std::vector<double> x = {2.0, 6.0};
  std::vector<double> duals = {0.0, 0.0, -1.0};

  VerificationReport r = verifyDual(m, x, duals, tol);
  CHECK(r.dual_ok);
  CHECK(r.max_dual_violation <= tol.dual_feasibility);
}

TEST(Verification, RejectsDualWithWrongSign) {
  OptimizationModel m = productMixModel();
  Tolerances tol;
  std::vector<double> x = {2.0, 6.0};
  std::vector<double> duals = {0.0, 0.0, 1.0};

  VerificationReport r = verifyDual(m, x, duals, tol);
  CHECK(!r.dual_ok);
}

TEST(Verification, RejectsDualViolatingComplementarity) {
  OptimizationModel m = productMixModel();
  Tolerances tol;
  std::vector<double> x = {2.0, 6.0};
  std::vector<double> duals = {0.0, -1.0, -1.0};

  VerificationReport r = verifyDual(m, x, duals, tol);
  CHECK(!r.dual_ok);
}

TEST(Verification, ReducedCostsMatchObjectiveGradient) {
  OptimizationModel m = productMixModel();
  std::vector<double> duals = {0.0, 0.0, -1.0};
  std::vector<double> rc = computeReducedCosts(m, duals);
  CHECK_EQ(static_cast<int>(rc.size()), 2);
  CHECK_NEAR(rc[0], -3.0 + 3.0, 1e-12);
  CHECK_NEAR(rc[1], -5.0 + 2.0, 1e-12);
}
