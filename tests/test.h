#ifndef TEST_H
#define TEST_H
#include <stdio.h>
#include <math.h>
#include <string.h>

static int test_failures = 0;
static int test_checks = 0;

#define CHECK(cond) do { \
    test_checks++; \
    if (!(cond)) { \
        test_failures++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } } while (0)

#define CHECK_EQ_INT(a, b) do { \
    long test_a_ = (long)(a), test_b_ = (long)(b); \
    test_checks++; \
    if (test_a_ != test_b_) { \
        test_failures++; \
        printf("FAIL %s:%d: %s == %s (got %ld, expected %ld)\n", \
               __FILE__, __LINE__, #a, #b, test_a_, test_b_); \
    } } while (0)

#define CHECK_NEAR(a, b, eps) do { \
    double test_a_ = (double)(a), test_b_ = (double)(b); \
    test_checks++; \
    if (fabs(test_a_ - test_b_) > (double)(eps)) { \
        test_failures++; \
        printf("FAIL %s:%d: %s ~ %s (got %g, expected %g)\n", \
               __FILE__, __LINE__, #a, #b, test_a_, test_b_); \
    } } while (0)

#define CHECK_EQ_STR(a, b) do { \
    const char *test_s_ = (a), *test_t_ = (b); \
    test_checks++; \
    if (strcmp(test_s_, test_t_) != 0) { \
        test_failures++; \
        printf("FAIL %s:%d: %s\n  got      \"%s\"\n  expected \"%s\"\n", \
               __FILE__, __LINE__, #a, test_s_, test_t_); \
    } } while (0)

#define TEST_MAIN_END() \
    printf("%d check(s), %d failure(s)\n", test_checks, test_failures); \
    return test_failures ? 1 : 0

#endif
