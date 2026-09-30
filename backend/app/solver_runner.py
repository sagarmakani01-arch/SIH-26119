import json
import os
import shutil
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


class SolverInputError(Exception):
    pass


class SolverNotFoundError(Exception):
    pass


class SolverTimeoutError(Exception):
    pass


class SolverCrashedError(Exception):
    pass


def find_cli() -> str | None:
    env = os.environ.get("SOLVER_CLI_PATH")
    if env and Path(env).exists():
        return env
    which = shutil.which("solver_cli") or shutil.which("solver_cli.exe")
    if which:
        return which
    for name in ("solver_cli.exe", "solver_cli"):
        candidate = REPO_ROOT / "build" / "solver" / name
        if candidate.exists():
            return str(candidate)
    return None


def cli_version() -> str | None:
    cli = find_cli()
    if not cli:
        return None
    try:
        proc = subprocess.run(
            [cli, "--version"], capture_output=True, text=True, timeout=10
        )
    except (OSError, subprocess.SubprocessError):
        return None
    out = (proc.stdout or "").strip()
    return out or None


def _run_cli(args: list[str], payload: str, timeout_sec: float) -> subprocess.CompletedProcess:
    cli = find_cli()
    if not cli:
        raise SolverNotFoundError(
            "solver_cli not found; set SOLVER_CLI_PATH or build the project"
        )
    try:
        return subprocess.run(
            [cli, *args],
            input=payload,
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=timeout_sec,
        )
    except subprocess.TimeoutExpired as exc:
        raise SolverTimeoutError(f"solver exceeded {timeout_sec}s") from exc


def _default_timeout(timeout_sec: float | None) -> float:
    if timeout_sec is not None:
        return timeout_sec
    return float(os.environ.get("SOLVE_TIMEOUT_SEC", "120"))


def validate_model(model: dict, timeout_sec: float | None = None) -> dict:
    timeout = _default_timeout(timeout_sec)
    proc = _run_cli(["--validate"], json.dumps(model), timeout)
    stdout = (proc.stdout or "").strip()
    if not stdout:
        raise SolverCrashedError(
            f"solver_cli produced no output (rc={proc.returncode}, "
            f"stderr={ (proc.stderr or '')[:500] })"
        )
    try:
        result = json.loads(stdout)
    except json.JSONDecodeError as exc:
        raise SolverCrashedError(f"solver_cli output is not JSON: {stdout[:500]}") from exc
    if proc.returncode == 1:
        raise SolverInputError(result.get("error", "invalid input"))
    if proc.returncode not in (0, 1):
        raise SolverCrashedError(
            f"solver_cli failed (rc={proc.returncode}): "
            f"{result.get('error') or (proc.stderr or '')[:500]}"
        )
    return result


def solve_model(model: dict, timeout_sec: float | None = None) -> dict:
    timeout = _default_timeout(timeout_sec)
    proc = _run_cli([], json.dumps(model), timeout)

    stdout = (proc.stdout or "").strip()
    if not stdout:
        raise SolverCrashedError(
            f"solver_cli produced no output (rc={proc.returncode}, "
            f"stderr={ (proc.stderr or '')[:500] })"
        )
    try:
        result = json.loads(stdout)
    except json.JSONDecodeError as exc:
        raise SolverCrashedError(
            f"solver_cli output is not JSON: {stdout[:500]}"
        ) from exc

    if proc.returncode == 1 and result.get("status") == "ERROR":
        raise SolverInputError(result.get("error", "invalid input"))
    if proc.returncode not in (0, 1):
        raise SolverCrashedError(
            f"solver_cli failed (rc={proc.returncode}): "
            f"{result.get('error') or (proc.stderr or '')[:500]}"
        )
    return result
