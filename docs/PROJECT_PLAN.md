# PROJECT PLAN — Indigenous GPU-Accelerated Optimization Solver

SIH 2026 · Problem Statement 26119 · Sponsor: MRPL

This document is the living implementation plan. It is updated as phases complete.
Status legend: `TODO` · `IN PROGRESS` · `DONE` · `BLOCKED` · `LIMITED` (works, documented limitation)

---

## 0. Environment (verified on this machine)

| Component | Status |
|---|---|
| MSVC 19.51 (VS 2026 BuildTools, vcvars64) | available |
| CMake 4.4.3, Ninja 1.13 (pip) | available |
| Python 3.14 + numpy/scipy | available |
| Node 24 / npm 11 | available |
| CUDA / nvcc / NVIDIA GPU | **not available** → GPU backend compiles only when `SOLVER_ENABLE_CUDA=ON`; runtime auto-falls back to CPU. No fabricated speedups. |
| C++20 | required standard (`/std:c++20`) |

Consequence: the GPU backend is implemented behind a real `ComputeBackend` interface with
CUDA kernels guarded by `SOLVER_ENABLE_CUDA`. On this machine the `GPU` request resolves to
`CPU` with an explicit `fallback_reason` in the result. Benchmarks are only reported when a
GPU run actually executed.

---

## 1. Architecture

Two strictly separated layers (see `docs/architecture.md`):

1. **Solver Core (C++20, no dependencies on the platform)** — model, sparse linear algebra,
   presolve, LP/MILP/QP engines, numerical verification, backends, instrumentation.
2. **Platform Layer (Python FastAPI + React/TS)** — REST API, jobs, persistence, audit trail,
   visualization. It invokes the core through a narrow CLI/JSON (and later pybind) boundary.
   The frontend never contains authoritative solver logic.

```
User → Web Dashboard → FastAPI → Model Manager/JobManager → native solver (subprocess)
                                                          → Solver Core: presolve → LP → MILP/QP
                                                          → CPUBackend / GPUBackend
                          ← results / charts ← DB ← audit trail
```

---

## 2. Phases

### PHASE 1 — Architecture scaffold — `DONE`
- [x] Repository structure (`solver/`, `gpu/`, `backend/`, `frontend/`, `datasets/`, `docs/`, `scripts/`, `docker/`)
- [x] Root + solver CMake (C++20, MSVC/Ninja, `SOLVER_ENABLE_CUDA` option, CTest)
- [x] Build/test driver scripts (`scripts/build.ps1`, `scripts/test.ps1`)
- [x] This plan + `docs/architecture.md`

### PHASE 2 — Mathematical model layer — `DONE`
- [x] Core types: `VarType`, `ConstraintSense`, `ObjectiveSense`, `SolveStatus`
- [x] CSC sparse matrix + triplet construction with duplicate summation
- [x] `OptimizationModel`: variables, bounds, types, objective, rows, names, metadata
- [x] `LinearExpr` builder for readable model construction
- [x] Validation (NaN/Inf, index ranges, bound consistency, empty rows, duplicate names)
- [x] Unit tests: model creation, bounds, validation, sparse ops, objective evaluation

### PHASE 3 — LP solver — `DONE`
- [x] Standardization (≤ /= / ≥, free/one-sided variables, bound shifting)
- [x] Presolve (fixed/empty/singleton/duplicate rows, bound tightening; reversible via
      `restorePrimal`/`mapDuals`)
- [x] Revised simplex (primal, bounded variables, explicit `B_inv` + refactorization,
      Bland anti-cycling fallback) — dual simplex/IP stubbed behind `LPAlgorithm`, not
      implemented (documented)
- [x] Iteration/time limits, duals & reduced costs, `LPSolver` + `Solver` facades
      (`SolverConfig`/`SolverResult` per spec §7)
- [x] Independent primal/dual feasibility verification before any `OPTIMAL` claim
      (failed certificate ⇒ status downgraded to `FEASIBLE`)
- [x] Known-answer LP tests + infeasible/unbounded tests

Status: **86 tests, 344 checks passing** (run `scripts/test.ps1`). `README.md`,
`docs/architecture.md`, `docs/limitations.md` written.

### PHASE 4 — MILP (branch & bound) — `DONE`
- [x] `BranchAndBound` engine (`solver/mip/branch_and_bound.hpp`): root presolve, LP
      relaxation, most-fractional branching, best-bound node selection, incumbent +
      rounding heuristic, bound pruning, MIP gap (`mip_rel_gap`), node/time/LP-iteration
      limits
- [x] Presolve hardening for integers: integer bound rounding (ceil/floor), rejection of
      integer variables pinned to fractional values (⇒ infeasible)
- [x] `Solver` facade dispatches MILP (`ProblemClass::MixedIntegerLinear`), collects node
      statistics and gap
