/*
 * ============================================================================
 * GOLDEN RNG - SIMD OPTIMIZED IMPLEMENTATIONS
 * ============================================================================
 */

#include "rng_simd_optimized.h"
#include "rng_engine.h"
#include <string.h>

/* Check for SIMD support */
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
    #define HAS_SSE2 1
    #include <emmintrin.h>
#endif

#if defined(__AVX2__)
    #define HAS_AVX2 1
    #include <immintrin.h>
#endif

/* ============================================================================
 * SSE2 IMPLEMENTATION (128-bit = 16 bytes per iteration)
 * ============================================================================ */

#if HAS_SSE2

void rng_generate_bytes_sse2(golden_rng_t *rng, unsigned char *buffer, size_t size) {
    if (!rng || !buffer || size == 0) return;
    
    size_t iterations = size / 16;
    size_t remainder = size % 16;
    
    for (size_t i = 0; i < iterations; i++) {
        uint64_t block[4];
        rng_generate_block(rng, block);
        
        /* Load first 16 bytes (block[0] and block[1]) into vector */
        __m128i vec = _mm_loadu_si128((const __m128i*)block);
        _mm_storeu_si128((__m128i*)(buffer + i * 16), vec);
    }
    
    if (remainder > 0) {
        uint64_t block[4];
        rng_generate_block(rng, block);
        memcpy(buffer + iterations * 16, block, remainder);
    }
}

#else

void rng_generate_bytes_sse2(golden_rng_t *rng, unsigned char *buffer, size_t size) {
    (void)rng; (void)buffer; (void)size;
}

#endif

/* ============================================================================
 * AVX2 IMPLEMENTATION (256-bit = 32 bytes per iteration)
 * ============================================================================ */

#if HAS_AVX2

void rng_generate_bytes_avx2(golden_rng_t *rng, unsigned char *buffer, size_t size) {
    if (!rng || !buffer || size == 0) return;
    
    size_t iterations = size / 32;
    size_t remainder = size % 32;
    
    for (size_t i = 0; i < iterations; i++) {
        uint64_t block[4];
        rng_generate_block(rng, block);
        
        /* Pack into 256-bit vector */
        __m256i vec = _mm256_set_epi64x(block[3], block[2], block[1], block[0]);
        
        /* XOR with another block for more randomness */
        rng_generate_block(rng, block);
        __m256i vec2 = _mm256_set_epi64x(block[3], block[2], block[1], block[0]);
        
        vec = _mm256_xor_si256(vec, vec2);
        
        _mm256_storeu_si256((__m256i*)(buffer + i * 32), vec);
    }
    
    if (remainder > 0) {
        uint64_t block[4];
        rng_generate_block(rng, block);
        memcpy(buffer + iterations * 32, block, remainder);
    }
}

#else

void rng_generate_bytes_avx2(golden_rng_t *rng, unsigned char *buffer, size_t size) {
    rng_generate_bytes(rng, buffer, size);
}

#endif

/* ============================================================================
 * AVX-512 (fallback to AVX2)
 * ============================================================================ */

void rng_generate_bytes_avx512(golden_rng_t *rng, unsigned char *buffer, size_t size) {
    /* AVX-512 not available, use AVX2 fallback */
#if HAS_AVX2
    rng_generate_bytes_avx2(rng, buffer, size);
#else
    rng_generate_bytes(rng, buffer, size);
#endif
}

/* ============================================================================
 * AUTO SELECTION
 * ============================================================================ */

void rng_generate_bytes_simd_auto(golden_rng_t *rng, unsigned char *buffer, size_t size) {
#if HAS_AVX2
    rng_generate_bytes_avx2(rng, buffer, size);
#elif HAS_SSE2
    rng_generate_bytes_sse2(rng, buffer, size);
#else
    rng_generate_bytes(rng, buffer, size);
#endif
}

const char* rng_simd_get_best_implementation(void) {
#if HAS_AVX2
    return "AVX2";
#elif HAS_SSE2
    return "SSE2";
#else
    return "Scalar";
#endif
}

int rng_simd_is_available(void) {
#if defined(__AVX2__) || defined(__SSE2__)
    return 1;
#else
    return 0;
#endif
}

