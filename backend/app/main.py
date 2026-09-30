from typing import Any, Dict

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from fastapi import Body

from . import solver_runner

API_VERSION = "0.1.0"

DEMO_MODEL: Dict[str, Any] = {
    "name": "demo_product_mix_synthetic",
    "objective": {"sense": "maximize", "coefficients": [3, 5]},
    "variables": [
        {"name": "x", "type": "continuous", "lb": 0, "ub": 4},
        {"name": "y", "type": "continuous", "lb": 0, "ub": 6},
    ],
    "constraints": [
        {"name": "machine_a", "sense": "<=", "coefficients": [1, 0], "rhs": 4},
        {"name": "machine_b", "sense": "<=", "coefficients": [0, 1], "rhs": 12},
        {"name": "material", "sense": "<=", "coefficients": [3, 2], "rhs": 18},
    ],
    "config": {"time_limit_sec": 60, "presolve": True, "verify": True},
}

app = FastAPI(
    title="Sovereign Solver API",
    version=API_VERSION,
    description=(
        "Backend for the indigenous GPU-accelerated optimization solver "
        "(SIH 2026 PS 26119). Models are submitted as JSON, solved by the "
        "C++ solver core through the solver_cli process boundary."
    ),
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/api/health")
def health() -> Dict[str, Any]:
    cli = solver_runner.find_cli()
    return {
        "status": "ok",
        "api_version": API_VERSION,
        "solver_cli": {"path": cli, "available": cli is not None},
        "solver_version": solver_runner.cli_version(),
    }


@app.get("/api/demo")
def demo() -> Dict[str, Any]:
    return DEMO_MODEL


@app.post("/api/validate")
def validate(model: Dict[str, Any] = Body(...)) -> Dict[str, Any]:
    if not isinstance(model, dict):
        raise HTTPException(status_code=422, detail="model must be a JSON object")
    try:
        return solver_runner.validate_model(model)
    except solver_runner.SolverInputError as exc:
        raise HTTPException(status_code=422, detail=str(exc))
    except solver_runner.SolverNotFoundError as exc:
        raise HTTPException(status_code=503, detail=str(exc))
    except solver_runner.SolverTimeoutError as exc:
        raise HTTPException(status_code=504, detail=str(exc))
    except solver_runner.SolverCrashedError as exc:
        raise HTTPException(status_code=500, detail=str(exc))


@app.post("/api/solve")
def solve(model: Dict[str, Any] = Body(...)) -> Dict[str, Any]:
    if not isinstance(model, dict):
        raise HTTPException(status_code=422, detail="model must be a JSON object")
    try:
        result = solver_runner.solve_model(model)
    except solver_runner.SolverInputError as exc:
        raise HTTPException(status_code=422, detail=str(exc))
    except solver_runner.SolverNotFoundError as exc:
        raise HTTPException(status_code=503, detail=str(exc))
    except solver_runner.SolverTimeoutError as exc:
        raise HTTPException(status_code=504, detail=str(exc))
    except solver_runner.SolverCrashedError as exc:
        raise HTTPException(status_code=500, detail=str(exc))
    return result


@app.exception_handler(Exception)
async def unhandled_exception_handler(request, exc):  # pragma: no cover
    return JSONResponse(status_code=500, content={"detail": str(exc)})
