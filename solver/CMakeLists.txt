file(GLOB_RECURSE SOLVER_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(solver_core STATIC ${SOLVER_CORE_SOURCES})

target_include_directories(solver_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(solver_core PUBLIC cxx_std_20)

if(SOLVER_ENABLE_CUDA)
  target_sources(solver_core PRIVATE src/gpu/cuda_backend.cu)
  target_compile_definitions(solver_core PUBLIC SOLVER_ENABLE_CUDA=1)
  find_package(CUDAToolkit REQUIRED)
  target_link_libraries(solver_core PUBLIC CUDA::cudart)
endif()

if(SOLVER_BUILD_TESTS)
  add_executable(solver_tests
    tests/main.cpp
    tests/test_sparse_matrix.cpp
    tests/test_model.cpp
    tests/test_validation.cpp
    tests/test_dense_lu.cpp
    tests/test_standardize.cpp
    tests/test_simplex.cpp
    tests/test_presolve.cpp
    tests/test_verification.cpp
    tests/test_lp_solver.cpp
    tests/test_mip.cpp
  )
  target_link_libraries(solver_tests PRIVATE solver_core)
  target_include_directories(solver_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME solver_unit_tests COMMAND solver_tests)
endif()

if(SOLVER_BUILD_EXAMPLES)
  add_executable(example_model examples/build_model.cpp)
  target_link_libraries(example_model PRIVATE solver_core)
endif()

if(SOLVER_BUILD_CLI)
  add_executable(solver_cli cli/main.cpp)
  target_link_libraries(solver_cli PRIVATE solver_core)
endif()
