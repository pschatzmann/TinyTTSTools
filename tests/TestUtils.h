/**
 * @file TestUtils.h
 * @brief Minimal assertion/summary helpers for TinyTTSTools' plain-C++
 * regression tests (no external test framework -- consistent with the
 * project's own lightweight style).
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstdio>

struct TestStats {
  int passed = 0;
  int failed = 0;
};

inline TestStats& testStats() {
  static TestStats stats;
  return stats;
}

/// Records a pass/fail without aborting -- prefer this over assert() so one
/// failing check doesn't hide other failures later in the same test.
#define CHECK(cond)                                                     \
  do {                                                                  \
    if (cond) {                                                         \
      testStats().passed++;                                             \
    } else {                                                            \
      testStats().failed++;                                             \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                   \
  } while (0)

#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    auto _check_a = (a);                                                                   \
    auto _check_b = (b);                                                                   \
    if (_check_a == _check_b) {                                                            \
      testStats().passed++;                                                                \
    } else {                                                                               \
      testStats().failed++;                                                                \
      printf("FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b);                        \
    }                                                                                       \
  } while (0)

/// Prints a pass/fail summary and returns the process exit code to use --
/// call this as the last thing in main().
inline int testSummary() {
  printf("\n%d passed, %d failed\n", testStats().passed, testStats().failed);
  return testStats().failed == 0 ? 0 : 1;
}
