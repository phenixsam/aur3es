/*
 * ============================================================================
 * GOLDEN RNG - SIMD OPTIMIZED IMPLEMENTATIONS
 * ============================================================================
 * Optimized random number generation using SIMD instructions
 * Supports SSE2, AVX2, and AVX-512
 * ============================================================================
 */

#ifndef RNG_SIMD_OPTIMIZED_H
#define RNG_SIMD_OPTIMIZED_H

#include <stdint.h>
#include <stddef.h>

/* Forward declaration */
struct rng_engine;
typedef struct rng_engine golden_rng_t;

/**
 * @brief Generate bytes using SSE2 (128-bit)
 * @param rng RNG handle
 * @param buffer Output buffer
 * @param size Number of bytes (must be multiple of 16)
 */
void rng_generate_bytes_sse2(golden_rng_t *rng, unsigned char *buffer, size_t size);

/**
 * @brief Generate bytes using AVX2 (256-bit)
 * @param rng RNG handle
 * @param buffer Output buffer
 * @param size Number of bytes (must be multiple of 32)
 */
void rng_generate_bytes_avx2(golden_rng_t *rng, unsigned char *buffer, size_t size);

/**
 * @brief Generate bytes using AVX-512 (512-bit)
 * @param rng RNG handle
 * @param buffer Output buffer  
 * @param size Number of bytes (must be multiple of 64)
 */
void rng_generate_bytes_avx512(golden_rng_t *rng, unsigned char *buffer, size_t size);

/**
 * @brief Auto-select best SIMD implementation
 * @param rng RNG handle
 * @param buffer Output buffer
 * @param size Number of bytes
 */
void rng_generate_bytes_simd_auto(golden_rng_t *rng, unsigned char *buffer, size_t size);

/**
 * @brief Get the name of the best available SIMD implementation
 * @return String name
 */
const char* rng_simd_get_best_implementation(void);

/**
 * @brief Check if SIMD is available
 * @return 1 if SIMD available, 0 otherwise
 */
int rng_simd_is_available(void);

#endif /* RNG_SIMD_OPTIMIZED_H */

