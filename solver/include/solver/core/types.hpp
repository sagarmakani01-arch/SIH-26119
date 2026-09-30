#pragma once

#include <limits>
#include <string>

namespace solver {

inline constexpr double kInfinity = std::numeric_limits<double>::infinity();
inline constexpr double kNaNDouble = std::numeric_limits<double>::quiet_NaN();

enum class VarType { Continuous, Integer, Binary };

enum class ConstraintSense { LessEqual, GreaterEqual, Equal };

enum class ObjectiveSense { Minimize, Maximize };

enum class SolveStatus {
  NotSolved,
  Optimal,
  Feasible,
  Infeasible,
  Unbounded,
  TimeLimit,
  IterationLimit,
  NumericalError,
  Error
};

enum class ComputeBackendKind { Auto, CPU, GPU };

inline const char* toString(VarType t) {
  switch (t) {
    case VarType::Continuous: return "CONTINUOUS";
    case VarType::Integer: return "INTEGER";
    case VarType::Binary: return "BINARY";
  }
  return "UNKNOWN";
}

inline const char* toString(ConstraintSense s) {
  switch (s) {
    case ConstraintSense::LessEqual: return "<=";
    case ConstraintSense::GreaterEqual: return ">=";
    case ConstraintSense::Equal: return "=";
  }
  return "UNKNOWN";
}

inline const char* toString(ObjectiveSense s) {
  switch (s) {
    case ObjectiveSense::Minimize: return "MINIMIZE";
    case ObjectiveSense::Maximize: return "MAXIMIZE";
  }
  return "UNKNOWN";
}

inline const char* toString(SolveStatus s) {
  switch (s) {
    case SolveStatus::NotSolved: return "NOT_SOLVED";
    case SolveStatus::Optimal: return "OPTIMAL";
    case SolveStatus::Feasible: return "FEASIBLE";
    case SolveStatus::Infeasible: return "INFEASIBLE";
    case SolveStatus::Unbounded: return "UNBOUNDED";
    case SolveStatus::TimeLimit: return "TIME_LIMIT";
    case SolveStatus::IterationLimit: return "ITERATION_LIMIT";
    case SolveStatus::NumericalError: return "NUMERICAL_ERROR";
    case SolveStatus::Error: return "ERROR";
  }
  return "UNKNOWN";
}

inline const char* toString(ComputeBackendKind k) {
  switch (k) {
    case ComputeBackendKind::Auto: return "AUTO";
    case ComputeBackendKind::CPU: return "CPU";
    case ComputeBackendKind::GPU: return "GPU";
  }
  return "UNKNOWN";
}

inline VarType varTypeFromString(const std::string& s) {
  if (s == "INTEGER" || s == "INT") return VarType::Integer;
  if (s == "BINARY" || s == "BIN") return VarType::Binary;
  return VarType::Continuous;
}

}  // namespace solver
