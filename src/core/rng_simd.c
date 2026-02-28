/*
 * ============================================================================
 * GOLDEN RNG ENGINE - SIMD UTILITIES (LEGACY)
 * ============================================================================
 * This file is now integrated into rng_engine.c
 * Kept for compatibility with existing code
 * ============================================================================
 */

#include "rng_engine.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * BENCHMARK LEGACY (Mantenido para compatibilidad)
 * ============================================================================ */

// The main functions are now in rng_engine.c:
// - get_platform_id()
// - get_platform_name()
// - detect_cpu_features()
// - get_cpu_features_string()
// - is_little_endian()
// - get_optimal_alignment()
// - gad1_print_system_info()
// - rng_benchmark_simd()
// - rng_benchmark_simd_print_report()
// - gad1_benchmark()

/* ============================================================================
 * UTILIDADES ADICIONALES
 * ============================================================================ */

// Get library version
const char* rng_simd_get_version(void) {
    return "GOLDEN RNG SIMD v5.0 (integrated)";
}

// Check if SIMD is available
int rng_simd_is_available(void) {
    cpu_features_t features;
    detect_cpu_features(&features);
    return features.has_sse2 || features.has_avx2 || features.has_neon;
}

// Imprimir resumen de capacidades SIMD
void rng_simd_print_capabilities(void) {
    cpu_features_t features;
    detect_cpu_features(&features);

    printf("\n=== Capacidades SIMD ===\n");
    printf("SSE2:    %s\n", features.has_sse2 ? "SI" : "NO");
    printf("SSE3:    %s\n", features.has_sse3 ? "SI" : "NO");
    printf("SSSE3:   %s\n", features.has_ssse3 ? "SI" : "NO");
    printf("SSE4.1:  %s\n", features.has_sse4_1 ? "SI" : "NO");
    printf("SSE4.2:  %s\n", features.has_sse4_2 ? "SI" : "NO");
    printf("AVX:     %s\n", features.has_avx ? "SI" : "NO");
    printf("AVX2:    %s\n", features.has_avx2 ? "SI" : "NO");
    printf("AVX-512: %s\n", features.has_avx512f ? "SI" : "NO");
    printf("NEON:    %s\n", features.has_neon ? "SI" : "NO");
    printf("==========================\n");
}

