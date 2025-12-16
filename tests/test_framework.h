#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Colors for output */
#define TERM_RED     "\033[31m"
#define TERM_GREEN   "\033[32m"
#define TERM_RESET   "\033[0m"

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define RUN_TEST(func) \
    do { \
        printf("[ RUN      ] %s\n", #func); \
        g_tests_run++; \
        if (func()) { \
            printf(TERM_GREEN "[       OK ]" TERM_RESET " %s\n", #func); \
        } else { \
            printf(TERM_RED   "[  FAILED  ]" TERM_RESET " %s\n", #func); \
            g_tests_failed++; \
        } \
    } while (0)

/* Assertions that return 0 on failure */

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, TERM_RED "  Assertion failed: %s:%d\n    Condition: %s\n" TERM_RESET, __FILE__, __LINE__, #cond); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_INT_EQ(expected, actual) \
    do { \
        int _e = (int)(expected); \
        int _a = (int)(actual); \
        if (_e != _a) { \
            fprintf(stderr, TERM_RED "  Assertion failed: %s:%d\n    Expected: %d\n    Actual:   %d\n" TERM_RESET, __FILE__, __LINE__, _e, _a); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_EQ(expected, actual, epsilon) \
    do { \
        float _e = (float)(expected); \
        float _a = (float)(actual); \
        if (fabsf(_e - _a) > (float)(epsilon)) { \
            fprintf(stderr, TERM_RED "  Assertion failed: %s:%d\n    Expected: %f\n    Actual:   %f\n    Diff:     %f\n" TERM_RESET, __FILE__, __LINE__, _e, _a, fabsf(_e - _a)); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQ(expected, actual) \
    do { \
        const char* _e = (expected); \
        const char* _a = (actual); \
        if (strcmp(_e, _a) != 0) { \
            fprintf(stderr, TERM_RED "  Assertion failed: %s:%d\n    Expected: \"%s\"\n    Actual:   \"%s\"\n" TERM_RESET, __FILE__, __LINE__, _e, _a); \
            return 0; \
        } \
    } while(0)

#endif // TEST_FRAMEWORK_H
