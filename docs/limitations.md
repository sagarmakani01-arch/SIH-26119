# Limitations

Honest limits of the current prototype. Nothing here is hidden behind a success status.

## Environment

- **No CUDA/GPU on the development machine.** The GPU backend compiles only when
  `SOLVER_ENABLE_CUDA=ON`. On this machine a `GPU` request resolves to `CPU` with an
  explicit warning in `SolverResult.warnings`. No speedup is ever reported without an
  executed GPU measurement.

## Solver core (Phase 3–4 scope)

- **LP + MILP only.** Quadratic objectives are rejected with an explicit `ERROR` status
  until Phase 5 lands.
- **Branch & bound is basic.** Best-bound node selection, most-fractional branching, a
  rounding heuristic, and gap-based termination. No cutting planes, no strong branching,
  no RINS/Local-branching heuristics, no MIP presolve beyond the LP presolve rules.
  Hard instances will hit node/time limits rather than prove optimality quickly.
- **Primal simplex only.** `LPAlgorithm::{DualSimplex, InteriorPoint}` exist in the API but
  return `ERROR` ("not implemented in this build") if requested. Node LP relaxations all go
  through the primal simplex; dual warm-starts between nodes are not implemented.
- **Dense `B_inv`.** The revised simplex keeps an explicit dense inverse of the basis
  (rank-1 updates, periodic refactorization). Memory is O(m²) and each iteration is O(m²),
  so problems beyond a few thousand rows are out of scope until sparse basis methods land.
- **Dense `B_inv` vs dependent rows.** If constraint rows are linearly dependent, phase 1
  may keep frozen artificial columns in the basis; this is numerically stable but leaves
  redundant rows with a non-basic artificial at zero (reported through diagnostics, not
  hidden).
- **Single-threaded.** `SolverConfig.threads > 1` records a warning; one thread is used.
- **No MPS/LP file formats yet.** Models enter through the C++ API or the JSON bridge
  (`solver_cli`, used by the FastAPI layer and the web app); there is no MPS/LP
  reader/writer.
- **Tolerances are fixed defaults** (`Tolerances` in `numerical/tolerances.hpp`); there is
  no automatic tolerance scaling or iterative refinement yet.
- **Scaling.** Problems are solved unscaled; badly scaled data (coefficients spanning many
  orders of magnitude) may hit iteration/numerical limits and will be surfaced as
  `ITERATION_LIMIT`/`NUMERICAL_ERROR`, not masked.

## Verification

- `OPTIMAL` requires both an optimizer-side gap certificate and an independent
  original-space KKT check (primal feasibility, dual sign/stationarity/complementarity).
  If the dual check fails while the primal point is feasible, status is **downgraded to
  `FEASIBLE`** with a warning — never reported as optimal.
- Verification costs are O(nnz · n) as implemented (per-variable gradient scans); fine for
  demo-scale models, not for very large instances.

## Testing

- Known-answer tests are analytic or hand-computed; SciPy/HiGHS cross-checks are planned as
  an offline oracle script (never shipped as the production engine).
- No randomized/adversarial fuzzing of the simplex yet.

## Platform

- Backend/frontend phases are not implemented yet; the current deliverable is the solver
  core with its test suite.
