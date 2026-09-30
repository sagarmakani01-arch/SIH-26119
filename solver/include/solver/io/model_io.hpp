#pragma once

#include <string>

#include "solver/io/json.hpp"
#include "solver/model/model.hpp"
#include "solver/solver.hpp"

namespace solver {

struct ModelDecodeResult {
  bool ok = false;
  OptimizationModel model;
  SolverConfig config;
  std::string error;
};

ModelDecodeResult modelFromJson(const json::Value& root);
json::Value resultToJson(const SolverResult& result);
json::Value modelToJson(const OptimizationModel& model);

}  // namespace solver
