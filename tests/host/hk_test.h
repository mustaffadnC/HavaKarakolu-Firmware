#ifndef HK_TEST_H
#define HK_TEST_H

/* Tiny dependency-free unit-test harness. Each test executable is one .c file
 * with its own main() that ends with `return hk_test_summary();`. */

#include <stdio.h>

static int hk_tests_run    = 0;
static int hk_tests_failed = 0;

#define HK_CHECK(cond)                                                     \
    do {                                                                   \
        hk_tests_run++;                                                    \
        if (!(cond)) {                                                     \
            hk_tests_failed++;                                             \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

#define HK_CHECK_EQ_INT(a, b)                                             \
    do {                                                                  \
        hk_tests_run++;                                                   \
        long _a = (long)(a);                                             \
        long _b = (long)(b);                                            \
        if (_a != _b) {                                                  \
            hk_tests_failed++;                                           \
            printf("  FAIL %s:%d: %s == %s (got %ld, want %ld)\n",      \
                   __FILE__, __LINE__, #a, #b, _a, _b);                  \
        }                                                                 \
    } while (0)

static inline int hk_test_summary(void)
{
    printf("  %d checks, %d failed\n", hk_tests_run, hk_tests_failed);
    return (hk_tests_failed == 0) ? 0 : 1;
}

#endif /* HK_TEST_H */
