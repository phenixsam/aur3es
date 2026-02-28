/*
 * ============================================================================
 * GOLDEN RNG - SECURITY HARDENING IMPLEMENTATION
 * ============================================================================
 */

#define _POSIX_C_SOURCE 199309L

#include "rng_security.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* Default security configuration */
/* 
 * SECURITY THRESHOLDS DOCUMENTATION:
 * 
 * - max_buffer_size: Maximum buffer allocation for RNG operations
 *   Default: 1GB - Allows large bulk generation while preventing excessive memory
 *   Rationale: 1GB is sufficient for most use cases; larger allocations may
 *   indicate potential denial-of-service attempts
 * 
 * - max_seed_size: Maximum seed length for rng_set_seed()
 *   Default: 1KB - Allows seeds up to 1024 bytes
 *   Rationale: 256-512 bits (32-64 bytes) is cryptographically sufficient;
 *   larger seeds don't provide additional security but consume memory
 * 
 * - min_generation_interval_ns: Minimum time between generations (0 = disabled)
 *   Default: 0 (disabled) - No rate limiting by default for performance
 *   Rationale: Rate limiting may be needed in adversarial environments or
 *   to prevent side-channel attacks; enable via rng_security_config for
 *   high-security deployments
 * 
 * - enable_strict_mode: Enable strict validation (reject weak seeds)
 *   Default: 0 (disabled) - Backward compatible
 *   Rationale: Rejecting seeds with all identical bytes may break some
 *   existing applications; enable for new high-security deployments
 */
static const rng_security_config_t default_config = {
    .max_buffer_size = 1024 * 1024 * 1024,  /* 1 GB max allocation */
    .max_seed_size = 1024,                     /* 1 KB max seed */
    .enable_time_checks = 1,
    .enable_rate_limiting = 1,
    .min_generation_interval_ns = 0,           /* No minimum by default */
    .enable_strict_mode = 0
};

/* ============================================================================
 * API IMPLEMENTATION
 * ============================================================================ */

rng_security_config_t rng_security_default_config(void) {
    return default_config;
}

int rng_security_init(rng_security_context_t *ctx, const rng_security_config_t *config) {
    if (!ctx) return RNG_SEC_ERROR_NULL_POINTER;
    
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = default_config;
    }
    
    ctx->last_generation_time = 0;
    ctx->total_generations = 0;
    ctx->total_bytes_generated = 0;
    ctx->rate_limit_exceeded = 0;
    
    return RNG_SEC_OK;
}

int rng_security_validate_buffer(const void *buffer, size_t size) {
    if (!buffer) {
        return RNG_SEC_ERROR_NULL_POINTER;
    }
    
    if (size == 0) {
        return RNG_SEC_ERROR_INVALID_SIZE;
    }
    
    if (size > default_config.max_buffer_size) {
        return RNG_SEC_ERROR_INVALID_SIZE;
    }
    
    return RNG_SEC_OK;
}

int rng_security_validate_seed(const uint8_t *seed, size_t seed_len, size_t max_len) {
    if (!seed) {
        return RNG_SEC_ERROR_NULL_POINTER;
    }
    
    if (seed_len == 0) {
        return RNG_SEC_ERROR_INVALID_SEED;
    }
    
    if (seed_len > max_len) {
        return RNG_SEC_ERROR_INVALID_SEED;
    }
    
    if (seed_len > default_config.max_seed_size) {
        return RNG_SEC_ERROR_INVALID_SEED;
    }
    
    /* Check for obviously weak seeds */
    int all_same = 1;
    for (size_t i = 1; i < seed_len; i++) {
        if (seed[i] != seed[0]) {
            all_same = 0;
            break;
        }
    }
    
    if (all_same && seed_len > 1 && default_config.enable_strict_mode) {
        return RNG_SEC_ERROR_INVALID_SEED;
    }
    
    return RNG_SEC_OK;
}

int rng_security_check_rate_limit(rng_security_context_t *ctx) {
    if (!ctx) return RNG_SEC_ERROR_NULL_POINTER;
    
    if (!ctx->config.enable_rate_limiting) {
        return RNG_SEC_OK;
    }
    
    /* Get current time */
    uint64_t now = 0;
#if defined(__linux__) || defined(__unix__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
    
    /* Check minimum interval */
    if (ctx->config.min_generation_interval_ns > 0) {
        if (now - ctx->last_generation_time < ctx->config.min_generation_interval_ns) {
            ctx->rate_limit_exceeded++;
            return RNG_SEC_ERROR_RATE_LIMITED;
        }
    }
    
    ctx->last_generation_time = now;
    return RNG_SEC_OK;
}

void rng_security_update_stats(rng_security_context_t *ctx, size_t bytes_generated) {
    if (!ctx) return;
    
    ctx->total_generations++;
    ctx->total_bytes_generated += bytes_generated;
}

const char* rng_security_error_string(int error) {
    switch (error) {
        case RNG_SEC_OK:                     return "Success";
        case RNG_SEC_ERROR_NULL_POINTER:      return "Null pointer";
        case RNG_SEC_ERROR_INVALID_SIZE:     return "Invalid size";
        case RNG_SEC_ERROR_INVALID_SEED:      return "Invalid seed";
        case RNG_SEC_ERROR_OUT_OF_MEMORY:     return "Out of memory";
        case RNG_SEC_ERROR_ENTROPY_FAILURE:   return "Entropy failure";
        case RNG_SEC_ERROR_INVALID_STATE:     return "Invalid state";
        case RNG_SEC_ERROR_RNG_NOT_INITIALIZED: return "RNG not initialized";
        case RNG_SEC_ERROR_BUFFER_TOO_SMALL:  return "Buffer too small";
        case RNG_SEC_ERROR_RATE_LIMITED:      return "Rate limited";
        default:                             return "Unknown error";
    }
}

/*
 * Constant-time comparison - prevents timing attacks
 * Returns 0 if equal, non-zero otherwise
 */
int rng_security_constant_time_compare(const uint8_t *a, const uint8_t *b, size_t len) {
    if (!a || !b) return 1;
    
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    
    return diff;
}

/*
 * Validate RNG state integrity
 * Checks for obviously invalid states
 */
bool rng_security_validate_state(const uint64_t *state) {
    if (!state) return false;
    
    /* Check for all zeros (uninitialized) */
    uint64_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum |= state[i];
    }
    
    if (sum == 0) return false;
    
    /* Check for all ones (corrupted) */
    uint64_t all_ones = 0xFFFFFFFFFFFFFFFFULL;
    int all_ones_count = 0;
    for (int i = 0; i < 8; i++) {
        if (state[i] == all_ones) all_ones_count++;
    }
    
    if (all_ones_count >= 7) return false;
    
    return true;
}

void rng_security_clear(rng_security_context_t *ctx) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(rng_security_context_t));
}

