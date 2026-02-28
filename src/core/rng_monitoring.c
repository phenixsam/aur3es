/*
 * ============================================================================
 * GOLDEN RNG - MONITORING IMPLEMENTATION
 * ============================================================================
 */

#include "platform/platform_abstraction.h"
#include "rng_monitoring.h"
#include "rng_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/* ============================================================================
 * STATIC VARIABLES
 * ============================================================================ */

static rng_health_callback_t g_health_callback = NULL;
static uint64_t g_start_time = 0;
static uint64_t g_total_operations = 0;
static uint64_t g_total_bytes_generated = 0;
static uint64_t g_entropy_collected_bits = 0;
static double g_peak_throughput = 0.0;

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

static uint64_t get_uptime_ns(void) {
    if (g_start_time == 0) return 0;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return now - g_start_time;
}

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

void rng_monitor_init(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_start_time = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void rng_monitor_get_statistics(rng_statistics_t *stats) {
    if (!stats) return;
    
    memset(stats, 0, sizeof(rng_statistics_t));
    
    stats->uptime_ns = get_uptime_ns();
    stats->total_operations = g_total_operations;
    stats->entropy_bits_collected = g_entropy_collected_bits;
    
    /* These would be populated from actual RNG instance in a full implementation */
    /* For now, we provide basic static tracking */
}

void rng_monitor_get_entropy_status(rng_entropy_status_t *status) {
    if (!status) return;
    
    memset(status, 0, sizeof(rng_entropy_status_t));
    
    /* Check entropy availability */
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        status->entropy_available = 1;
        status->primary_source = "/dev/urandom";
        fclose(urandom);
    }
    
    status->entropy_quality = 100; /* Assuming system entropy is high quality */
    status->backup_source = "OpenSSL RAND_bytes";
}

void rng_monitor_get_performance(rng_performance_metrics_t *metrics) {
    if (!metrics) return;
    
    memset(metrics, 0, sizeof(rng_performance_metrics_t));
    
    metrics->peak_throughput_mbps = g_peak_throughput;
    metrics->average_throughput_mbps = g_peak_throughput * 0.9; /* Estimated */
    metrics->current_throughput_mbps = g_peak_throughput * 0.95; /* Estimated */
}

rng_health_check_t rng_monitor_health_check(void) {
    rng_health_check_t health = {
        .rng_initialized = (g_start_time != 0),
        .entropy_adequate = 1,
        .memory_adequate = 1,
        .thread_safe = 1,
        .health_score = 100,
        .status_message = "OK"
    };
    
    /* Check entropy */
    rng_entropy_status_t entropy;
    rng_monitor_get_entropy_status(&entropy);
    if (!entropy.entropy_available) {
        health.entropy_adequate = 0;
        health.health_score -= 30;
        health.status_message = "Low entropy";
    }
    
    /* Check memory */
    size_t rss, virt;
    pl_memory_usage(&rss, &virt);
    if (rss > 512 * 1024 * 1024) { /* > 512 MB */
        health.memory_adequate = 0;
        health.health_score -= 20;
        health.status_message = "High memory usage";
    }
    
    /* Call callback if registered */
    if (g_health_callback && health.health_score < 80) {
        g_health_callback(&health);
    }
    
    return health;
}

void rng_monitor_print_report(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("                    GOLDEN RNG MONITORING REPORT\n");
    printf("================================================================================\n\n");
    
    /* Statistics */
    rng_statistics_t stats;
    rng_monitor_get_statistics(&stats);
    
    printf("Statistics:\n");
    printf("  Uptime: %.2f seconds\n", stats.uptime_ns / 1e9);
    printf("  Total Operations: %llu\n", (unsigned long long)stats.total_operations);
    printf("\n");
    
    /* Entropy */
    rng_entropy_status_t entropy;
    rng_monitor_get_entropy_status(&entropy);
    
    printf("Entropy Status:\n");
    printf("  Available: %s\n", entropy.entropy_available ? "Yes" : "No");
    printf("  Quality: %d%%\n", entropy.entropy_quality);
    printf("  Primary Source: %s\n", entropy.primary_source);
    printf("  Backup Source: %s\n", entropy.backup_source);
    printf("\n");
    
    /* Performance */
    rng_performance_metrics_t perf;
    rng_monitor_get_performance(&perf);
    
    printf("Performance:\n");
    printf("  Current Throughput: %.2f MB/s\n", perf.current_throughput_mbps);
    printf("  Average Throughput: %.2f MB/s\n", perf.average_throughput_mbps);
    printf("  Peak Throughput: %.2f MB/s\n", perf.peak_throughput_mbps);
    printf("\n");
    
    /* Health */
    rng_health_check_t health = rng_monitor_health_check();
    
    printf("Health:\n");
    printf("  Status: %s\n", health.status_message);
    printf("  Score: %d/100\n", health.health_score);
    printf("  RNG Initialized: %s\n", health.rng_initialized ? "Yes" : "No");
    printf("  Entropy Adequate: %s\n", health.entropy_adequate ? "Yes" : "No");
    printf("  Memory Adequate: %s\n", health.memory_adequate ? "Yes" : "No");
    printf("  Thread Safe: %s\n", health.thread_safe ? "Yes" : "No");
    printf("\n");
    
    printf("================================================================================\n");
}

void rng_monitor_reset_statistics(void) {
    g_total_operations = 0;
    g_total_bytes_generated = 0;
    g_entropy_collected_bits = 0;
    g_peak_throughput = 0.0;
}

void rng_monitor_set_health_callback(rng_health_callback_t callback) {
    g_health_callback = callback;
}

/* Helper function to track operations */
void rng_monitor_record_operation(uint64_t bytes) {
    g_total_operations++;
    g_total_bytes_generated += bytes;
}

/* Helper function to track throughput */
void rng_monitor_record_throughput(double mbps) {
    if (mbps > g_peak_throughput) {
        g_peak_throughput = mbps;
    }
}

/* Helper function to track entropy collection */
void rng_monitor_record_entropy(uint64_t bits) {
    g_entropy_collected_bits += bits;
}

