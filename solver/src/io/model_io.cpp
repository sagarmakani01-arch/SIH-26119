#include "solver/io/model_io.hpp"

#include <cmath>
#include <sstream>

namespace solver {
namespace {

bool senseFromString(const std::string& s, ConstraintSense& out) {
  if (s == "<=" || s == "le" || s == "LessEqual") {
    out = ConstraintSense::LessEqual;
    return true;
  }
  if (s == ">=" || s == "ge" || s == "GreaterEqual") {
    out = ConstraintSense::GreaterEqual;
    return true;
  }
  if (s == "=" || s == "eq" || s == "==" || s == "Equal") {
    out = ConstraintSense::Equal;
    return true;
  }
  return false;
}

bool objectiveSenseFromString(const std::string& s, ObjectiveSense& out) {
  if (s == "minimize" || s == "min" || s == "Minimize") {
    out = ObjectiveSense::Minimize;
    return true;
  }
  if (s == "maximize" || s == "max" || s == "Maximize") {
    out = ObjectiveSense::Maximize;
    return true;
  }
  return false;
}

bool varTypeFromString(const std::string& s, VarType& out) {
  if (s == "continuous" || s == "Continuous" || s.empty()) {
    out = VarType::Continuous;
    return true;
  }
  if (s == "integer" || s == "Integer" || s == "int") {
    out = VarType::Integer;
    return true;
  }
  if (s == "binary" || s == "Binary" || s == "bin") {
    out = VarType::Binary;
    return true;
  }
  return false;
}

double boundOr(const json::Value& obj, const std::string& key, double fallback) {
  const json::Value* v = obj.find(key);
  if (!v) return fallback;
  if (v->isNull()) return fallback;
  if (!v->isNumber()) return fallback;
  return v->asNumber();
}

}  // namespace

ModelDecodeResult modelFromJson(const json::Value& root) {
  ModelDecodeResult out;
  if (!root.isObject()) {
    out.error = "model must be a JSON object";
    return out;
  }

  OptimizationModel& m = out.model;
  if (root.has("name")) m.setName(root.getString("name", ""));

  const json::Value* objective = root.find("objective");
  ObjectiveSense osense = ObjectiveSense::Minimize;
  if (objective && objective->isObject()) {
    const std::string sense_str = objective->getString("sense", "minimize");
    if (!objectiveSenseFromString(sense_str, osense)) {
      out.error = "objective.sense must be 'minimize' or 'maximize'";
      return out;
    }
  }
  m.setObjectiveSense(osense);

  const json::Value* variables = root.find("variables");
  if (!variables || !variables->isArray() || variables->size() == 0) {
    out.error = "variables must be a non-empty array";
    return out;
  }
  const std::size_t n = variables->size();
  for (std::size_t j = 0; j < n; ++j) {
    const json::Value& v = (*variables).at(j);
    if (!v.isObject()) {
      out.error = "variables[" + std::to_string(j) + "] must be an object";
      return out;
    }
    const std::string name = v.getString("name", "x" + std::to_string(j));
    VarType type = VarType::Continuous;
    if (!varTypeFromString(v.getString("type", "continuous"), type)) {
      out.error = "variables[" + std::to_string(j) + "].type must be continuous|integer|binary";
      return out;
    }
    double lb = 0.0;
    double ub = kInfinity;
    if (type == VarType::Binary) {
      lb = boundOr(v, "lb", 0.0);
      ub = boundOr(v, "ub", 1.0);
    } else {
      lb = boundOr(v, "lb", 0.0);
      ub = boundOr(v, "ub", kInfinity);
    }
    m.addVariable(name, type, lb, ub);
  }

  if (objective) {
    const json::Value* coeffs = objective->find("coefficients");
    if (coeffs && coeffs->isArray()) {
      if (coeffs->size() != n) {
        out.error = "objective.coefficients length must match variables length";
        return out;
      }
      LinearExpression expr;
      expr.constant(objective->getNumber("constant", 0.0));
      for (std::size_t j = 0; j < n; ++j) {
        const double c = coeffs->at(j).isNumber() ? coeffs->at(j).asNumber() : 0.0;
        if (c != 0.0) expr.add(static_cast<int>(j), c);
      }
      m.setObjective(expr);
    }
  }

  const json::Value* constraints = root.find("constraints");
  if (constraints) {
    if (!constraints->isArray()) {
      out.error = "constraints must be an array";
      return out;
    }
    for (std::size_t i = 0; i < constraints->size(); ++i) {
      const json::Value& c = (*constraints).at(i);
      if (!c.isObject()) {
        out.error = "constraints[" + std::to_string(i) + "] must be an object";
        return out;
      }
      ConstraintSense sense = ConstraintSense::LessEqual;
      if (!senseFromString(c.getString("sense", "<="), sense)) {
        out.error = "constraints[" + std::to_string(i) +
                    "].sense must be <=|>=|= ";
        return out;
      }
      const json::Value* coeffs = c.find("coefficients");
      if (!coeffs || !coeffs->isArray()) {
        out.error = "constraints[" + std::to_string(i) + "].coefficients must be an array";
        return out;
      }
      if (coeffs->size() != n) {
        out.error = "constraints[" + std::to_string(i) +
                    "].coefficients length must match variables length";
        return out;
      }
      std::vector<int> cols;
      std::vector<double> vals;
      for (std::size_t j = 0; j < n; ++j) {
        const double a = coeffs->at(j).isNumber() ? coeffs->at(j).asNumber() : 0.0;
        if (a != 0.0) {
          cols.push_back(static_cast<int>(j));
          vals.push_back(a);
        }
      }
      const std::string name = c.getString("name", "c" + std::to_string(i));
      const double rhs = c.getNumber("rhs", 0.0);
      m.addConstraint(name, sense, rhs, std::move(cols), std::move(vals));
    }
  }

  SolverConfig& cfg = out.config;
  const json::Value* config = root.find("config");
  if (config && config->isObject()) {
    cfg.time_limit_sec = config->getNumber("time_limit_sec", cfg.time_limit_sec);
    cfg.max_iterations = static_cast<long>(config->getNumber(
        "max_iterations", static_cast<double>(cfg.max_iterations)));
    cfg.mip_rel_gap = config->getNumber("mip_rel_gap", cfg.mip_rel_gap);
    cfg.presolve = config->getBool("presolve", cfg.presolve);
    cfg.verify = config->getBool("verify", cfg.verify);
    const std::string backend = config->getString("backend", "auto");
    if (backend == "cpu") cfg.backend = ComputeBackendKind::CPU;
    else if (backend == "gpu") cfg.backend = ComputeBackendKind::GPU;
    else cfg.backend = ComputeBackendKind::Auto;
    const json::Value* tols = config->find("tolerances");
    if (tols && tols->isObject()) {
      cfg.tolerances.primal_feasibility =
          tols->getNumber("primal_feasibility", cfg.tolerances.primal_feasibility);
      cfg.tolerances.dual_feasibility =
          tols->getNumber("dual_feasibility", cfg.tolerances.dual_feasibility);
      cfg.tolerances.integrality = tols->getNumber("integrality", cfg.tolerances.integrality);
    }
  }

  out.ok = true;
  return out;
}

json::Value resultToJson(const SolverResult& r) {
  json::Value out = json::Value::makeObject();
  out.set("status", json::Value::makeString(toString(r.status)));
  out.set("problem_class", json::Value::makeString(toString(r.problem_class)));
  out.set("objective", json::Value::makeNumber(r.objective));
  out.set("feasible_point", json::Value::makeBool(r.feasible_point));
  out.set("verified", json::Value::makeBool(r.verified));
  out.set("primal_violation", json::Value::makeNumber(r.primal_violation));
  out.set("dual_violation", json::Value::makeNumber(r.dual_violation));
  out.set("duality_gap",
          std::isfinite(r.duality_gap) ? json::Value::makeNumber(r.duality_gap)
                                       : json::Value::makeNull());

  json::Value x = json::Value::makeArray();
  for (double v : r.x) x.push_back(json::Value::makeNumber(v));
  out.set("x", std::move(x));

  json::Value duals = json::Value::makeArray();
  for (double v : r.duals) duals.push_back(json::Value::makeNumber(v));
  out.set("duals", std::move(duals));

  json::Value rc = json::Value::makeArray();
  for (double v : r.reduced_costs) rc.push_back(json::Value::makeNumber(v));
  out.set("reduced_costs", std::move(rc));

  json::Value acts = json::Value::makeArray();
  for (double v : r.constraint_activities) acts.push_back(json::Value::makeNumber(v));
  out.set("constraint_activities", std::move(acts));

  out.set("termination_reason", json::Value::makeString(r.termination_reason));

  json::Value warnings = json::Value::makeArray();
  for (const std::string& w : r.warnings) warnings.push_back(json::Value::makeString(w));
  out.set("warnings", std::move(warnings));

  json::Value stats = json::Value::makeObject();
  stats.set("solve_time_sec", json::Value::makeNumber(r.statistics.solve_time_sec));
  stats.set("presolve_time_sec", json::Value::makeNumber(r.statistics.presolve_time_sec));
  stats.set("verify_time_sec", json::Value::makeNumber(r.statistics.verify_time_sec));
  stats.set("iterations", json::Value::makeNumber(static_cast<double>(r.statistics.iterations)));
  stats.set("phase1_iterations",
            json::Value::makeNumber(static_cast<double>(r.statistics.phase1_iterations)));
  stats.set("nodes", json::Value::makeNumber(static_cast<double>(r.statistics.nodes)));
  stats.set("refactorizations",
            json::Value::makeNumber(static_cast<double>(r.statistics.refactorizations)));
  stats.set("rows", json::Value::makeNumber(r.statistics.rows));
  stats.set("cols", json::Value::makeNumber(r.statistics.cols));
  stats.set("nonzeros", json::Value::makeNumber(r.statistics.nonzeros));
  stats.set("presolve_rows_removed",
            json::Value::makeNumber(r.statistics.presolve_rows_removed));
  stats.set("presolve_bounds_tightened",
            json::Value::makeNumber(r.statistics.presolve_bounds_tightened));
  stats.set("presolve_fixed_variables_removed",
            json::Value::makeNumber(r.statistics.presolve_fixed_variables_removed));
  stats.set("backend_used", json::Value::makeString(r.statistics.backend_used));
  out.set("statistics", std::move(stats));

  return out;
}

json::Value modelToJson(const OptimizationModel& model) {
  json::Value out = json::Value::makeObject();
  out.set("name", json::Value::makeString(model.name()));

  json::Value objective = json::Value::makeObject();
  objective.set("sense", json::Value::makeString(
                             model.objectiveSense() == ObjectiveSense::Maximize ? "maximize"
                                                                                : "minimize"));
  json::Value coeffs = json::Value::makeArray();
  for (int j = 0; j < model.numVariables(); ++j) {
    coeffs.push_back(json::Value::makeNumber(model.objective()[static_cast<std::size_t>(j)]));
  }
  objective.set("coefficients", std::move(coeffs));
  objective.set("constant", json::Value::makeNumber(model.objectiveConstant()));
  out.set("objective", std::move(objective));

  json::Value variables = json::Value::makeArray();
  for (int j = 0; j < model.numVariables(); ++j) {
    const Variable& v = model.variable(j);
    json::Value var = json::Value::makeObject();
    var.set("name", json::Value::makeString(v.name));
    var.set("type", json::Value::makeString(toString(v.type)));
    var.set("lb", json::Value::makeNumber(v.lb));
    var.set("ub", json::Value::makeNumber(v.ub));
    variables.push_back(std::move(var));
  }
  out.set("variables", std::move(variables));

  json::Value constraints = json::Value::makeArray();
  for (int i = 0; i < model.numConstraints(); ++i) {
    const Constraint& c = model.constraint(i);
    json::Value con = json::Value::makeObject();
    con.set("name", json::Value::makeString(c.name));
    con.set("sense", json::Value::makeString(toString(c.sense)));
    con.set("rhs", json::Value::makeNumber(c.rhs));
    std::vector<double> dense(static_cast<std::size_t>(model.numVariables()), 0.0);
    for (std::size_t k = 0; k < c.cols.size(); ++k) {
      const std::size_t j = static_cast<std::size_t>(c.cols[k]);
      if (j < dense.size()) dense[j] = c.vals[k];
    }
    json::Value ccoeffs = json::Value::makeArray();
    for (double a : dense) ccoeffs.push_back(json::Value::makeNumber(a));
    con.set("coefficients", std::move(ccoeffs));
    constraints.push_back(std::move(con));
  }
  out.set("constraints", std::move(constraints));

  return out;
}

}  // namespace solver
