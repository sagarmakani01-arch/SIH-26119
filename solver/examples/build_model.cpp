#include <iostream>

#include "solver/model/model.hpp"
#include "solver/model/validation.hpp"

using namespace solver;

int main() {
  OptimizationModel model;
  model.setName("demo_product_mix");
  model.setMeta("data", "synthetic");

  int x1 = model.addVariable("x1", VarType::Continuous, 0.0, 40.0);
  int x2 = model.addVariable("x2", VarType::Continuous, 0.0, 30.0);
  int batches = model.addVariable("batches", VarType::Integer, 0.0, 10.0);

  model.setObjectiveSense(ObjectiveSense::Maximize);
  model.setObjective(LinearExpression().add(x1, 10.0).add(x2, 15.0).add(batches, 4.0));

  model.addConstraint("material", ConstraintSense::LessEqual,
                      LinearExpression().add(x1, 2.0).add(x2, 3.0), 120.0);
  model.addConstraint("labor", ConstraintSense::LessEqual,
                      LinearExpression().add(x1, 1.0).add(x2, 1.0).add(batches, 1.0), 50.0);
  model.addConstraint("min_x1", ConstraintSense::GreaterEqual,
                      LinearExpression().add(x1, 1.0), 5.0);

  ModelStatistics stats = model.statistics();
  std::cout << "Model: " << model.name() << "\n"
            << "  variables    : " << stats.num_variables << "\n"
            << "  constraints  : " << stats.num_constraints << "\n"
            << "  nonzeros     : " << stats.num_nonzeros << "\n"
            << "  integer vars : " << stats.num_integer_variables << "\n"
            << "  density      : " << stats.density << "\n"
            << "  objective    : " << toString(model.objectiveSense()) << "\n\n";

  std::cout << "Matrix (CSC coeff table):\n";
  for (int r = 0; r < model.numConstraints(); ++r) {
    const Constraint& c = model.constraint(r);
    std::cout << "  " << c.name << " (" << toString(c.sense) << " " << c.rhs << "): ";
    for (std::size_t k = 0; k < c.cols.size(); ++k) {
      std::cout << c.vals[k] << "*" << model.variable(c.cols[k]).name
                << (k + 1 < c.cols.size() ? " + " : "");
    }
    std::cout << "\n";
  }

  ValidationResult validation = model.validate();
  std::cout << "\nValidation: " << (validation.ok() ? "OK" : "ERRORS") << "\n"
            << validation.toString();

  std::cout << "objective at (10, 10, 1) = " << model.evaluateObjective({10.0, 10.0, 1.0})
            << "\n";
  return validation.hasErrors() ? 1 : 0;
}
