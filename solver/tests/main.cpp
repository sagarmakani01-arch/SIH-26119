#include <cstdio>
#include <exception>

#include "testing.hpp"

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("Running %zu tests\n", testing::cases().size());
  for (const auto& c : testing::cases()) {
    testing::currentTest() = c.name.c_str();
    int before = testing::failedChecks();
    std::printf("[ RUN  ] %s\n", c.name.c_str());
    try {
      c.fn();
    } catch (const std::exception& ex) {
      ++testing::failedChecks();
      std::printf("    FAIL uncaught exception: %s\n", ex.what());
    } catch (...) {
      ++testing::failedChecks();
      std::printf("    FAIL uncaught non-standard exception\n");
    }
    if (testing::failedChecks() > before) {
      ++testing::failedTests();
      std::printf("[ FAIL ] %s\n", c.name.c_str());
    } else {
      std::printf("[  OK  ] %s\n", c.name.c_str());
    }
  }
  std::printf("\n%zu tests, %d checks, %d failed checks, %d failed tests\n",
              testing::cases().size(), testing::checks(), testing::failedChecks(),
              testing::failedTests());
  return testing::failedTests() == 0 ? 0 : 1;
}
