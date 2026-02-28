/**
 * @file csprng_entropy.h
 * @brief CSPRNG Entropy Collector - Professional Implementation
 * @version 2.0.0
 * 
 * Features:
 * - No external crypto library dependencies
 * - Multiple entropy sources with health monitoring
 * - CPU jitter entropy (x86/x64 RDTSC, ARM CNTVCT)
 * - Hardware RNG support (RDRAND, RDSEED)
 * - OS entropy sources (/dev/urandom, getrandom(), RtlGenRandom)
 * - Cryptographic mixing with ChaCha20-based PRF
 * - Continuous health tests (NIST SP 800-90B)
 * - Thread-safe and memory-secure
 * 
 * Cross-platform: Linux, macOS, BSD, Windows
 */

#ifndef CSPRNG_ENTROPY_H
#define CSPRNG_ENTROPY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define CSPRNG_VERSION_MAJOR 2
#define CSPRNG_VERSION_MINOR 0
#define CSPRNG_VERSION_PATCH 0

/* Maximum entropy sources */
#define CSPRNG_MAX_SOURCES     8
#define CSPRNG_MIN_ENTROPY     256    /* bits */
#define CSPRNG_HEALTH_WINDOW   512    /* samples for health tests */

/* Error codes */
#define CSPRNG_SUCCESS         0
#define CSPRNG_ERROR_NULL      -1
#define CSPRNG_ERROR_LENGTH    -2
#define CSPRNG_ERROR_SOURCE    -3
#define CSPRNG_ERROR_HEALTH    -4
#define CSPRNG_ERROR_INSUFFICIENT -5

/* Entropy source flags */
typedef enum {
    CSPRNG_SOURCE_NONE         = 0,
    CSPRNG_SOURCE_GETRANDOM    = (1 << 0),   /* Linux getrandom() */
    CSPRNG_SOURCE_URANDOM      = (1 << 1),   /* /dev/urandom */
    CSPRNG_SOURCE_RDRAND       = (1 << 2),   /* Intel RDRAND */
    CSPRNG_SOURCE_RDSEED       = (1 << 3),   /* Intel RDSEED */
    CSPRNG_SOURCE_JITTER       = (1 << 4),   /* CPU timing jitter */
    CSPRNG_SOURCE_TIMER        = (1 << 5),   /* High-resolution timers */
    CSPRNG_SOURCE_WIN32_CRYPTO = (1 << 6),   /* Windows RtlGenRandom */
    CSPRNG_SOURCE_SYSCTL       = (1 << 7),   /* BSD/macOS sysctl KERN_ARND */
} csprng_source_flags_t;

/* Health test result */
typedef enum {
    CSPRNG_HEALTH_PASS     = 0,
    CSPRNG_HEALTH_FAIL     = 1,
    CSPRNG_HEALTH_DEGRADED = 2,
} csprng_health_status_t;

/* Entropy source info */
typedef struct {
    const char *name;
    csprng_source_flags_t flag;
    bool available;
    bool healthy;
    uint64_t bytes_contributed;
    uint32_t error_count;
} csprng_source_info_t;

/* Main context structure */
typedef struct {
    uint8_t  state[64];           /* Internal state buffer */
    uint64_t counter;             /* Counter for mixing */
    uint32_t sources_available;   /* Bitmask of available sources */
    uint32_t sources_healthy;     /* Bitmask of healthy sources */
    bool     initialized;
    
    /* Health monitoring */
    uint64_t health_samples[CSPRNG_HEALTH_WINDOW];
    size_t   health_index;
    uint32_t health_failures;
    
    /* Source statistics */
    csprng_source_info_t sources[CSPRNG_MAX_SOURCES];
    size_t               source_count;
} csprng_ctx_t;

/* ============================================================================
 * Core API Functions
 * ============================================================================ */

/**
 * @brief Initialize CSPRNG context
 * @param ctx Pointer to context structure
 * @return CSPRNG_SUCCESS on success, error code on failure
 */
int csprng_init(csprng_ctx_t *ctx);

/**
 * @brief Clean up CSPRNG context (secure memory zeroing)
 * @param ctx Pointer to context structure
 */
void csprng_cleanup(csprng_ctx_t *ctx);

/**
 * @brief Get random bytes from CSPRNG
 * @param ctx Initialized context
 * @param buffer Output buffer
 * @param length Number of bytes to generate
 * @return CSPRNG_SUCCESS on success, error code on failure
 */
int csprng_get_bytes(csprng_ctx_t *ctx, uint8_t *buffer, size_t length);

/**
 * @brief Get entropy directly from system sources (no PRNG)
 * @param buffer Output buffer
 * @param length Number of bytes
 * @return CSPRNG_SUCCESS on success, error code on failure
 */
int csprng_get_entropy(uint8_t *buffer, size_t length);

/* ============================================================================
 * Advanced API Functions
 * ============================================================================ */

/**
 * @brief Add entropy to the internal state
 * @param ctx Initialized context
 * @param data Entropy data
 * @param length Data length
 * @param estimated_bits Estimated entropy bits
 * @return CSPRNG_SUCCESS on success
 */
int csprng_add_entropy(csprng_ctx_t *ctx, const uint8_t *data, 
                       size_t length, size_t estimated_bits);

/**
 * @brief Force reseed from system entropy sources
 * @param ctx Initialized context
 * @return CSPRNG_SUCCESS on success
 */
int csprng_reseed(csprng_ctx_t *ctx);

/**
 * @brief Check health status of entropy sources
 * @param ctx Initialized context
 * @return Health status
 */
csprng_health_status_t csprng_health_check(csprng_ctx_t *ctx);

/**
 * @brief Get available entropy sources
 * @param ctx Initialized context
 * @return Bitmask of available sources
 */
uint32_t csprng_get_sources(csprng_ctx_t *ctx);

/**
 * @brief Get source information
 * @param ctx Initialized context
 * @param source Source flag
 * @return Pointer to source info, or NULL if not found
 */
const csprng_source_info_t* csprng_get_source_info(
    csprng_ctx_t *ctx, csprng_source_flags_t source);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get error description string
 * @param error_code Error code from API functions
 * @return Human-readable error description
 */
const char* csprng_strerror(int error_code);

/**
 * @brief Get version string
 * @return Version string (e.g., "2.0.0")
 */
const char* csprng_version(void);

/**
 * @brief Perform startup self-test
 * @return true if all tests pass
 */
bool csprng_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* CSPRNG_ENTROPY_H */
