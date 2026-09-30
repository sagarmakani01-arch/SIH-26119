#include "solver/mip/branch_and_bound.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <queue>

#include "solver/numerical/verification.hpp"

namespace solver {
namespace {

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

double toMinSpace(ObjectiveSense sense, double value) {
  return sense == ObjectiveSense::Maximize ? -value : value;
}

struct Node {
  std::vector<double> lb;
  std::vector<double> ub;
  double bound_min = 0.0;
  std::vector<double> x;
};

struct BetterNode {
  bool operator()(const Node& a, const Node& b) const { return a.bound_min > b.bound_min; }
};

LPConfig makeNodeConfig(const MIPConfig& config) {
  LPConfig lp_cfg;
  lp_cfg.simplex.tol = config.tol;
  lp_cfg.simplex.max_iterations = 1000000;
  lp_cfg.simplex.time_limit_sec = config.time_limit_sec;
  lp_cfg.presolve.enabled = false;
  lp_cfg.verify = false;
  lp_cfg.algorithm = LPAlgorithm::PrimalSimplex;
  return lp_cfg;
}

OptimizationModel modelWithBounds(const OptimizationModel& base,
                                  const std::vector<double>& lb,
                                  const std::vector<double>& ub) {
  OptimizationModel m = base;
  for (int j = 0; j < m.numVariables(); ++j) {
    const Variable& v = base.variable(j);
    const double nlb = lb[static_cast<std::size_t>(j)];
    const double nub = ub[static_cast<std::size_t>(j)];
    if (nlb != v.lb || nub != v.ub) m.setVariableBounds(j, nlb, nub);
  }
  return m;
}

int pickBranchVariable(const OptimizationModel& base, const std::vector<double>& x,
                       const Tolerances& tol) {
  int best = -1;
  double best_frac = tol.integrality;
  for (int j = 0; j < base.numVariables(); ++j) {
    const Variable& v = base.variable(j);
    if (v.type == VarType::Continuous) continue;
    const double xv = x[static_cast<std::size_t>(j)];
    const double f = xv - std::floor(xv);
    const double dist = std::min(f, 1.0 - f);
    if (dist > best_frac) {
      best_frac = dist;
      best = j;
    }
  }
  return best;
}

}  // namespace

MIPResult BranchAndBound::solve(const OptimizationModel& model, const MIPConfig& config) {
  MIPResult out;
  const auto t_start = Clock::now();

  const ValidationResult validation = model.validate();
  if (validation.hasErrors()) {
    out.status = SolveStatus::Error;
    out.termination_reason = "model validation failed: " + validation.toString();
    out.solve_time_sec = secondsSince(t_start);
    return out;
  }
  for (const ValidationIssue& w : validation.warnings()) out.warnings.push_back(w.message);

  PresolveConfig presolve_cfg;
  presolve_cfg.enabled = config.presolve;
  presolve_cfg.tol = config.tol;
  const auto t_presolve = Clock::now();
  PresolveResult presolved = presolveModel(model, presolve_cfg);
  out.presolve_time_sec = secondsSince(t_presolve);
  out.presolve_stats = presolved.stats;
  if (presolved.infeasible) {
    out.status = SolveStatus::Infeasible;
    out.termination_reason = "detected during presolve: " + presolved.infeasibility_reason;
    out.solve_time_sec = secondsSince(t_start);
    return out;
  }

  const OptimizationModel& base = presolved.reduced;
  const ObjectiveSense sense = base.objectiveSense();
  const LPConfig lp_cfg = makeNodeConfig(config);
  LPSolver lp_solver;

  std::vector<double> base_lb(static_cast<std::size_t>(base.numVariables()));
  std::vector<double> base_ub(static_cast<std::size_t>(base.numVariables()));
  for (int j = 0; j < base.numVariables(); ++j) {
    base_lb[static_cast<std::size_t>(j)] = base.variable(j).lb;
    base_ub[static_cast<std::size_t>(j)] = base.variable(j).ub;
  }

  auto solveNodeModel = [&](const std::vector<double>& nlb,
                            const std::vector<double>& nub) -> LPResult {
    OptimizationModel nm = modelWithBounds(base, nlb, nub);
    return lp_solver.solve(nm, lp_cfg);
  };

  const auto t_solve = Clock::now();

  LPResult root = solveNodeModel(base_lb, base_ub);
  out.lp_iterations += root.iterations;
  out.lp_relaxations += 1;

  if (!root.feasible_point) {
    out.status = root.status;
    out.termination_reason = "root LP relaxation: " + root.termination_reason;
    out.solve_time_sec = secondsSince(t_solve);
    return out;
  }
  if (root.status == SolveStatus::Unbounded) {
    out.status = SolveStatus::Unbounded;
    out.termination_reason = "LP relaxation is unbounded: " + root.termination_reason;
    out.warnings.push_back(
        "relaxation unbounded; an integer program can still be bounded in rare cases");
    out.solve_time_sec = secondsSince(t_solve);
    return out;
  }

  bool limit_hit = false;
  std::string limit_reason;
  SolveStatus limit_status = SolveStatus::TimeLimit;
  if (root.status == SolveStatus::TimeLimit || root.status == SolveStatus::IterationLimit) {
    limit_hit = true;
    limit_status = root.status;
    limit_reason = "root LP relaxation limit: " + root.termination_reason;
  }

  bool has_incumbent = false;
  std::vector<double> incumbent_x;
  double incumbent_obj = kNaNDouble;
  double incumbent_min = 0.0;

  auto tryCandidate = [&](const std::vector<double>& xr) {
    if (static_cast<int>(xr.size()) != base.numVariables()) return;
    if (!base.isIntegralFeasible(xr, config.tol.integrality)) return;
    const VerificationReport pv = verifyPrimal(base, xr, config.tol);
    if (!pv.primal_ok) return;
    const double obj = base.evaluateObjective(xr);
    const double obj_min = toMinSpace(sense, obj);
    if (!has_incumbent || obj_min < incumbent_min - 1e-9) {
      has_incumbent = true;
      incumbent_min = obj_min;
      incumbent_obj = obj;
      incumbent_x = xr;
    }
  };

  auto tryRounded = [&](const std::vector<double>& xr) {
    if (static_cast<int>(xr.size()) != base.numVariables()) return;
    std::vector<double> rounded = xr;
    for (int j = 0; j < base.numVariables(); ++j) {
      const Variable& v = base.variable(j);
      if (v.type == VarType::Continuous) continue;
      double r = std::round(rounded[static_cast<std::size_t>(j)]);
      r = std::min(std::max(r, base_lb[static_cast<std::size_t>(j)]),
                   base_ub[static_cast<std::size_t>(j)]);
      rounded[static_cast<std::size_t>(j)] = r;
    }
    tryCandidate(rounded);
  };

  auto tryBoth = [&](const std::vector<double>& xr) {
    tryCandidate(xr);
    tryRounded(xr);
  };

  tryBoth(root.x);

  std::priority_queue<Node, std::vector<Node>, BetterNode> queue;
  if (!limit_hit) {
    Node root_node;
    root_node.lb = base_lb;
    root_node.ub = base_ub;
    root_node.bound_min = toMinSpace(sense, root.objective);
    root_node.x = root.x;
    queue.push(std::move(root_node));
  }

  long nodes = 0;
  bool exhausted = false;
  bool gap_stop = false;
  bool unbounded_child_skipped = false;
  const double prune_eps = 1e-9;

  auto pushChild = [&](std::vector<double> clb, std::vector<double> cub,
                       const LPResult& lp) {
    tryBoth(lp.x);
    if (has_incumbent) {
      const double level = incumbent_min - prune_eps * (1.0 + std::fabs(incumbent_min));
      if (toMinSpace(sense, lp.objective) >= level) return;
    }
    Node child;
    child.lb = std::move(clb);
    child.ub = std::move(cub);
    child.bound_min = toMinSpace(sense, lp.objective);
    child.x = lp.x;
    queue.push(std::move(child));
  };

  auto solveChild = [&](const std::vector<double>& clb,
                        const std::vector<double>& cub) -> bool {
    LPResult lp = solveNodeModel(clb, cub);
    out.lp_iterations += lp.iterations;
    out.lp_relaxations += 1;
    if (lp.status == SolveStatus::Error) {
      out.status = SolveStatus::Error;
      out.termination_reason = "node LP failed: " + lp.termination_reason;
      out.solve_time_sec = secondsSince(t_solve);
      return false;
    }
    if (lp.feasible_point && lp.status != SolveStatus::Unbounded) {
      pushChild(clb, cub, lp);
    } else if (lp.status == SolveStatus::Unbounded) {
      unbounded_child_skipped = true;
    }
    return true;
  };

  while (!queue.empty()) {
    if (secondsSince(t_start) >= config.time_limit_sec) {
      limit_hit = true;
      limit_status = SolveStatus::TimeLimit;
      limit_reason = "time limit reached";
      break;
    }
    if (nodes >= config.max_nodes) {
      limit_hit = true;
      limit_status = SolveStatus::IterationLimit;
      limit_reason = "node limit reached";
      break;
    }
    if (out.lp_iterations >= config.max_lp_iterations) {
      limit_hit = true;
      limit_status = SolveStatus::IterationLimit;
      limit_reason = "LP iteration limit reached";
      break;
    }

    Node node = queue.top();
    queue.pop();

    if (has_incumbent) {
      const double level = incumbent_min - prune_eps * (1.0 + std::fabs(incumbent_min));
      if (node.bound_min >= level) continue;
    }

    ++nodes;

    const int j = pickBranchVariable(base, node.x, config.tol);
    if (j < 0) {
      tryBoth(node.x);
      continue;
    }

    const double xv = node.x[static_cast<std::size_t>(j)];

    std::vector<double> child_lb = node.lb;
    std::vector<double> child_ub = node.ub;
    child_ub[static_cast<std::size_t>(j)] =
        std::min(child_ub[static_cast<std::size_t>(j)], std::floor(xv));
    if (child_lb[static_cast<std::size_t>(j)] <=
        child_ub[static_cast<std::size_t>(j)] + config.tol.primal_feasibility) {
      if (!solveChild(child_lb, child_ub)) return out;
    }

    child_lb = node.lb;
    child_ub = node.ub;
    child_lb[static_cast<std::size_t>(j)] =
        std::max(child_lb[static_cast<std::size_t>(j)], std::ceil(xv));
    if (child_lb[static_cast<std::size_t>(j)] <=
        child_ub[static_cast<std::size_t>(j)] + config.tol.primal_feasibility) {
      if (!solveChild(child_lb, child_ub)) return out;
    }

    if (has_incumbent && !queue.empty() && config.mip_rel_gap >= 0.0) {
      const double best_min = queue.top().bound_min;
      const double denom = std::max(1.0, std::fabs(incumbent_min));
      const double gap = std::max(0.0, (incumbent_min - best_min) / denom);
      if (gap <= config.mip_rel_gap) {
        gap_stop = true;
        break;
      }
    }
  }

  if (queue.empty() && !limit_hit) exhausted = true;

  out.nodes = nodes;
  out.solve_time_sec = secondsSince(t_solve);
  if (unbounded_child_skipped) {
    out.warnings.push_back(
        "an unbounded node LP relaxation was skipped; reported bound may be incomplete");
  }

  if (!has_incumbent) {
    if (limit_hit) {
      out.status = limit_status;
      out.termination_reason = limit_reason + "; no feasible integer solution found after " +
                               std::to_string(nodes) + " nodes";
      return out;
    }
    out.status = SolveStatus::Infeasible;
    out.termination_reason =
        "branch and bound exhausted without an integer feasible solution after " +
        std::to_string(nodes) + " nodes";
    return out;
  }

  out.x = presolved.restorePrimal(incumbent_x);
  out.feasible_point = true;
  out.objective = model.evaluateObjective(out.x);

  double best_min = incumbent_min;
  if (!queue.empty()) best_min = std::min(incumbent_min, queue.top().bound_min);
  const double denom = std::max(1.0, std::fabs(incumbent_min));
  out.mip_gap = std::max(0.0, (incumbent_min - best_min) / denom);
  out.best_bound = sense == ObjectiveSense::Maximize ? -best_min : best_min;

  if (limit_hit) {
    out.status = SolveStatus::Feasible;
    out.termination_reason = limit_reason + " after " + std::to_string(nodes) +
                             " nodes; returning incumbent with MIP gap " +
                             std::to_string(out.mip_gap);
  } else if (exhausted) {
    out.status = SolveStatus::Optimal;
    out.mip_gap = 0.0;
    out.best_bound = out.objective;
    out.termination_reason =
        "branch and bound exhausted; proven optimal after " + std::to_string(nodes) +
        " nodes";
  } else if (gap_stop) {
    out.status = SolveStatus::Optimal;
    out.termination_reason = "optimal within MIP gap " + std::to_string(config.mip_rel_gap) +
                             "; gap " + std::to_string(out.mip_gap) + " after " +
                             std::to_string(nodes) + " nodes";
  } else {
    out.status = SolveStatus::Optimal;
    out.termination_reason = "branch and bound completed after " + std::to_string(nodes) +
                             " nodes";
  }

  if (config.verify) {
    const auto t_verify = Clock::now();
    const VerificationReport primal = verifyPrimal(model, out.x, config.tol);
    out.primal_violation = primal.max_primal_violation;
    const bool integral_ok = model.isIntegralFeasible(out.x, config.tol.integrality);
    if (primal.primal_ok && integral_ok) {
      out.verified = true;
    } else {
      out.verified = false;
      out.status = SolveStatus::NumericalError;
      out.termination_reason = "incumbent failed independent verification: " + primal.detail +
                               (integral_ok ? "" : "; integrality violated");
    }
    out.verify_time_sec = secondsSince(t_verify);
  }

  (void)incumbent_obj;
  return out;
}

}  // namespace solver
