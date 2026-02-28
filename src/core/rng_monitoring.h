/*
 * ============================================================================
 * GOLDEN RNG - STATISTICS AND MONITORING MODULE
 * ============================================================================
 * Exposes generation statistics, entropy monitoring, and health checks
 * ============================================================================
 */

#ifndef RNG_MONITORING_H
#define RNG_MONITORING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * STATISTICS STRUCTURES
 * ============================================================================ */

/**
 * @brief Generation statistics
 */
typedef struct {
    uint64_t total_bytes_generated;
    uint64_t total_blocks_generated;
    uint64_t total_operations;
    uint64_t reseed_count;
    uint64_t last_reseed_time;
    uint64_t uptime_ns;
    size_t memory_used_bytes;
    int thread_count;
    uint64_t entropy_bits_collected;
} rng_statistics_t;

/**
 * @brief Entropy source status
 */
typedef struct {
    int entropy_available;
    int entropy_quality;      /* 0-100 */
    uint64_t entropy_bits_collected;
    uint64_t last_entropy_collection;
    const char* primary_source;
    const char* backup_source;
} rng_entropy_status_t;

/**
 * @brief Performance metrics
 */
typedef struct {
    double current_throughput_mbps;
    double average_throughput_mbps;
    double peak_throughput_mbps;
    uint64_t operations_per_second;
    double cpu_usage_percent;
    size_t buffer_size_bytes;
} rng_performance_metrics_t;

/**
 * @brief Health check result
 */
typedef struct {
    bool rng_initialized;
    bool entropy_adequate;
    bool memory_adequate;
    bool thread_safe;
    int health_score;        /* 0-100 */
    const char* status_message;
} rng_health_check_t;

/* ============================================================================
 * MONITORING FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get current generation statistics
 * @param stats Statistics structure to fill
 */
void rng_monitor_get_statistics(rng_statistics_t *stats);

/**
 * @brief Get entropy source status
 * @param status Entropy status structure to fill
 */
void rng_monitor_get_entropy_status(rng_entropy_status_t *status);

/**
 * @brief Get performance metrics
 * @param metrics Performance metrics structure to fill
 */
void rng_monitor_get_performance(rng_performance_metrics_t *metrics);

/**
 * @brief Perform health check
 * @return Health check result
 */
rng_health_check_t rng_monitor_health_check(void);

/**
 * @brief Print monitoring report
 */
void rng_monitor_print_report(void);

/**
 * @brief Reset statistics counters
 */
void rng_monitor_reset_statistics(void);

/**
 * @brief Register callback for health events
 * @param callback Function to call on health events
 */
typedef void (*rng_health_callback_t)(const rng_health_check_t *health);
void rng_monitor_set_health_callback(rng_health_callback_t callback);

/**
 * @brief Record an operation (for statistics tracking)
 * @param bytes Number of bytes generated in this operation
 */
void rng_monitor_record_operation(uint64_t bytes);

/**
 * @brief Record throughput measurement
 * @param mbps Throughput in MB/s
 */
void rng_monitor_record_throughput(double mbps);

/**
 * @brief Record entropy collection
 * @param bits Number of entropy bits collected
 */
void rng_monitor_record_entropy(uint64_t bits);

#endif /* RNG_MONITORING_H */

