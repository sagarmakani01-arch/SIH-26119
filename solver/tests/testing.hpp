#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct Case {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<Case>& cases() {
  static std::vector<Case> c;
  return c;
}

inline int& checks() {
  static int c = 0;
  return c;
}

inline int& failedChecks() {
  static int f = 0;
  return f;
}

inline int& failedTests() {
  static int f = 0;
  return f;
}

inline const char*& currentTest() {
  static const char* n = "";
  return n;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) { cases().push_back({name, std::move(fn)}); }
};

inline void reportFailure(const char* file, int line, const std::string& message) {
  ++failedChecks();
  std::printf("    FAIL %s:%d  %s\n", file, line, message.c_str());
}

template <typename A, typename B>
std::string formatValues(const A& a, const B& b) {
  std::ostringstream os;
  os << "  actual=" << a << "  expected=" << b;
  return os.str();
}

}  // namespace testing

#define TEST(suite, name)                                                              \
  static void suite##_##name##_impl();                                                 \
  static ::testing::Registrar suite##_##name##_reg(#suite "." #name,                   \
                                                   suite##_##name##_impl);             \
  static void suite##_##name##_impl()

#define CHECK(cond)                                                                    \
  do {                                                                                 \
    ++::testing::checks();                                                             \
    if (!(cond)) ::testing::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ")");     \
  } while (0)

#define CHECK_FALSE(cond)                                                              \
  do {                                                                                 \
    ++::testing::checks();                                                             \
    if ((cond)) ::testing::reportFailure(__FILE__, __LINE__, "CHECK_FALSE(" #cond ")"); \
  } while (0)

#define CHECK_EQ(a, b)                                                                 \
  do {                                                                                 \
    ++::testing::checks();                                                             \
    if (!((a) == (b)))                                                                 \
      ::testing::reportFailure(__FILE__, __LINE__,                                     \
                               std::string("CHECK_EQ(" #a ", " #b ")") +               \
                                   ::testing::formatValues((a), (b)));                 \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                          \
  do {                                                                                 \
    ++::testing::checks();                                                             \
    double _va = static_cast<double>(a);                                               \
    double _vb = static_cast<double>(b);                                               \
    if (!(std::fabs(_va - _vb) <= (tol)))                                              \
      ::testing::reportFailure(__FILE__, __LINE__,                                     \
                               std::string("CHECK_NEAR(" #a ", " #b ")") +             \
                                   ::testing::formatValues(_va, _vb) + " tol=" +       \
                                   std::to_string(tol));                               \
  } while (0)

#define REQUIRE(cond)                                                                  \
  do {                                                                                 \
    ++::testing::checks();                                                             \
    if (!(cond)) {                                                                     \
      ::testing::reportFailure(__FILE__, __LINE__, "REQUIRE(" #cond ")");              \
      return;                                                                          \
    }                                                                                  \
  } while (0)
