#include <cmath>

#include "solver/model/validation.hpp"
#include "testing.hpp"

using namespace solver;

TEST(Validation, WellFormedModelHasNoErrors) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 10.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 10.0);
  m.setObjective(LinearExpression().add(x, 1.0).add(y, 2.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0).add(y, 1.0),
                  10.0);
  ValidationResult r = m.validate();
  CHECK(r.ok());
  CHECK_EQ(r.errors().size(), 0u);
}

TEST(Validation, DetectsInvertedBounds) {
  OptimizationModel m;
  m.addVariable("bad", VarType::Continuous, 10.0, 5.0);
  ValidationResult r = m.validate();
  CHECK(r.hasErrors());
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "INVALID_BOUNDS" && i.context == "bad") found = true;
  }
  CHECK(found);
}

TEST(Validation, DetectsNaNCoefficient) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 1.0);
  m.constraint(0).vals[0] = kNaNDouble;
  ValidationResult r = m.validate();
  CHECK(r.hasErrors());
}

TEST(Validation, DetectsNaNRhs) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  int c = m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 1.0);
  CHECK(m.setConstraintRhs(c, kNaNDouble));
  ValidationResult r = m.validate();
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "NON_FINITE_RHS") found = true;
  }
  CHECK(found);
  CHECK(r.hasErrors());
}

TEST(Validation, DetectsOutOfRangeIndex) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 1.0);
  m.addConstraint("c2", ConstraintSense::LessEqual, 5.0, {7}, {1.0});
  ValidationResult r = m.validate();
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "INDEX_OUT_OF_RANGE") found = true;
  }
  CHECK(found);
  CHECK(r.hasErrors());
}

TEST(Validation, WarnsOnEmptyRow) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1.0);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression(), 3.0);
  ValidationResult r = m.validate();
  CHECK(r.ok());
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "EMPTY_ROW") found = true;
  }
  CHECK(found);
}

TEST(Validation, WarnsOnUnusedVariable) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1.0);
  int y = m.addVariable("y", VarType::Continuous, 0.0, 1.0);
  (void)y;
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 1.0);
  ValidationResult r = m.validate();
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "UNUSED_VARIABLE" && i.context == "y") found = true;
  }
  CHECK(found);
}

TEST(Validation, BinaryBoundsViolationIsError) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 5.0);
  m.setVariableType(x, VarType::Binary);
  m.setVariableBounds(x, -1.0, 1.0);
  ValidationResult r = m.validate();
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "BINARY_BOUND_VIOLATION") found = true;
  }
  CHECK(found);
  CHECK(r.hasErrors());
}

TEST(Validation, UnboundedIntegerIsWarning) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Integer, 0.0, kInfinity);
  m.setObjective(LinearExpression().add(x, 1.0));
  m.addConstraint("c1", ConstraintSense::GreaterEqual, LinearExpression().add(x, 1.0), 0.0);
  ValidationResult r = m.validate();
  CHECK(r.ok());
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "UNBOUNDED_INTEGER") found = true;
  }
  CHECK(found);
}

TEST(Validation, EmptyModelWithConstraintsIsError) {
  OptimizationModel m;
  m.addConstraint("c1", ConstraintSense::LessEqual, 5.0, {0}, {1.0});
  ValidationResult r = m.validate();
  CHECK(r.hasErrors());
}

TEST(Validation, ZeroObjectiveWarning) {
  OptimizationModel m;
  int x = m.addVariable("x", VarType::Continuous, 0.0, 1.0);
  m.addConstraint("c1", ConstraintSense::LessEqual, LinearExpression().add(x, 1.0), 1.0);
  ValidationResult r = m.validate();
  bool found = false;
  for (const auto& i : r.issues) {
    if (i.code == "ZERO_OBJECTIVE") found = true;
  }
  CHECK(found);
}
