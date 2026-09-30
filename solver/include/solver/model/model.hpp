#pragma once

#include <map>
#include <string>
#include <vector>

#include "solver/core/types.hpp"
#include "solver/model/linear_expr.hpp"
#include "solver/model/sparse_matrix.hpp"

namespace solver {

struct Variable {
  std::string name;
  VarType type = VarType::Continuous;
  double lb = 0.0;
  double ub = kInfinity;
  double obj = 0.0;
};

struct Constraint {
  std::string name;
  ConstraintSense sense = ConstraintSense::LessEqual;
  double rhs = 0.0;
  std::vector<int> cols;
  std::vector<double> vals;
};

struct ValidationIssue {
  enum class Severity { Error, Warning };
  Severity severity = Severity::Error;
  std::string code;
  std::string context;
  std::string message;
};

struct ValidationResult {
  std::vector<ValidationIssue> issues;

  bool hasErrors() const;
  bool ok() const { return !hasErrors(); }
  std::vector<ValidationIssue> errors() const;
  std::vector<ValidationIssue> warnings() const;
  std::string toString() const;
};

struct ModelStatistics {
  int num_variables = 0;
  int num_constraints = 0;
  int num_nonzeros = 0;
  int num_integer_variables = 0;
  int num_binary_variables = 0;
  int num_free_variables = 0;
  int num_fixed_variables = 0;
  double density = 0.0;
};

class OptimizationModel {
 public:
  OptimizationModel() = default;

  void setName(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  int addVariable(std::string name = "", VarType type = VarType::Continuous, double lb = 0.0,
                  double ub = kInfinity);
  int addVariable(const Variable& variable);

  int addConstraint(const Constraint& constraint);
  int addConstraint(std::string name, ConstraintSense sense, double rhs,
                    std::vector<int> cols, std::vector<double> vals);
  int addConstraint(std::string name, ConstraintSense sense, const LinearExpression& expr,
                    double rhs);

  void setObjectiveSense(ObjectiveSense sense) { sense_ = sense; }
  ObjectiveSense objectiveSense() const { return sense_; }

  void setObjective(const LinearExpression& expr);
  void setObjectiveCoefficient(int variable, double coefficient);
  double objectiveConstant() const { return objective_constant_; }

  void setVariableBounds(int variable, double lb, double ub);
  void setVariableType(int variable, VarType type);
  void setVariableObjective(int variable, double coefficient);
  bool setVariableName(int variable, const std::string& name);
  bool setConstraintRhs(int constraint, double rhs);
  bool setConstraintName(int constraint, const std::string& name);

  void setMeta(const std::string& key, const std::string& value) { meta_[key] = value; }
  std::string getMeta(const std::string& key, const std::string& fallback = "") const;
  const std::map<std::string, std::string>& meta() const { return meta_; }

  int numVariables() const { return static_cast<int>(variables_.size()); }
  int numConstraints() const { return static_cast<int>(constraints_.size()); }
  int numNonzeros() const;
  int numIntegerVariables() const;

  const std::vector<Variable>& variables() const { return variables_; }
  const std::vector<Constraint>& constraints() const { return constraints_; }
  const Variable& variable(int index) const { return variables_[static_cast<std::size_t>(index)]; }
  const Constraint& constraint(int index) const {
    return constraints_[static_cast<std::size_t>(index)];
  }
  Variable& variable(int index) {
    invalidate();
    return variables_[static_cast<std::size_t>(index)];
  }
  Constraint& constraint(int index) {
    invalidate();
    return constraints_[static_cast<std::size_t>(index)];
  }
  const std::vector<double>& objective() const { return objective_; }

  const std::vector<int>& rowStarts() const;
  const std::vector<int>& rowCols() const;
  const std::vector<double>& rowVals() const;

  const SparseMatrix& matrix() const;

  int findVariable(const std::string& name) const;
  int findConstraint(const std::string& name) const;

  double evaluateObjective(const std::vector<double>& x) const;
  double rowActivity(int constraint, const std::vector<double>& x) const;
  std::vector<double> rowActivities(const std::vector<double>& x) const;

  bool isIntegralFeasible(const std::vector<double>& x, double tolerance) const;
  bool isBoundFeasible(const std::vector<double>& x, double tolerance) const;

  ValidationResult validate() const;
  ModelStatistics statistics() const;

  void clear();

 private:
  void invalidate();
  void ensureRowCache() const;

  std::string name_;
  ObjectiveSense sense_ = ObjectiveSense::Minimize;
  double objective_constant_ = 0.0;
  std::vector<Variable> variables_;
  std::vector<Constraint> constraints_;
  std::vector<double> objective_;
  std::map<std::string, std::string> meta_;

  mutable bool matrix_dirty_ = true;
  mutable SparseMatrix matrix_;
  mutable bool row_cache_dirty_ = true;
  mutable std::vector<int> row_starts_;
  mutable std::vector<int> row_cols_;
  mutable std::vector<double> row_vals_;
};

}  // namespace solver
