cmake_minimum_required(VERSION 3.20)

project(SovereignSolver VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOLVER_ENABLE_CUDA "Compile the CUDA GPU backend" OFF)
option(SOLVER_BUILD_TESTS "Build unit tests" ON)
option(SOLVER_BUILD_EXAMPLES "Build examples" ON)
option(SOLVER_BUILD_CLI "Build the solver JSON CLI" ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
  add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(SOLVER_ENABLE_CUDA)
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()

if(SOLVER_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(solver)
