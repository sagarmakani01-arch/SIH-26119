# Frontend Architecture

SIH 2026 PS 26119 · Sovereign Optimization Engine · Web application

## 1. Purpose

The Swagger page at `/docs` is API documentation, **not** the product. This document
defines the user-facing web application built on top of the existing backend. The backend,
solver core, and CLI are **not modified** by the frontend except for one additive endpoint
(`/api/validate`, see §4).

## 2. Inventory of what exists (inspected before building)

| Layer | Status |
|---|---|
| Solver core (C++20) | LP + MILP, presolve, verification, 96 unit tests |
| `solver_cli` | JSON on stdin → JSON on stdout; `--version`, `--help`; exit 1 = invalid input |
| FastAPI (`backend/app/`) | `GET /api/health`, `GET /api/demo`, `POST /api/solve` |
| Request schema | `{name?, objective:{sense,coefficients,constant?}, variables:[{name,type,lb,ub}], constraints:[{name,sense,coefficients,rhs}], config?}` |
| Response schema | `{status, problem_class, objective, feasible_point, verified, primal_violation, dual_violation, duality_gap, x[], duals[], reduced_costs[], constraint_activities[], termination_reason, warnings[], statistics{…}}` |
| Solve statuses | `OPTIMAL FEASIBLE INFEASIBLE UNBOUNDED TIME_LIMIT ITERATION_LIMIT NUMERICAL_ERROR ERROR NOT_SOLVED` |
| GPU | Not available on this hardware — UI must show "Unavailable", never fake speedups |
| Frontend | Empty scaffold (`frontend/src` only) |

## 3. Technology

- React 19 + TypeScript + Vite
- Tailwind CSS v4 (dark industrial theme, `@theme` tokens in `styles/index.css`)
- Recharts (charts), Lucide (icons), React Router (routing)
- No state library; one React context (`WorkspaceContext`) + `sessionStorage` persistence,
  clearly labelled local

## 4. Backend additions (additive, documented)

| Endpoint | Why | Implementation |
|---|---|---|
| `POST /api/validate` | Model-review step must validate **without solving** | new `solver_cli --validate` flag → `{valid, issues[], statistics}` |
| Swagger stays at `/docs` | Product rule | nginx (prod) and Vite proxy (dev) forward `/docs`, `/openapi.json`, `/redoc` to the backend |

Required backend work that does **not** exist yet (frontend shows these as unavailable,
never as working): async job manager (real live progress), AI/NL model generation
(`/api/ai/generate-model`), benchmark registry, persistence/database, GPU backend.

## 5. Routing

| Route | Page | Notes |
|---|---|---|
| `/` | Landing | hero, pipeline visualization, capabilities |
| `/optimizer` | Optimizer | Guided / AI / Expert mode chooser |
| `/optimizer/guided/:templateId` | Template input form | refinery, production, logistics, resource, scheduling |
| `/optimizer/review` | Model review | human formulation + expert JSON, validate, optimize |
| `/optimizer/solving` | Solving screen | real elapsed time only; no fabricated live metrics |
| `/optimizer/results` | Results | objective, decision table, utilization charts, stats |
| `/scenarios` | What-if analysis | clone model, edit inputs, real re-solves, comparison table |
| `/benchmarks` | Benchmarks | live measured CPU runs only; "GPU benchmark unavailable" |
| `/models` | Model workspace | local (browser) model library, labelled as such |
| `/documentation` | Documentation | schema, workflow, API reference (`/docs` link) |
| `/status` | System status | real `GET /api/health` + honest capability flags |

Workflow contract: **CHOOSE PROBLEM → ENTER DATA → REVIEW → OPTIMIZE → UNDERSTAND RESULT →
RUN WHAT-IF**. A normal user never sees simplex/branch-and-bound/duals; those live in
Expert Mode and Technical Details.

## 6. Service layer

```
src/services/api/
  client.ts    fetch wrapper: JSON, timeout, ApiError (422/503/504/500 mapping)
  types.ts     SolverModel, SolverResult, SolveStatistics, ValidationReport, Health…
  solver.ts    solveModel(), validateModel()  → POST /api/solve, /api/validate
  health.ts    getHealth()                    → GET  /api/health
  demo.ts      getDemoModel()                 → GET  /api/demo
```

No `fetch()` calls outside this layer.

## 7. Templates (guided mode)

`src/templates/` — each template is data + a pure `buildModel(inputs) → SolverModel`.
All templates produce **real** models solved by the real engine:

| Template | Class | Shape |
|---|---|---|
| refinery | LP | crudes → products yields, demands, capacity, profit |
| production | LP | product mix over shared resources |
| logistics | LP | transportation (supply → demand, costs) |
| resource | LP | activities over budgets/capacity |
| scheduling | MILP | binary shift/task assignment |

Refinery is the polished showcase: objective selector, raw materials (availability, cost,
quality), product demands, operating limits, Reset/Load-Example.

## 8. Honesty rules enforced in code

1. Every displayed solve metric comes from the backend response (`statistics`, `objective`,
   `status`, `verified`, `duality_gap`).
2. The solving screen shows one real value: client **elapsed time** (measured locally);
   everything else shows "Solver running…" until the response arrives.
3. Benchmarks are executed live and labelled "measured in this session (CPU)".
4. GPU shows "Unavailable" when `health` reports no GPU path; no invented speedups.
5. Synthetic demo data is labelled **Demo / Synthetic Data — not MRPL data**.
6. AI mode calls the expected endpoint and displays the real failure/absence state
   ("capability pending") instead of pretending to generate a model.
7. `verified:false` results are shown with a warning banner, never silently as optimal.

## 9. Build & run

```bash
# dev (proxies /api and /docs to localhost:8000)
cd frontend && npm install && npm run dev        # → http://localhost:5173

# production (single origin)
docker compose up -d --build                      # → http://localhost:8000  (app at /, Swagger at /docs)
```

`docker/Dockerfile.frontend`: node build stage → nginx serving static files with
reverse proxy: `/api/*`, `/docs`, `/openapi.json`, `/redoc` → `solver-backend:8000`.

## 10. Design system

- Dark primary: deep navy/black (`--bg`, `--panel`), cyan/violet accents, subtle glow
- Glass surfaces used sparingly; strong type hierarchy (Inter + JetBrains Mono for numbers)
- Responsive: desktop-first, usable on tablet/mobile; charts collapse to single column
- Micro-interactions only (hover/focus/transition); no decorative over-animation
