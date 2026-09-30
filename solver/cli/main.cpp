#include <iostream>
#include <sstream>
#include <string>

#include "solver/io/json.hpp"
#include "solver/io/model_io.hpp"
#include "solver/solver.hpp"

namespace {

int writeOutput(const solver::json::Value& value) {
  std::cout << value.dump() << std::endl;
  return 0;
}

solver::json::Value validationToJson(const solver::OptimizationModel& model) {
  const solver::ValidationResult validation = model.validate();
  solver::json::Value out = solver::json::Value::makeObject();
  out.set("valid", solver::json::Value::makeBool(validation.ok()));
  out.set("status", solver::json::Value::makeString(validation.ok() ? "OK" : "ERROR"));

  solver::json::Value issues = solver::json::Value::makeArray();
  for (const solver::ValidationIssue& issue : validation.issues) {
    solver::json::Value item = solver::json::Value::makeObject();
    item.set("severity",
             solver::json::Value::makeString(issue.severity ==
                                                     solver::ValidationIssue::Severity::Error
                                                 ? "ERROR"
                                                 : "WARNING"));
    item.set("code", solver::json::Value::makeString(issue.code));
    item.set("context", solver::json::Value::makeString(issue.context));
    item.set("message", solver::json::Value::makeString(issue.message));
    issues.push_back(std::move(item));
  }
  out.set("issues", std::move(issues));

  const solver::ModelStatistics s = model.statistics();
  solver::json::Value stats = solver::json::Value::makeObject();
  stats.set("num_variables", solver::json::Value::makeNumber(s.num_variables));
  stats.set("num_constraints", solver::json::Value::makeNumber(s.num_constraints));
  stats.set("num_nonzeros", solver::json::Value::makeNumber(s.num_nonzeros));
  stats.set("num_integer_variables", solver::json::Value::makeNumber(s.num_integer_variables));
  stats.set("num_binary_variables", solver::json::Value::makeNumber(s.num_binary_variables));
  stats.set("num_free_variables", solver::json::Value::makeNumber(s.num_free_variables));
  stats.set("num_fixed_variables", solver::json::Value::makeNumber(s.num_fixed_variables));
  stats.set("density", solver::json::Value::makeNumber(s.density));
  out.set("statistics", std::move(stats));
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const bool validate_only = argc > 1 && std::string(argv[1]) == "--validate";
  if (argc > 1 && !validate_only) {
    const std::string arg = argv[1];
    if (arg == "--version") {
      std::cout << "solver_cli 0.1.0" << std::endl;
      return 0;
    }
    if (arg == "--help" || arg == "-h") {
      std::cout << "usage: solver_cli [--version|--validate] < model.json > result.json\n"
                   "       Reads an optimization model as JSON on stdin, solves it with the\n"
                   "       built-in engine, and writes the result as JSON on stdout.\n"
                   "       --validate only validates the model (no solve).\n";
      return 0;
    }
    std::cerr << "unknown argument: " << arg << std::endl;
    return 2;
  }

  std::ostringstream buffer;
  buffer << std::cin.rdbuf();
  const std::string text = buffer.str();

  std::string parse_error;
  const solver::json::Value root = solver::json::Value::parse(text, &parse_error);
  if (!parse_error.empty()) {
    solver::json::Value out = solver::json::Value::makeObject();
    out.set("status", solver::json::Value::makeString("ERROR"));
    out.set("error", solver::json::Value::makeString("invalid input JSON: " + parse_error));
    writeOutput(out);
    return 1;
  }

  solver::ModelDecodeResult decoded = solver::modelFromJson(root);
  if (!decoded.ok) {
    solver::json::Value out = solver::json::Value::makeObject();
    out.set("status", solver::json::Value::makeString("ERROR"));
    out.set("error", solver::json::Value::makeString(decoded.error));
    writeOutput(out);
    return 1;
  }

  if (validate_only) {
    writeOutput(validationToJson(decoded.model));
    return 0;
  }

  try {
    solver::Solver solver(decoded.config);
    const solver::SolverResult result = solver.solve(decoded.model);
    writeOutput(solver::resultToJson(result));
    return 0;
  } catch (const std::exception& ex) {
    solver::json::Value out = solver::json::Value::makeObject();
    out.set("status", solver::json::Value::makeString("ERROR"));
    out.set("error", solver::json::Value::makeString(std::string("internal error: ") + ex.what()));
    writeOutput(out);
    return 2;
  }
}
