# Architecture

## Two layers

1. **Solver core (C++20)** — models, sparse linear algebra, presolve, LP/MILP/QP engines,
   numerical verification, compute backends, instrumentation. No dependency on the platform
   layer; buildable and testable standalone (CMake + CTest).
2. **Platform layer (FastAPI + React/TS)** — REST API, jobs, persistence, audit trail,
   visualization. It invokes the core through a narrow CLI/JSON (later pybind) boundary.
   The frontend never contains authoritative solver logic.

```
User → Web Dashboard → FastAPI → Model/JobManager → native solver (subprocess)
                                    → Solver Core: presolve → standardize → LP → MILP/QP
                                    → CPUBackend / GPUBackend
                          ← results/charts ← DB ← audit trail
```

## Solve pipeline (LP, implemented)

```
OptimizationModel
  │  validate()                     errors → ERROR status with diagnostics
  │  presolveModel()                fixed-var elimination, empty/singleton/duplicate/redundant
  │                                 rows, bound tightening; reversible via restorePrimal/mapDuals
  │                                 infeasibility → INFEASIBLE with reason (no solver run)
  │  standardize()                  all-equality form, lb=0 columns (SHIFT/REFLECT/SPLIT/FIXED),
  │                                 slacks (+1 for ≤, −1 for ≥), artificials only where needed,
  │                                 row negation when b < 0 (std_row_sign tracks dual flips)
  │  SimplexSolver                  two-phase revised simplex, bounded variables, explicit B_inv
  │                                 with periodic refactorization (DenseLU), Bland anti-cycling,
  │                                 iteration/time limits, primal + dual objectives and gap
  │  postsolve                      expand → restorePrimal (original space),
  │                                 duals: λ = std_row_sign · y, then mapDuals (dropped rows → 0)
  │  verifyPrimal/verifyDual        independent check on the ORIGINAL model:
  │                                 bounds + row violations; dual sign conditions, bound
  │                                 stationarity, complementary slackness
  │                                 failed certificate → status downgraded to FEASIBLE
  ▼
SolverResult (status, objective, x, duals, reduced_costs, activities, statistics, warnings)
```

## Solve pipeline (MILP, implemented)

```
OptimizationModel (integer/binary variables)
  │  presolve (root)                 same rules + integer bound rounding (ceil/floor) +
  │                                  fractional-pinned integer ⇒ INFEASIBLE
  │  root LP relaxation              LPSolver on the continuous relaxation
  │                                  unbounded relaxation → UNBOUNDED (+ warning)
  │  rounding heuristic              nearest-integer candidate, primal-verified → incumbent
  │  branch & bound loop             best-bound queue, most-fractional variable branching
  │                                  (x ≤ ⌊x̄⌋ / x ≥ ⌈x̄⌉), bound pruning vs incumbent,
  │                                  node/time/LP-iteration limits, MIP gap ≤ mip_rel_gap
  │  postsolve + verify              restorePrimal (original space); verifyPrimal on the
  │                                  ORIGINAL model + integrality; failure ⇒ NUMERICAL_ERROR
  ▼
SolverResult (status, objective, x, mip gap, node/iteration statistics, warnings)
```

Duals/reduced costs are reported for LP only; MILP incumbents carry primal verification
and the MIP gap instead of a dual certificate.

Key invariant: the optimizer's own certificate is never trusted. `OPTIMAL` is only returned
after the original-space verifier passes; otherwise the result is downgraded (e.g. to
`FEASIBLE`) or reported as `NUMERICAL_ERROR` with diagnostics.

## Module map (solver core)

| Module | Path | Role |
|---|---|---|
| core types | `solver/include/solver/core/types.hpp` | VarType, senses, SolveStatus, backends |
| model | `solver/model/*`, `solver/src/model/*` | variables, constraints, objective, validation, CSC cache |
| sparse | `solver/model/sparse_matrix.hpp` | triplets → CSC, multiply, transpose |
| presolve | `solver/presolve/*`, `solver/src/presolve/*` | reductions with reversible restore maps |
| standardization | `solver/lp/standard_problem.hpp`, `src/lp/standardize.cpp` | column transforms, slack/artificial placement |
| simplex | `solver/lp/simplex.hpp`, `src/lp/simplex.cpp` | two-phase bounded-variable revised simplex |
| verification | `solver/numerical/verification.*` | primal/dual KKT checks, reduced costs |
| LP facade | `solver/lp/lp_solver.*` | validate → presolve → standardize → solve → verify |
| MILP | `solver/mip/branch_and_bound.*` | branch & bound over LP relaxations, gap control |
| top API | `solver/solver.hpp`, `solver/src/solver.cpp` | `SolverConfig`/`SolverResult`, problem-class dispatch |
| linear algebra | `solver/linear_algebra/dense_lu.*` | LU factorization/solve/invert with partial pivoting |

## GPU backend strategy

`ComputeBackendKind::{Auto, CPU, GPU}` is part of `SolverConfig`. Backends sit behind a
narrow interface so the simplex (and later MIP/QP kernels) can offload dense kernels
(`B_inv` updates, matrix products). CUDA sources compile only with `SOLVER_ENABLE_CUDA=ON`;
on CPU-only builds a `GPU` request resolves to `CPU` with an explicit warning recorded in
`SolverResult.warnings`. Benchmarks report GPU numbers only from executed GPU runs.

## Platform layer (Phase 7, implemented)

FastAPI exposes `GET /api/health`, `GET /api/demo`, `POST /api/validate` and
`POST /api/solve`; each request runs `solver_cli` as a native subprocess speaking JSON on
stdin/stdout (timeout and crash mapped to HTTP 504/500). The React app (Vite + TypeScript +
Tailwind) renders the vertical workflow — guided templates, model review, solving, results
and what-if scenarios — through an isolated service layer; nginx serves the SPA and proxies
`/api/*` and `/docs` to FastAPI on one origin. Async jobs, audit persistence and the model
registry remain on the roadmap (see `PROJECT_PLAN.md` Phase 7).
