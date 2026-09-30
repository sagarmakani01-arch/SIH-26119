# Indigenous GPU-Accelerated Optimization Solver

SIH 2026 · Problem Statement 26119 · Sponsor: MRPL

A from-scratch optimization solver prototype: a C++20 solver core (LP today, MILP/QP next),
a FastAPI platform layer, and a React dashboard, with a GPU backend behind a real compile
flag and measured-only benchmarking. No wrapped commercial solvers; SciPy/HiGHS may be used
only as offline test oracles.

## Current status

| Phase | Scope | Status |
|---|---|---|
| 1 | Architecture scaffold, CMake, scripts | DONE |
| 2 | Model layer (sparse matrix, model, validation) | DONE |
| 3 | LP solver (presolve, standardize, simplex, verification) | DONE |
| 4 | MILP (branch & bound) | DONE — 96 tests passing |
| 5 | Convex QP | TODO |
| 6 | GPU backend + measured benchmarks | LIMITED — interface + CPU path; CUDA behind `SOLVER_ENABLE_CUDA` |
| 7 | FastAPI platform (models, async jobs, results, audit) | IN PROGRESS — health/demo/validate/solve API + Docker; jobs/audit/DB pending |
| 8 | React dashboard (React, TS, Vite, Tailwind) | DONE — full vertical: Landing → Optimizer → Review → Solve → Results → Explain → Scenarios + Models/Benchmarks/Docs/Status |
| 9 | End-to-end integration | IN PROGRESS — app served at `http://localhost:8000/`, API + Swagger proxied |
| 10 | Refinery demo + benchmarks + docs | IN PROGRESS — refinery template + measured live benchmarks; docs updated |

See `docs/PROJECT_PLAN.md` for the living plan and `docs/limitations.md` for honest limits.

## Layout

```
solver/     C++20 core: model, presolve, standardization, simplex, MILP B&B, verification, Solver API, solver_cli
gpu/        GPU backend sources (CUDA guarded by SOLVER_ENABLE_CUDA, CPU fallback)
backend/    FastAPI platform layer (health, demo, validate, solve)
frontend/   React + TypeScript + Vite + Tailwind app (service layer in src/services/api/)
docker/     Dockerfile (solver+API), Dockerfile.frontend (nginx + reverse proxy), nginx.conf
datasets/   synthetic demo data (labeled synthetic)
docs/       PROJECT_PLAN.md, architecture.md, limitations.md, FRONTEND_ARCHITECTURE.md
scripts/    build.ps1, test.ps1 (MSVC vcvars64 + pip cmake/ninja)
```

## Build and test (Windows, MSVC)

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
powershell -ExecutionPolicy Bypass -File scripts\test.ps1
```

`test.ps1` configures CMake if needed, builds, and runs CTest (`solver_unit_tests`).

## Run the full app (Docker, localhost)

```bash
docker compose up -d --build     # solver_cli (gcc) + FastAPI + nginx frontend
```

**http://localhost:8000** serves the web app; the same origin proxies the API:

| Endpoint | Purpose |
|---|---|
| `/` | React app (Landing → Optimizer → Results → Scenarios …) |
| `GET /api/health` | API + solver_cli availability/version |
| `GET /api/demo` | synthetic demo model (product mix) |
| `POST /api/validate` | validate a model — issues + structural statistics |
| `POST /api/solve` | solve a JSON model, returns status/objective/x/verification |
| `GET /docs` | Swagger UI (proxied from FastAPI) |

```bash
curl http://localhost:8000/api/health
curl -X POST http://localhost:8000/api/solve -H "Content-Type: application/json" -d @model.json
docker compose down                # stop
docker compose logs -f             # logs
```

Architecture: React SPA → nginx reverse proxy → FastAPI (`/api/*`, `/docs`) → `solver_cli`
subprocess (JSON on stdin/stdout) → C++ `Solver`.
Without Docker (dev): build the project, run
`uvicorn app.main:app --port 8000` from `backend/`, then `npm run dev` in `frontend/`
(Vite proxies `/api` and `/docs` to `localhost:8000`).

## Quick example

```cpp
#include "solver/solver.hpp"

solver::OptimizationModel m;
int x = m.addVariable("x", solver::VarType::Continuous, 0.0, 4.0);
int y = m.addVariable("y", solver::VarType::Continuous, 0.0, 6.0);
m.setObjectiveSense(solver::ObjectiveSense::Maximize);
m.setObjective(solver::LinearExpression().add(x, 3.0).add(y, 5.0));
m.addConstraint("c3", solver::ConstraintSense::LessEqual,
                solver::LinearExpression().add(x, 3.0).add(y, 2.0), 18.0);

solver::Solver s;
solver::SolverResult r = s.solve(m);
// r.status == Optimal, r.verified == true, r.objective == 36
```

Every reported `OPTIMAL` is independently re-verified in original model space (primal
feasibility + dual KKT certificate) before the status is returned.

## GPU honesty

This machine has no CUDA toolchain/GPU. The GPU backend compiles only when
`SOLVER_ENABLE_CUDA=ON`. A `GPU` request on a CPU-only build resolves to `CPU` with an
explicit warning; no speedup number is ever reported without an executed GPU measurement.
