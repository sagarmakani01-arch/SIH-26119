#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "solver/lp/standard_problem.hpp"
#include "solver/numerical/tolerances.hpp"

namespace solver {

enum class SimplexStatus {
  Optimal,
  Infeasible,
  Unbounded,
  IterationLimit,
  TimeLimit,
  NumericalError
};

const char* toString(SimplexStatus s);

struct SimplexConfig {
  Tolerances tol;
  long max_iterations = 1000000;
  int refactor_interval = 60;
  double time_limit_sec = kInfinity;
  int stall_bland_threshold = 40;
};

struct SimplexSolution {
  SimplexStatus status = SimplexStatus::Optimal;
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> reduced_costs;
  double primal_objective = 0.0;
  double dual_objective = 0.0;
  double gap = kInfinity;
  bool certificate = false;
  bool feasible_point = false;
  long iterations = 0;
  long phase1_iterations = 0;
  int refactorizations = 0;
  double solve_time_sec = 0.0;
  std::string message;
};

class SimplexSolver {
 public:
  SimplexSolver(const StandardLP& problem, SimplexConfig config);

  SimplexSolution solve();

 private:
  enum Stat : unsigned char { AtLower = 0, AtUpper = 1, Basic = 2 };

  bool initialize();
  bool refactorize();
  void buildStdX(std::vector<double>& out) const;
  void computeY();
  void computeDirection(int entering, double& delta, int& pivot_row, bool& own_bound);
  int selectEntering();
  int selectEnteringBland();
  void applyMove(int entering, double delta, int pivot_row, bool own_bound);
  void updateBinv(int pivot_row, const std::vector<double>& d);
  void driveOutArtificials();
  void preparePhase2();
  SimplexStatus runPhase();
  double phaseObjective();
  bool timeExceeded() const;
  void finalize(SimplexSolution& sol) const;

  const StandardLP& P_;
  SimplexConfig cfg_;
  int m_ = 0;
  int n_ = 0;

  std::vector<int> basis_;
  std::vector<unsigned char> stat_;
  std::vector<double> xB_;
  std::vector<double> Binv_;
  std::vector<double> ub_eff_;
  std::vector<double> y_;
  std::vector<double> d_;
  std::vector<double> cB_;
  std::vector<double> cur_c_;

  int entering_dir_ = 1;
  long iterations_ = 0;
  long phase1_iterations_ = 0;
  int refactorizations_ = 0;
  int since_refactor_ = 0;
  long stall_ = 0;
  bool use_bland_ = false;
  std::chrono::steady_clock::time_point start_;
};

}  // namespace solver
