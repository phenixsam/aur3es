/*
 * ============================================================================
 * GOLDEN RNG - TestU01 BigCrush Integration Header
 * ============================================================================
 * Provides integration with TestU01 statistical test suite
 * Supports SmallCrush, Crush, and BigCrush tests
 * ============================================================================
 */

#ifndef TESTU01_BIGCRUSH_H
#define TESTU01_BIGCRUSH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declaration */
struct rng_engine;
typedef struct rng_engine golden_rng_t;

/* ============================================================================
 * TEST SUITE TYPES
 * ============================================================================ */

/**
 * TestU01 test suite types
 */
typedef enum {
    TESTU01_SMALLCRUSH = 0,
    TESTU01_CRUSH = 1,
    TESTU01_BIGCRUSH = 2
} testu01_suite_t;

/**
 * Result structure for TestU01 tests
 */
typedef struct {
    testu01_suite_t suite;
    int total_tests;
    int passed;
    int failed;
    int warnings;
    double p_value_sum;
    bool completed;
} testu01_result_t;

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * @brief Create a TestU01 compatible generator from Golden RNG
 * @param rng Golden RNG instance
 * @param name Name for the generator
 * @return TestU01 generator structure (unif01_Gen*)
 * 
 * Note: The returned generator uses the provided RNG and should NOT
 * be deleted separately. The underlying RNG must be kept alive.
 */
void* testu01_create_gen(golden_rng_t *rng, const char *name);

/**
 * @brief Delete TestU01 generator wrapper
 * @param gen Generator to delete
 */
void testu01_delete_gen(void *gen);

/**
 * @brief Run SmallCrush test suite
 * @param rng Golden RNG instance
 * @return Test result
 */
testu01_result_t testu01_run_smallcrush(golden_rng_t *rng);

/**
 * @brief Run Crush test suite
 * @param rng Golden RNG instance
 * @return Test result
 */
testu01_result_t testu01_run_crush(golden_rng_t *rng);

/**
 * @brief Run BigCrush test suite
 * @param rng Golden RNG instance
 * @return Test result
 */
testu01_result_t testu01_run_bigcrush(golden_rng_t *rng);

/**
 * @brief Run specified test suite
 * @param rng Golden RNG instance
 * @param suite Test suite to run
 * @return Test result
 */
testu01_result_t testu01_run_suite(golden_rng_t *rng, testu01_suite_t suite);

/**
 * @brief Print test result in human-readable format
 * @param result Test result to print
 */
void testu01_print_result(const testu01_result_t *result);

/**
 * @brief Check if TestU01 is available
 * @return true if TestU01 libraries are linked
 */
bool testu01_is_available(void);

/* ============================================================================
 * STDIN-BASED GENERATOR (For piping binary data)
 * ============================================================================ */

/**
 * @brief Create a TestU01 generator that reads from stdin
 * @param buffer_size Size of internal buffer (default: 1MB)
 * @return TestU01 generator or NULL on error
 * 
 * Usage: ./goldenrng | ./bin/test_bigcrush
 */
void* testu01_create_stdin_gen(size_t buffer_size);

/**
 * @brief Delete stdin-based generator
 * @param gen Generator to delete
 */
void testu01_delete_stdin_gen(void *gen);

/* ============================================================================
 * BENCHMARK MODE
 * ============================================================================ */

/**
 * @brief Run TestU01 in benchmark mode (less verbose)
 * @param rng Golden RNG instance
 * @param suite Test suite
 * @return Test result
 */
testu01_result_t testu01_run_benchmark(golden_rng_t *rng, testu01_suite_t suite);

#endif /* TESTU01_BIGCRUSH_H */
