#include "solver/lp/simplex.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <sstream>

#include "solver/linear_algebra/dense_lu.hpp"

namespace solver {

const char* toString(SimplexStatus s) {
  switch (s) {
    case SimplexStatus::Optimal: return "OPTIMAL";
    case SimplexStatus::Infeasible: return "INFEASIBLE";
    case SimplexStatus::Unbounded: return "UNBOUNDED";
    case SimplexStatus::IterationLimit: return "ITERATION_LIMIT";
    case SimplexStatus::TimeLimit: return "TIME_LIMIT";
    case SimplexStatus::NumericalError: return "NUMERICAL_ERROR";
  }
  return "UNKNOWN";
}

SimplexSolver::SimplexSolver(const StandardLP& problem, SimplexConfig config)
    : P_(problem), cfg_(std::move(config)), m_(problem.m), n_(problem.n) {}

bool SimplexSolver::timeExceeded() const {
  if (!std::isfinite(cfg_.time_limit_sec)) return false;
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(now - start_).count() > cfg_.time_limit_sec;
}

bool SimplexSolver::initialize() {
  stat_.assign(static_cast<std::size_t>(n_), AtLower);
  basis_.assign(static_cast<std::size_t>(m_), -1);
  xB_.assign(static_cast<std::size_t>(m_), 0.0);
  Binv_.assign(static_cast<std::size_t>(m_) * static_cast<std::size_t>(m_), 0.0);
  y_.assign(static_cast<std::size_t>(m_), 0.0);
  d_.assign(static_cast<std::size_t>(m_), 0.0);
  ub_eff_ = P_.ub;
  cur_c_.assign(static_cast<std::size_t>(n_), 0.0);

  for (int i = 0; i < m_; ++i) {
    if (P_.b[static_cast<std::size_t>(i)] < -cfg_.tol.primal_feasibility) return false;
    const int col = P_.artificial_col[static_cast<std::size_t>(i)] >= 0
                        ? P_.artificial_col[static_cast<std::size_t>(i)]
                        : P_.slack_col[static_cast<std::size_t>(i)];
    if (col < 0) return false;
    basis_[static_cast<std::size_t>(i)] = col;
    stat_[static_cast<std::size_t>(col)] = Basic;
    xB_[static_cast<std::size_t>(i)] = std::max(0.0, P_.b[static_cast<std::size_t>(i)]);
  }
  for (int i = 0; i < m_; ++i) {
    Binv_[static_cast<std::size_t>(i * m_ + i)] = 1.0;
  }

  for (int j = 0; j < n_; ++j) {
    cur_c_[static_cast<std::size_t>(j)] = P_.isArtificial(j) ? 1.0 : 0.0;
  }
  return true;
}

bool SimplexSolver::refactorize() {
  if (m_ == 0) return true;
  std::vector<double> dense(static_cast<std::size_t>(m_) * static_cast<std::size_t>(m_), 0.0);
  for (int q = 0; q < m_; ++q) {
    const int col = basis_[static_cast<std::size_t>(q)];
    const auto& cp = P_.A.colPtr();
    const auto& ri = P_.A.rowIdx();
    const auto& vv = P_.A.values();
    for (int k = cp[static_cast<std::size_t>(col)]; k < cp[static_cast<std::size_t>(col) + 1]; ++k) {
      dense[static_cast<std::size_t>(ri[static_cast<std::size_t>(k)] * m_ + q)] =
          vv[static_cast<std::size_t>(k)];
    }
  }
  DenseLU lu;
  if (!lu.factorize(dense, m_, cfg_.tol.pivot)) return false;
  if (!lu.invert(Binv_)) return false;

  std::vector<double> x_full;
  buildStdX(x_full);
  std::vector<double> x_nb = x_full;
  for (int i = 0; i < m_; ++i) x_nb[static_cast<std::size_t>(basis_[static_cast<std::size_t>(i)])] = 0.0;
  std::vector<double> ax = P_.A.multiply(x_nb);
  std::vector<double> rhs(static_cast<std::size_t>(m_));
  for (int i = 0; i < m_; ++i) rhs[static_cast<std::size_t>(i)] = P_.b[static_cast<std::size_t>(i)] - ax[static_cast<std::size_t>(i)];
  std::vector<double> new_xB;
  if (!lu.solve(rhs, new_xB)) return false;
  for (int i = 0; i < m_; ++i) xB_[static_cast<std::size_t>(i)] = new_xB[static_cast<std::size_t>(i)];
  since_refactor_ = 0;
  ++refactorizations_;
  return true;
}

void SimplexSolver::buildStdX(std::vector<double>& out) const {
  out.assign(static_cast<std::size_t>(n_), 0.0);
  for (int j = 0; j < n_; ++j) {
    const unsigned char s = stat_[static_cast<std::size_t>(j)];
    if (s == Basic) continue;
    out[static_cast<std::size_t>(j)] = (s == AtUpper) ? ub_eff_[static_cast<std::size_t>(j)]
                                                      : P_.lb[static_cast<std::size_t>(j)];
  }
  for (int i = 0; i < m_; ++i) {
    out[static_cast<std::size_t>(basis_[static_cast<std::size_t>(i)])] =
        xB_[static_cast<std::size_t>(i)];
  }
}

void SimplexSolver::computeY() {
  std::fill(y_.begin(), y_.end(), 0.0);
  if (m_ == 0) return;
  for (int k = 0; k < m_; ++k) {
    const double cb = cur_c_[static_cast<std::size_t>(basis_[static_cast<std::size_t>(k)])];
    if (cb == 0.0) continue;
    for (int i = 0; i < m_; ++i) {
      y_[static_cast<std::size_t>(i)] += cb * Binv_[static_cast<std::size_t>(k * m_ + i)];
    }
  }
}

int SimplexSolver::selectEntering() {
  int best = -1;
  double best_score = cfg_.tol.dual_feasibility;
  for (int j = 0; j < n_; ++j) {
    const unsigned char s = stat_[static_cast<std::size_t>(j)];
    if (s == Basic) continue;
    const double lb = P_.lb[static_cast<std::size_t>(j)];
    const double ub = ub_eff_[static_cast<std::size_t>(j)];
    if (!(ub > lb + cfg_.tol.primal_feasibility)) continue;
    const double r = cur_c_[static_cast<std::size_t>(j)] - P_.A.dotColumn(j, y_);
    double score = 0.0;
    if (s == AtLower) {
      if (r >= -cfg_.tol.dual_feasibility) continue;
      score = -r;
    } else {
      if (r <= cfg_.tol.dual_feasibility) continue;
      score = r;
    }
    if (score > best_score) {
      best_score = score;
      best = j;
    }
  }
  if (best >= 0) entering_dir_ = (stat_[static_cast<std::size_t>(best)] == AtLower) ? 1 : -1;
  return best;
}

int SimplexSolver::selectEnteringBland() {
  int best = -1;
  for (int j = 0; j < n_; ++j) {
    const unsigned char s = stat_[static_cast<std::size_t>(j)];
    if (s == Basic) continue;
    const double lb = P_.lb[static_cast<std::size_t>(j)];
    const double ub = ub_eff_[static_cast<std::size_t>(j)];
    if (!(ub > lb + cfg_.tol.primal_feasibility)) continue;
    const double r = cur_c_[static_cast<std::size_t>(j)] - P_.A.dotColumn(j, y_);
    if (s == AtLower && r < -cfg_.tol.dual_feasibility) {
      best = j;
      entering_dir_ = 1;
      break;
    }
    if (s == AtUpper && r > cfg_.tol.dual_feasibility) {
      best = j;
      entering_dir_ = -1;
      break;
    }
  }
  return best;
}

void SimplexSolver::computeDirection(int e, double& delta, int& pivot_row, bool& own_bound) {
  std::fill(d_.begin(), d_.end(), 0.0);
  const auto& cp = P_.A.colPtr();
  const auto& ri = P_.A.rowIdx();
  const auto& vv = P_.A.values();
  for (int k = cp[static_cast<std::size_t>(e)]; k < cp[static_cast<std::size_t>(e) + 1]; ++k) {
    const double v = vv[static_cast<std::size_t>(k)];
    const int row = ri[static_cast<std::size_t>(k)];
    for (int i = 0; i < m_; ++i) {
      d_[static_cast<std::size_t>(i)] += Binv_[static_cast<std::size_t>(i * m_ + row)] * v;
    }
  }

  const double xe = (stat_[static_cast<std::size_t>(e)] == AtLower)
                        ? P_.lb[static_cast<std::size_t>(e)]
                        : ub_eff_[static_cast<std::size_t>(e)];
  pivot_row = -1;
  own_bound = false;

  if (entering_dir_ > 0) {
    double own = kInfinity;
    if (std::isfinite(ub_eff_[static_cast<std::size_t>(e)])) {
      own = ub_eff_[static_cast<std::size_t>(e)] - xe;
    }
    double best_cap = kInfinity;
    int best_row = -1;
    for (int i = 0; i < m_; ++i) {
      const double di = d_[static_cast<std::size_t>(i)];
      if (std::fabs(di) <= cfg_.tol.pivot) continue;
      const int bi = basis_[static_cast<std::size_t>(i)];
      double cap;
      if (di > 0.0) {
        cap = (xB_[static_cast<std::size_t>(i)] - P_.lb[static_cast<std::size_t>(bi)]) / di;
      } else {
        const double bu = ub_eff_[static_cast<std::size_t>(bi)];
        if (!std::isfinite(bu)) continue;
        cap = (xB_[static_cast<std::size_t>(i)] - bu) / di;
      }
      if (cap < best_cap - cfg_.tol.zero) {
        best_cap = cap;
        best_row = i;
      } else if (best_row >= 0 && std::fabs(cap - best_cap) <= cfg_.tol.zero && i < best_row) {
        best_row = i;
      } else if (best_row < 0 && cap <= best_cap + cfg_.tol.zero) {
        best_cap = cap;
        best_row = i;
      }
    }
    if (best_row >= 0 && best_cap <= own + std::max(cfg_.tol.zero, 1e-12)) {
      delta = best_cap;
      pivot_row = best_row;
    } else {
      delta = own;
      own_bound = true;
    }
  } else {
    double own = -kInfinity;
    if (std::isfinite(P_.lb[static_cast<std::size_t>(e)])) {
      own = P_.lb[static_cast<std::size_t>(e)] - xe;
    }
    double best_cap = -kInfinity;
    int best_row = -1;
    for (int i = 0; i < m_; ++i) {
      const double di = d_[static_cast<std::size_t>(i)];
      if (std::fabs(di) <= cfg_.tol.pivot) continue;
      const int bi = basis_[static_cast<std::size_t>(i)];
      double cap;
      if (di < 0.0) {
        cap = (xB_[static_cast<std::size_t>(i)] - P_.lb[static_cast<std::size_t>(bi)]) / di;
      } else {
        const double bu = ub_eff_[static_cast<std::size_t>(bi)];
        if (!std::isfinite(bu)) continue;
        cap = (xB_[static_cast<std::size_t>(i)] - bu) / di;
      }
      if (cap > best_cap + cfg_.tol.zero) {
        best_cap = cap;
        best_row = i;
      } else if (best_row >= 0 && std::fabs(cap - best_cap) <= cfg_.tol.zero && i < best_row) {
        best_row = i;
      } else if (best_row < 0 && cap >= best_cap - cfg_.tol.zero) {
        best_cap = cap;
        best_row = i;
      }
    }
    if (best_row >= 0 && best_cap >= own - std::max(cfg_.tol.zero, 1e-12)) {
      delta = best_cap;
      pivot_row = best_row;
    } else {
      delta = own;
      own_bound = true;
    }
  }
}

void SimplexSolver::updateBinv(int pivot_row, const std::vector<double>& d) {
  const double dp = d[static_cast<std::size_t>(pivot_row)];
  for (int k = 0; k < m_; ++k) {
    Binv_[static_cast<std::size_t>(pivot_row * m_ + k)] /= dp;
  }
  for (int i = 0; i < m_; ++i) {
    if (i == pivot_row) continue;
    const double f = d[static_cast<std::size_t>(i)];
    if (f == 0.0) continue;
    for (int k = 0; k < m_; ++k) {
      Binv_[static_cast<std::size_t>(i * m_ + k)] -=
          f * Binv_[static_cast<std::size_t>(pivot_row * m_ + k)];
    }
  }
}

void SimplexSolver::applyMove(int entering, double delta, int pivot_row, bool own_bound) {
  for (int i = 0; i < m_; ++i) {
    double v = xB_[static_cast<std::size_t>(i)] - d_[static_cast<std::size_t>(i)] * delta;
    const int bi = basis_[static_cast<std::size_t>(i)];
    const double lo = P_.lb[static_cast<std::size_t>(bi)];
    const double hi = ub_eff_[static_cast<std::size_t>(bi)];
    if (v < lo) v = (v >= lo - cfg_.tol.primal_feasibility) ? lo : v;
    if (v > hi) v = (v <= hi + cfg_.tol.primal_feasibility) ? hi : v;
    xB_[static_cast<std::size_t>(i)] = v;
  }

  if (own_bound) {
    stat_[static_cast<std::size_t>(entering)] = (entering_dir_ > 0) ? AtUpper : AtLower;
    return;
  }

  const int p = pivot_row;
  const int old_col = basis_[static_cast<std::size_t>(p)];
  const double lo = P_.lb[static_cast<std::size_t>(old_col)];
  const double hi = ub_eff_[static_cast<std::size_t>(old_col)];
  const double old_value_after = xB_[static_cast<std::size_t>(p)];
  const bool hit_upper = std::isfinite(hi) && std::fabs(old_value_after - hi) <=
                                                std::fabs(old_value_after - lo);
  stat_[static_cast<std::size_t>(old_col)] = hit_upper ? AtUpper : AtLower;

  const double x_entering =
      (stat_[static_cast<std::size_t>(entering)] == AtLower)
          ? P_.lb[static_cast<std::size_t>(entering)]
          : ub_eff_[static_cast<std::size_t>(entering)];
  stat_[static_cast<std::size_t>(entering)] = Basic;
  basis_[static_cast<std::size_t>(p)] = entering;
  xB_[static_cast<std::size_t>(p)] = x_entering + delta;
  updateBinv(p, d_);
  ++since_refactor_;
}

double SimplexSolver::phaseObjective() {
  double acc = 0.0;
  for (int i = 0; i < m_; ++i) {
    acc += cur_c_[static_cast<std::size_t>(basis_[static_cast<std::size_t>(i)])] *
           xB_[static_cast<std::size_t>(i)];
  }
  for (int j = 0; j < n_; ++j) {
    if (stat_[static_cast<std::size_t>(j)] == Basic) continue;
    const double x = (stat_[static_cast<std::size_t>(j)] == AtUpper)
                         ? ub_eff_[static_cast<std::size_t>(j)]
                         : P_.lb[static_cast<std::size_t>(j)];
    acc += cur_c_[static_cast<std::size_t>(j)] * x;
  }
  return acc;
}

SimplexStatus SimplexSolver::runPhase() {
  while (true) {
    if (iterations_ >= cfg_.max_iterations) return SimplexStatus::IterationLimit;
    if ((iterations_ & 15) == 0 && timeExceeded()) return SimplexStatus::TimeLimit;
    if (m_ > 0 && since_refactor_ >= cfg_.refactor_interval) {
      if (!refactorize()) return SimplexStatus::NumericalError;
    }

    computeY();
    const int e = use_bland_ ? selectEnteringBland() : selectEntering();
    if (getenv("SOLVER_SIMPLEX_TRACE")) {
      std::fprintf(stderr, "[it %ld] entering=%d dir=%d y=[", iterations_, e, entering_dir_);
      for (int i = 0; i < m_; ++i) std::fprintf(stderr, "%g%s", y_[static_cast<std::size_t>(i)], i + 1 < m_ ? " " : "");
      std::fprintf(stderr, "] xB=[");
      for (int i = 0; i < m_; ++i) std::fprintf(stderr, "%g%s", xB_[static_cast<std::size_t>(i)], i + 1 < m_ ? " " : "");
      std::fprintf(stderr, "] basis=[");
      for (int i = 0; i < m_; ++i) std::fprintf(stderr, "%d%s", basis_[static_cast<std::size_t>(i)], i + 1 < m_ ? " " : "");
      std::fprintf(stderr, "]\n");
    }
    if (e < 0) return SimplexStatus::Optimal;

    double delta = 0.0;
    int pivot_row = -1;
    bool own_bound = false;
    computeDirection(e, delta, pivot_row, own_bound);

    if (!std::isfinite(delta)) return SimplexStatus::Unbounded;
    if (pivot_row >= 0) {
      const double dp = d_[static_cast<std::size_t>(pivot_row)];
      if (std::fabs(dp) < cfg_.tol.pivot) {
        if (!refactorize()) return SimplexStatus::NumericalError;
        computeDirection(e, delta, pivot_row, own_bound);
        if (pivot_row >= 0 &&
            std::fabs(d_[static_cast<std::size_t>(pivot_row)]) < cfg_.tol.pivot) {
          return SimplexStatus::NumericalError;
        }
        if (!std::isfinite(delta)) return SimplexStatus::Unbounded;
      }
    }

    if (std::fabs(delta) <= cfg_.tol.zero) {
      ++stall_;
      if (stall_ >= cfg_.stall_bland_threshold) use_bland_ = true;
    } else {
      stall_ = 0;
    }

    applyMove(e, delta, pivot_row, own_bound);
    ++iterations_;
  }
}

void SimplexSolver::driveOutArtificials() {
  if (P_.first_artificial_col < 0) return;
  for (int i = 0; i < m_; ++i) {
    const int col = basis_[static_cast<std::size_t>(i)];
    if (col < 0 || !P_.isArtificial(col)) continue;
    if (std::fabs(xB_[static_cast<std::size_t>(i)]) > cfg_.tol.primal_feasibility) continue;

    int candidate = -1;
    for (int j = 0; j < n_; ++j) {
      if (stat_[static_cast<std::size_t>(j)] == Basic || P_.isArtificial(j)) continue;
      if (!(ub_eff_[static_cast<std::size_t>(j)] > P_.lb[static_cast<std::size_t>(j)] +
                                                    cfg_.tol.primal_feasibility))
        continue;
      double entry = 0.0;
      const auto& cp = P_.A.colPtr();
      const auto& ri = P_.A.rowIdx();
      const auto& vv = P_.A.values();
      for (int k = cp[static_cast<std::size_t>(j)]; k < cp[static_cast<std::size_t>(j) + 1]; ++k) {
        entry += Binv_[static_cast<std::size_t>(i * m_ + ri[static_cast<std::size_t>(k)])] *
                 vv[static_cast<std::size_t>(k)];
      }
      if (std::fabs(entry) > cfg_.tol.pivot) {
        candidate = j;
        break;
      }
    }
    if (candidate < 0) continue;

    std::fill(d_.begin(), d_.end(), 0.0);
    const auto& cp = P_.A.colPtr();
    const auto& ri = P_.A.rowIdx();
    const auto& vv = P_.A.values();
    for (int k = cp[static_cast<std::size_t>(candidate)]; k < cp[static_cast<std::size_t>(candidate) + 1]; ++k) {
      const double v = vv[static_cast<std::size_t>(k)];
      const int row = ri[static_cast<std::size_t>(k)];
      for (int r = 0; r < m_; ++r) {
        d_[static_cast<std::size_t>(r)] += Binv_[static_cast<std::size_t>(r * m_ + row)] * v;
      }
    }
    const int old_col = basis_[static_cast<std::size_t>(i)];
    stat_[static_cast<std::size_t>(old_col)] = AtLower;
    stat_[static_cast<std::size_t>(candidate)] = Basic;
    basis_[static_cast<std::size_t>(i)] = candidate;
    updateBinv(i, d_);
    ++since_refactor_;
    if (since_refactor_ >= cfg_.refactor_interval) {
      if (!refactorize()) break;
    }
  }
}

void SimplexSolver::preparePhase2() {
  cur_c_ = P_.c;
  for (int j = 0; j < n_; ++j) {
    if (!P_.isArtificial(j)) continue;
    if (stat_[static_cast<std::size_t>(j)] != Basic) stat_[static_cast<std::size_t>(j)] = AtLower;
    ub_eff_[static_cast<std::size_t>(j)] = 0.0;
  }
  for (int i = 0; i < m_; ++i) {
    const int col = basis_[static_cast<std::size_t>(i)];
    if (col >= 0 && P_.isArtificial(col)) xB_[static_cast<std::size_t>(i)] = 0.0;
  }
}

void SimplexSolver::finalize(SimplexSolution& sol) const {
  sol.x.assign(static_cast<std::size_t>(n_), 0.0);
  sol.reduced_costs.assign(static_cast<std::size_t>(n_), 0.0);
  sol.y.assign(static_cast<std::size_t>(m_), 0.0);

  std::vector<double> x_nb;
  {
    for (int j = 0; j < n_; ++j) {
      const unsigned char s = stat_[static_cast<std::size_t>(j)];
      if (s == Basic) continue;
      sol.x[static_cast<std::size_t>(j)] = (s == AtUpper)
                                               ? ub_eff_[static_cast<std::size_t>(j)]
                                               : P_.lb[static_cast<std::size_t>(j)];
    }
    x_nb = sol.x;
    for (int i = 0; i < m_; ++i) x_nb[static_cast<std::size_t>(basis_[static_cast<std::size_t>(i)])] = 0.0;
  }

  if (m_ > 0) {
    std::vector<double> dense(static_cast<std::size_t>(m_) * static_cast<std::size_t>(m_), 0.0);
    for (int q = 0; q < m_; ++q) {
      const int col = basis_[static_cast<std::size_t>(q)];
      const auto& cp = P_.A.colPtr();
      const auto& ri = P_.A.rowIdx();
      const auto& vv = P_.A.values();
      for (int k = cp[static_cast<std::size_t>(col)]; k < cp[static_cast<std::size_t>(col) + 1]; ++k) {
        dense[static_cast<std::size_t>(ri[static_cast<std::size_t>(k)] * m_ + q)] =
            vv[static_cast<std::size_t>(k)];
      }
    }
    DenseLU lu;
    if (!lu.factorize(dense, m_, cfg_.tol.pivot)) {
      sol.status = SimplexStatus::NumericalError;
      sol.message = "final basis factorization failed";
      return;
    }
    std::vector<double> ax = P_.A.multiply(x_nb);
    std::vector<double> rhs(static_cast<std::size_t>(m_));
    for (int i = 0; i < m_; ++i) {
      rhs[static_cast<std::size_t>(i)] = P_.b[static_cast<std::size_t>(i)] - ax[static_cast<std::size_t>(i)];
    }
    std::vector<double> xB_final;
    if (!lu.solve(rhs, xB_final)) {
      sol.status = SimplexStatus::NumericalError;
      sol.message = "final basis solve failed";
      return;
    }
    for (int i = 0; i < m_; ++i) {
      double v = xB_final[static_cast<std::size_t>(i)];
      const int bi = basis_[static_cast<std::size_t>(i)];
      const double lo = P_.lb[static_cast<std::size_t>(bi)];
      const double hi = ub_eff_[static_cast<std::size_t>(bi)];
      if (std::isfinite(lo) && v < lo && v >= lo - cfg_.tol.primal_feasibility) v = lo;
      if (std::isfinite(hi) && v > hi && v <= hi + cfg_.tol.primal_feasibility) v = hi;
      sol.x[static_cast<std::size_t>(bi)] = v;
    }

    std::vector<double> cB(static_cast<std::size_t>(m_));
    for (int i = 0; i < m_; ++i) {
      cB[static_cast<std::size_t>(i)] = P_.c[static_cast<std::size_t>(basis_[static_cast<std::size_t>(i)])];
    }
    if (!lu.solveTranspose(cB, sol.y)) {
      sol.status = SimplexStatus::NumericalError;
      sol.message = "dual solve failed";
      return;
    }
  }

  double primal = 0.0;
  for (int j = 0; j < n_; ++j) {
    primal += P_.c[static_cast<std::size_t>(j)] * sol.x[static_cast<std::size_t>(j)];
    sol.reduced_costs[static_cast<std::size_t>(j)] =
        P_.c[static_cast<std::size_t>(j)] - P_.A.dotColumn(j, sol.y);
  }
  sol.primal_objective = primal;

  double dual = 0.0;
  for (int i = 0; i < m_; ++i) dual += P_.b[static_cast<std::size_t>(i)] * sol.y[static_cast<std::size_t>(i)];
  for (int j = 0; j < n_; ++j) {
    const double r = sol.reduced_costs[static_cast<std::size_t>(j)];
    if (r >= 0.0) continue;
    const double ub = ub_eff_[static_cast<std::size_t>(j)];
    if (!std::isfinite(ub)) {
      dual = -kInfinity;
      break;
    }
    dual += r * ub;
  }
  sol.dual_objective = dual;
  sol.gap = dual == -kInfinity ? kInfinity : (primal - dual);
  const double gap_tol = 1e-5 * (1.0 + std::fabs(primal));
  sol.certificate = std::isfinite(sol.gap) && sol.gap <= gap_tol;
}

SimplexSolution SimplexSolver::solve() {
  start_ = std::chrono::steady_clock::now();
  SimplexSolution sol;
  if (!initialize()) {
    sol.status = SimplexStatus::NumericalError;
    sol.message = "simplex initialization failed (negative rhs or missing basis column)";
    return sol;
  }

  const bool has_artificials = P_.first_artificial_col >= 0;
  if (has_artificials && m_ > 0) {
    use_bland_ = false;
    stall_ = 0;
    since_refactor_ = 0;
    const SimplexStatus st = runPhase();
    phase1_iterations_ = iterations_;
    if (st == SimplexStatus::IterationLimit) {
      sol.status = st;
      sol.message = "phase-1 iteration limit reached";
      sol.iterations = iterations_;
      return sol;
    }
    if (st == SimplexStatus::TimeLimit) {
      sol.status = st;
      sol.message = "time limit reached during phase 1";
      sol.iterations = iterations_;
      return sol;
    }
    if (st == SimplexStatus::NumericalError) {
      sol.status = st;
      sol.message = "numerical failure during phase 1";
      sol.iterations = iterations_;
      return sol;
    }
    if (st == SimplexStatus::Unbounded) {
      sol.status = SimplexStatus::NumericalError;
      sol.message = "phase 1 reported unboundedness (should be impossible)";
      sol.iterations = iterations_;
      return sol;
    }

    const double p1 = phaseObjective();
    double b_norm = 0.0;
    for (int i = 0; i < m_; ++i) b_norm += std::fabs(P_.b[static_cast<std::size_t>(i)]);
    const double infeas_tol = cfg_.tol.primal_feasibility * (1.0 + b_norm);
    if (p1 > infeas_tol) {
      sol.status = SimplexStatus::Infeasible;
      std::ostringstream os;
      os << "phase-1 residual " << p1 << " exceeds tolerance " << infeas_tol;
      std::vector<int> offenders;
      for (int i = 0; i < m_ && offenders.size() < 8; ++i) {
        const int col = basis_[static_cast<std::size_t>(i)];
        if (col >= 0 && P_.isArtificial(col) &&
            xB_[static_cast<std::size_t>(i)] > cfg_.tol.primal_feasibility) {
          offenders.push_back(P_.std_row_orig[static_cast<std::size_t>(i)]);
        }
      }
      if (!offenders.empty()) {
        os << "; unresolved constraints:";
        for (int idx : offenders) os << " c" << idx;
      }
      sol.message = os.str();
      sol.iterations = iterations_;
      sol.phase1_iterations = phase1_iterations_;
      sol.solve_time_sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
      return sol;
    }

    driveOutArtificials();
  }

  preparePhase2();
  use_bland_ = false;
  stall_ = 0;
  const SimplexStatus st = runPhase();
  sol.iterations = iterations_;
  sol.phase1_iterations = phase1_iterations_;
  sol.refactorizations = refactorizations_;
  sol.solve_time_sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();

  if (st == SimplexStatus::NumericalError) {
    sol.status = st;
    sol.message = "numerical failure during phase 2";
    return sol;
  }
  if (st == SimplexStatus::Unbounded) {
    finalize(sol);
    sol.status = SimplexStatus::Unbounded;
    sol.message = "unbounded ray detected: objective can improve indefinitely";
    sol.feasible_point = true;
    return sol;
  }

  finalize(sol);
  if (sol.status == SimplexStatus::NumericalError) return sol;
  sol.feasible_point = true;

  if (st == SimplexStatus::IterationLimit) {
    sol.status = st;
    sol.message = "iteration limit reached; returning best feasible point";
    return sol;
  }
  if (st == SimplexStatus::TimeLimit) {
    sol.status = st;
    sol.message = "time limit reached; returning best feasible point";
    return sol;
  }

  if (!sol.certificate) {
    sol.status = SimplexStatus::NumericalError;
    std::ostringstream os;
    os << "optimality certificate failed: primal=" << sol.primal_objective
       << " dual=" << sol.dual_objective << " gap=" << sol.gap;
    sol.message = os.str();
    return sol;
  }
  sol.status = SimplexStatus::Optimal;
  return sol;
}

}  // namespace solver