- [x] Tests: binary knapsack, branching-required min, presolve fractional pin,
      branch-exhausted infeasibility, unbounded relaxation, node-limit → `FEASIBLE` with
      gap, fixed integers, facade dispatch

Status: **96 tests, 388 checks passing** (run `scripts/test.ps1`).
### PHASE 5 — Convex QP — `TODO`
### PHASE 6 — GPU backend + measured benchmarks — `LIMITED` (interface + CPU path; CUDA code behind flag)
### PHASE 7 — FastAPI platform (models, async jobs, results, audit) — `IN PROGRESS`
- [x] Solver JSON boundary: `solver_cli` (stdin model JSON → stdout result JSON),
      portable JSON parser/serializer (`solver/io/json.*`), model/result codec
      (`solver/io/model_io.*`), `SOLVER_BUILD_CLI` CMake option
- [x] FastAPI app (`backend/app/`): `GET /api/health`, `GET /api/demo`,
      `POST /api/validate` (`solver_cli --validate` → issues + model statistics),
      `POST /api/solve` with error mapping (422 input, 503 no CLI, 504 timeout, 500 crash)
- [x] Docker: `docker/Dockerfile` (gcc build stage + slim Python runtime),
      `docker/Dockerfile.frontend` (node build → nginx), `docker/nginx.conf` reverse proxy,
      `docker-compose.yml`, `.dockerignore` — app + API on `http://localhost:8000`
- [ ] Async job manager, persistence, audit trail, model registry
- [ ] Input schema (pydantic) hardening + API tests in CI
- [ ] `POST /api/ai/generate-model` (frontend panel already targets this path)
### PHASE 8 — React dashboard / model builder / visualizations — `DONE`
- [x] Scaffold: Vite + React + TS + Tailwind v4 + Recharts + Lucide + React Router;
      strict `tsc --noEmit` in the build script and inside the Docker image build
- [x] Service layer `src/services/api/` (no `fetch` in components) + `ApiError`
      mapping with real backend error strings; `BackendContext` health polling
- [x] Vertical workflow: Landing → Optimizer (Guided / AI / Expert) → Refinery inputs →
      Model Review (live `/api/validate`) → Solving (real elapsed timer) → Results
      (status banner, charts, variables/constraints/statistics tabs) → Explain This Solution →
      Scenarios (what-if comparison chart + table)
- [x] Supporting pages: Models (browser-local library), Benchmarks (live CPU measurements,
      GPU honestly unavailable), Documentation (format/API/roadmap), Status (component health)
- [x] Honesty rules enforced: synthetic-data labels, no fabricated metrics/percentages,
      AI panel shows the real "capability pending" state, localStorage labeled local
- [x] Template models verified against the real solver: all 5 templates solve `OPTIMAL`
      (refinery obj 3067062.5, production 12500, logistics 53500, resource 1381.25,
      scheduling 120 proven by B&B)
### PHASE 9 — End-to-end integration — `IN PROGRESS`
- [x] nginx serves the SPA at `/` and proxies `/api/*`, `/docs`, `/redoc`, `/openapi.json`
      to `solver-backend:8000` (single origin, no CORS); Vite dev proxy mirrors it
- [x] Smoke-tested through the proxy: `/`, SPA fallback `/optimizer`, `/api/health`,
      `/api/validate` (valid + 422), `/api/solve` (OPTIMAL obj=21 verified), `/docs`
- [ ] Async job polling flow (long solves), multi-user audit store
### PHASE 10 — Refinery demonstration + benchmarks + docs — `IN PROGRESS`
- [x] Refinery blend template (6 vars, 8 constraints, quality + demand + capacity) with
      synthetic MRPL-style scenario, guided input form, narrative model review
- [x] Benchmark page: 6-model suite measured live in-session (4 templates + dense 60×30 LP +
      30-binary knapsack), wall-clock + solver-internal times, persisted in browser storage
- [x] Docs: README run instructions, `docs/FRONTEND_ARCHITECTURE.md`, in-app Documentation page
- [ ] Presentation deck / recorded walkthrough of the refinery scenario

---

## 3. Non-negotiables (Section 34 of the problem statement)

- No hard-coded answers; every reported `OPTIMAL` is independently re-verified.
- No wrapped commercial solver; reference solvers (e.g. SciPy/HiGHS) are used **only** as
  offline test oracles, never shipped as the production engine.
- No fabricated GPU numbers; speedups require an executed GPU measurement.
- Synthetic demo data is labelled synthetic; no MRPL confidential data.
- Failures are surfaced with diagnostics, never hidden.

---

## 4. Reporting

At every stage, run the test suite and record exactly what passes in this file and in
`README.md`. Limitations are documented in `docs/limitations.md` instead of being faked.
