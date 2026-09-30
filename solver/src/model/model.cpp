#include "solver/model/model.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include "solver/model/validation.hpp"

namespace solver {

bool ValidationResult::hasErrors() const {
  return std::any_of(issues.begin(), issues.end(), [](const ValidationIssue& i) {
    return i.severity == ValidationIssue::Severity::Error;
  });
}

std::vector<ValidationIssue> ValidationResult::errors() const {
  std::vector<ValidationIssue> out;
  for (const auto& i : issues) {
    if (i.severity == ValidationIssue::Severity::Error) out.push_back(i);
  }
  return out;
}

std::vector<ValidationIssue> ValidationResult::warnings() const {
  std::vector<ValidationIssue> out;
  for (const auto& i : issues) {
    if (i.severity == ValidationIssue::Severity::Warning) out.push_back(i);
  }
  return out;
}

std::string ValidationResult::toString() const {
  std::ostringstream os;
  for (const auto& i : issues) {
    os << (i.severity == ValidationIssue::Severity::Error ? "[ERROR] " : "[WARN] ") << i.code;
    if (!i.context.empty()) os << " (" << i.context << ")";
    os << ": " << i.message << "\n";
  }
  return os.str();
}

void OptimizationModel::invalidate() {
  matrix_dirty_ = true;
  row_cache_dirty_ = true;
}

int OptimizationModel::addVariable(std::string name, VarType type, double lb, double ub) {
  Variable v;
  v.name = std::move(name);
  v.type = type;
  v.lb = lb;
  v.ub = ub;
  if (type == VarType::Binary) {
    v.lb = std::max(lb, 0.0);
    v.ub = std::min(ub, 1.0);
  }
  return addVariable(v);
}

int OptimizationModel::addVariable(const Variable& variable) {
  variables_.push_back(variable);
  objective_.push_back(variable.obj);
  invalidate();
  return static_cast<int>(variables_.size()) - 1;
}

int OptimizationModel::addConstraint(const Constraint& constraint) {
  if (constraint.cols.size() != constraint.vals.size()) {
    throw std::invalid_argument("addConstraint: cols/vals size mismatch");
  }
  constraints_.push_back(constraint);
  invalidate();
  return static_cast<int>(constraints_.size()) - 1;
}

int OptimizationModel::addConstraint(std::string name, ConstraintSense sense, double rhs,
                                     std::vector<int> cols, std::vector<double> vals) {
  Constraint c;
  c.name = std::move(name);
  c.sense = sense;
  c.rhs = rhs;
  c.cols = std::move(cols);
  c.vals = std::move(vals);
  return addConstraint(c);
}

int OptimizationModel::addConstraint(std::string name, ConstraintSense sense,
                                     const LinearExpression& expr, double rhs) {
  LinearExpression copy = expr;
  copy.merge();
  std::vector<int> cols;
  std::vector<double> vals;
  cols.reserve(copy.terms().size());
  vals.reserve(copy.terms().size());
  for (const auto& t : copy.terms()) {
    cols.push_back(t.first);
    vals.push_back(t.second);
  }
  return addConstraint(std::move(name), sense, rhs, std::move(cols), std::move(vals));
}

void OptimizationModel::setObjective(const LinearExpression& expr) {
  LinearExpression copy = expr;
  copy.merge();
  std::fill(objective_.begin(), objective_.end(), 0.0);
  for (const auto& t : copy.terms()) {
    if (t.first < 0 || t.first >= numVariables()) {
      throw std::out_of_range("setObjective: variable index out of range");
    }
    objective_[static_cast<std::size_t>(t.first)] += t.second;
  }
  objective_constant_ = copy.constantValue();
  invalidate();
}

void OptimizationModel::setObjectiveCoefficient(int variable, double coefficient) {
  if (variable < 0 || variable >= numVariables()) {
    throw std::out_of_range("setObjectiveCoefficient: variable index out of range");
  }
  objective_[static_cast<std::size_t>(variable)] = coefficient;
}

void OptimizationModel::setVariableBounds(int variable, double lb, double ub) {
  if (variable < 0 || variable >= numVariables()) {
    throw std::out_of_range("setVariableBounds: variable index out of range");
  }
  variables_[static_cast<std::size_t>(variable)].lb = lb;
  variables_[static_cast<std::size_t>(variable)].ub = ub;
  invalidate();
}

void OptimizationModel::setVariableType(int variable, VarType type) {
  if (variable < 0 || variable >= numVariables()) {
    throw std::out_of_range("setVariableType: variable index out of range");
  }
  variables_[static_cast<std::size_t>(variable)].type = type;
  invalidate();
}

void OptimizationModel::setVariableObjective(int variable, double coefficient) {
  if (variable < 0 || variable >= numVariables()) {
    throw std::out_of_range("setVariableObjective: variable index out of range");
  }
  variables_[static_cast<std::size_t>(variable)].obj = coefficient;
  objective_[static_cast<std::size_t>(variable)] = coefficient;
}

bool OptimizationModel::setVariableName(int variable, const std::string& name) {
  if (variable < 0 || variable >= numVariables()) return false;
  variables_[static_cast<std::size_t>(variable)].name = name;
  return true;
}

bool OptimizationModel::setConstraintRhs(int constraint, double rhs) {
  if (constraint < 0 || constraint >= numConstraints()) return false;
  constraints_[static_cast<std::size_t>(constraint)].rhs = rhs;
  return true;
}

bool OptimizationModel::setConstraintName(int constraint, const std::string& name) {
  if (constraint < 0 || constraint >= numConstraints()) return false;
  constraints_[static_cast<std::size_t>(constraint)].name = name;
  return true;
}

std::string OptimizationModel::getMeta(const std::string& key, const std::string& fallback) const {
  auto it = meta_.find(key);
  return it == meta_.end() ? fallback : it->second;
}

int OptimizationModel::numNonzeros() const {
  int total = 0;
  for (const auto& c : constraints_) total += static_cast<int>(c.cols.size());
  return total;
}

int OptimizationModel::numIntegerVariables() const {
  int total = 0;
  for (const auto& v : variables_) {
    if (v.type != VarType::Continuous) ++total;
  }
  return total;
}

void OptimizationModel::ensureRowCache() const {
  if (!row_cache_dirty_) return;
  row_starts_.assign(constraints_.size() + 1, 0);
  row_cols_.clear();
  row_vals_.clear();
  row_cols_.reserve(static_cast<std::size_t>(numNonzeros()));
  row_vals_.reserve(static_cast<std::size_t>(numNonzeros()));
  for (std::size_t r = 0; r < constraints_.size(); ++r) {
    const Constraint& c = constraints_[r];
    for (std::size_t k = 0; k < c.cols.size(); ++k) {
      row_cols_.push_back(c.cols[k]);
      row_vals_.push_back(c.vals[k]);
    }
    row_starts_[r + 1] = static_cast<int>(row_cols_.size());
  }
  row_cache_dirty_ = false;
}

const std::vector<int>& OptimizationModel::rowStarts() const {
  ensureRowCache();
  return row_starts_;
}

const std::vector<int>& OptimizationModel::rowCols() const {
  ensureRowCache();
  return row_cols_;
}

const std::vector<double>& OptimizationModel::rowVals() const {
  ensureRowCache();
  return row_vals_;
}

const SparseMatrix& OptimizationModel::matrix() const {
  if (matrix_dirty_) {
    std::vector<Triplet> ts;
    ts.reserve(static_cast<std::size_t>(numNonzeros()));
    for (std::size_t r = 0; r < constraints_.size(); ++r) {
      const Constraint& c = constraints_[r];
      for (std::size_t k = 0; k < c.cols.size(); ++k) {
        ts.push_back({static_cast<int>(r), c.cols[k], c.vals[k]});
      }
    }
    matrix_ = SparseMatrix::fromTriplets(numConstraints(), numVariables(), std::move(ts));
    matrix_dirty_ = false;
  }
  return matrix_;
}

int OptimizationModel::findVariable(const std::string& name) const {
  for (std::size_t i = 0; i < variables_.size(); ++i) {
    if (variables_[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

int OptimizationModel::findConstraint(const std::string& name) const {
  for (std::size_t i = 0; i < constraints_.size(); ++i) {
    if (constraints_[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

double OptimizationModel::evaluateObjective(const std::vector<double>& x) const {
  if (static_cast<int>(x.size()) != numVariables()) {
    throw std::invalid_argument("evaluateObjective: dimension mismatch");
  }
  double acc = objective_constant_;
  for (std::size_t i = 0; i < objective_.size(); ++i) acc += objective_[i] * x[i];
  return acc;
}

double OptimizationModel::rowActivity(int constraint, const std::vector<double>& x) const {
  if (constraint < 0 || constraint >= numConstraints()) {
    throw std::out_of_range("rowActivity: constraint index out of range");
  }
  const Constraint& c = constraints_[static_cast<std::size_t>(constraint)];
  double acc = 0.0;
  for (std::size_t k = 0; k < c.cols.size(); ++k) {
    acc += c.vals[k] * x[static_cast<std::size_t>(c.cols[k])];
  }
  return acc;
}

std::vector<double> OptimizationModel::rowActivities(const std::vector<double>& x) const {
  std::vector<double> out(constraints_.size(), 0.0);
  for (std::size_t r = 0; r < constraints_.size(); ++r) out[r] = rowActivity(static_cast<int>(r), x);
  return out;
}

bool OptimizationModel::isBoundFeasible(const std::vector<double>& x, double tolerance) const {
  if (static_cast<int>(x.size()) != numVariables()) return false;
  for (std::size_t i = 0; i < variables_.size(); ++i) {
    if (x[i] < variables_[i].lb - tolerance) return false;
    if (x[i] > variables_[i].ub + tolerance) return false;
    if (!std::isfinite(x[i])) return false;
  }
  return true;
}

bool OptimizationModel::isIntegralFeasible(const std::vector<double>& x, double tolerance) const {
  if (static_cast<int>(x.size()) != numVariables()) return false;
  for (std::size_t i = 0; i < variables_.size(); ++i) {
    if (variables_[i].type == VarType::Continuous) continue;
    double r = std::round(x[i]);
    if (std::abs(x[i] - r) > tolerance) return false;
  }
  return true;
}

ValidationResult OptimizationModel::validate() const { return validateModel(*this); }

ModelStatistics OptimizationModel::statistics() const {
  ModelStatistics s;
  s.num_variables = numVariables();
  s.num_constraints = numConstraints();
  s.num_nonzeros = numNonzeros();
  s.num_integer_variables = numIntegerVariables();
  for (const auto& v : variables_) {
    if (v.type == VarType::Binary) ++s.num_binary_variables;
    if (v.lb == -kInfinity && v.ub == kInfinity) ++s.num_free_variables;
    if (v.lb == v.ub) ++s.num_fixed_variables;
  }
  double denom = static_cast<double>(std::max(1, s.num_variables)) *
                 static_cast<double>(std::max(1, s.num_constraints));
  s.density = s.num_nonzeros / denom;
  return s;
}

void OptimizationModel::clear() {
  name_.clear();
  sense_ = ObjectiveSense::Minimize;
  objective_constant_ = 0.0;
  variables_.clear();
  constraints_.clear();
  objective_.clear();
  meta_.clear();
  invalidate();
}

}  // namespace solver
