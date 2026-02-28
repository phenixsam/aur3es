/*
 * ============================================================================
 * GOLDEN RNG - SECURITY HARDENING MODULE
 * ============================================================================
 * Enhanced security features for production deployment
 * - Input validation
 * - Error handling
 * - Resource limits
 * - Timing attack mitigation
 * ============================================================================
 */

#ifndef RNG_SECURITY_H
#define RNG_SECURITY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * ERROR CODES
 * ============================================================================ */

typedef enum {
    RNG_SEC_OK = 0,
    RNG_SEC_ERROR_NULL_POINTER = -1,
    RNG_SEC_ERROR_INVALID_SIZE = -2,
    RNG_SEC_ERROR_INVALID_SEED = -3,
    RNG_SEC_ERROR_OUT_OF_MEMORY = -4,
    RNG_SEC_ERROR_ENTROPY_FAILURE = -5,
    RNG_SEC_ERROR_INVALID_STATE = -6,
    RNG_SEC_ERROR_RNG_NOT_INITIALIZED = -7,
    RNG_SEC_ERROR_BUFFER_TOO_SMALL = -8,
    RNG_SEC_ERROR_RATE_LIMITED = -9
} rng_sec_error_t;

/* ============================================================================
 * SECURITY CONFIGURATION
 * ============================================================================ */

typedef struct {
    size_t max_buffer_size;        // Maximum allocation size
    size_t max_seed_size;          // Maximum seed length
    int enable_time_checks;        // Enable timing validation
    int enable_rate_limiting;      // Enable rate limiting
    uint64_t min_generation_interval_ns;  // Minimum time between generations
    int enable_strict_mode;        // Enable strict validation
} rng_security_config_t;

/* ============================================================================
 * SECURITY CONTEXT
 * ============================================================================ */

typedef struct {
    uint64_t last_generation_time;
    size_t total_generations;
    size_t total_bytes_generated;
    int rate_limit_exceeded;
    rng_security_config_t config;
} rng_security_context_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Get default security configuration
 * @return Default security configuration
 */
rng_security_config_t rng_security_default_config(void);

/**
 * @brief Initialize security context
 * @param ctx Security context to initialize
 * @param config Security configuration (NULL for defaults)
 * @return 0 on success, error code on failure
 */
int rng_security_init(rng_security_context_t *ctx, const rng_security_config_t *config);

/**
 * @brief Validate input buffer pointer and size
 * @param buffer Buffer to validate
 * @param size Requested size
 * @return 0 if valid, error code otherwise
 */
int rng_security_validate_buffer(const void *buffer, size_t size);

/**
 * @brief Validate seed input
 * @param seed Seed data
 * @param seed_len Seed length
 * @param max_len Maximum allowed length
 * @return 0 if valid, error code otherwise
 */
int rng_security_validate_seed(const uint8_t *seed, size_t seed_len, size_t max_len);

/**
 * @brief Check and update rate limiting
 * @param ctx Security context
 * @return 0 if allowed, RNG_SEC_ERROR_RATE_LIMITED if rate limited
 */
int rng_security_check_rate_limit(rng_security_context_t *ctx);

/**
 * @brief Update generation statistics
 * @param ctx Security context
 * @param bytes_generated Number of bytes generated
 */
void rng_security_update_stats(rng_security_context_t *ctx, size_t bytes_generated);

/**
 * @brief Get error string for error code
 * @param error Error code
 * @return Error string
 */
const char* rng_security_error_string(int error);

/**
 * @brief Secure comparison (constant-time)
 * @param a First buffer
 * @param b Second buffer
 * @param len Length to compare
 * @return 0 if equal, non-zero otherwise
 */
int rng_security_constant_time_compare(const uint8_t *a, const uint8_t *b, size_t len);

/**
 * @brief Validate RNG state integrity
 * @param state RNG state array (8 uint64_t)
 * @return true if valid, false if corrupted
 */
bool rng_security_validate_state(const uint64_t *state);

/**
 * @brief Clear security context
 * @param ctx Context to clear
 */
void rng_security_clear(rng_security_context_t *ctx);

#endif /* RNG_SECURITY_H */

