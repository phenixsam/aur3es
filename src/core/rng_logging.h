/*
 * ============================================================================
 * GOLDEN RNG - LOGGING MODULE
 * ============================================================================
 * Configurable logging with multiple levels and outputs
 * ============================================================================
 */

#ifndef RNG_LOGGING_H
#define RNG_LOGGING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * LOG LEVELS
 * ============================================================================ */

typedef enum {
    RNG_LOG_ERROR = 0,
    RNG_LOG_WARN = 1,
    RNG_LOG_INFO = 2,
    RNG_LOG_DEBUG = 3,
    RNG_LOG_TRACE = 4
} rng_log_level_t;

/* ============================================================================
 * LOG OUTPUT
 * ============================================================================ */

typedef enum {
    RNG_LOG_OUTPUT_STDOUT = 0,
    RNG_LOG_OUTPUT_STDERR = 1,
    RNG_LOG_OUTPUT_FILE = 2,
    RNG_LOG_OUTPUT_SYSLOG = 3,
    RNG_LOG_OUTPUT_CALLBACK = 4
} rng_log_output_t;

/* ============================================================================
 * LOG CONFIGURATION
 * ============================================================================ */

typedef struct {
    rng_log_level_t level;
    rng_log_output_t output;
    bool use_colors;
    bool use_timestamps;
    char filename[256];
    size_t max_file_size;
    int max_files;
} rng_log_config_t;

/* ============================================================================
 * LOGGING FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize logging system
 * @param config Logging configuration (NULL for defaults)
 */
void rng_log_init(const rng_log_config_t *config);

/**
 * @brief Set logging level
 * @param level New logging level
 */
void rng_log_set_level(rng_log_level_t level);

/**
 * @brief Set output destination
 * @param output Output type
 * @param filename File name (if output is FILE)
 */
void rng_log_set_output(rng_log_output_t output, const char *filename);

/**
 * @brief Log a message
 * @param level Log level
 * @param format Format string
 * @param ... Arguments
 */
void rng_log(rng_log_level_t level, const char *format, ...);

/**
 * @brief Log error message
 */
#define RNG_LOG_ERROR(fmt, ...) rng_log(RNG_LOG_ERROR, fmt, ##__VA_ARGS__)

/**
 * @brief Log warning message
 */
#define RNG_LOG_WARN(fmt, ...) rng_log(RNG_LOG_WARN, fmt, ##__VA_ARGS__)

/**
 * @brief Log info message
 */
#define RNG_LOG_INFO(fmt, ...) rng_log(RNG_LOG_INFO, fmt, ##__VA_ARGS__)

/**
 * @brief Log debug message
 */
#define RNG_LOG_DEBUG(fmt, ...) rng_log(RNG_LOG_DEBUG, fmt, ##__VA_ARGS__)

/**
 * @brief Log trace message
 */
#define RNG_LOG_TRACE(fmt, ...) rng_log(RNG_LOG_TRACE, fmt, ##__VA_ARGS__)

/**
 * @brief Flush log buffers
 */
void rng_log_flush(void);

/**
 * @brief Shutdown logging system
 */
void rng_log_shutdown(void);

#endif /* RNG_LOGGING_H */

