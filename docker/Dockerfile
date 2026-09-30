# Multi-stage build:
#   stage 1 compiles the C++20 solver core + solver_cli with gcc on Linux
#   stage 2 ships a slim Python image running the FastAPI backend
FROM ubuntu:24.04 AS solver-build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       g++ cmake ninja-build ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY solver ./solver

RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DSOLVER_BUILD_TESTS=OFF \
      -DSOLVER_BUILD_EXAMPLES=OFF \
      -DSOLVER_BUILD_CLI=ON \
 && cmake --build build --target solver_cli

FROM python:3.12-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends tini \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /usr/sbin/nologin appuser

COPY --from=solver-build /src/build/solver/solver_cli /usr/local/bin/solver_cli

WORKDIR /app
COPY backend/requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir --retries 8 --timeout 90 -r /app/requirements.txt

COPY backend/app /app/app

ENV SOLVER_CLI_PATH=/usr/local/bin/solver_cli \
    SOLVE_TIMEOUT_SEC=120

EXPOSE 8000
USER appuser

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
