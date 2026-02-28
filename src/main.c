/*
 * Copyright (C) 2024-2026 Golden RNG Enterprise
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * GOLDEN RNG ENTERPRISE - MAIN
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <limits.h>

#include "core/rng_engine.h"
#include "utils/rng_advanced_tests.h"
#include "testu01_bigcrush.h"
#include "platform/platform_abstraction.h"

static volatile int g_running = 1;
static golden_rng_t *g_rng = NULL;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void print_banner(void) {
    fprintf(stderr, "\n======================================================\n");
    fprintf(stderr, "  GOLDEN RNG ENTERPRISE v5.0\n");
    fprintf(stderr, "  Multiplatform Cryptographic Generator\n");
    fprintf(stderr, "  Platform: %s (%s)\n", PLATFORM_NAME, PLATFORM_ARCH);
    fprintf(stderr, "======================================================\n\n");
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  -m, --mode MODE    Mode: stdout, file, stats, test, simulate, benchmark, simd, kat, unittests, determinism, advanced\n");
    printf("  -d, --dist DIST    Distribution: bytes, uniform, normal\n");
    printf("  -o, --output FILE  Output file\n");
    printf("  -s, --size SIZE    Size (1G, 100M, 500K)\n");
    printf("  -S, --seed SEED    Seed for deterministic RNG\n");
    printf("  -t, --threads NUM  Number of threads for parallel benchmark\n");
    printf("  -v, --verbose      Verbose mode\n");
    printf("  -V, --version      Show version\n");
    printf("  -h, --help         Show help\n\n");
    printf("Test modes:\n");
    printf("  test         - Basic RNG test\n");
    printf("  kat          - Known Answer Tests\n");
    printf("  unittests    - Complete unit test suite\n");
    printf("  determinism  - Verify RNG determinism\n");
    printf("  simd         - SIMD benchmark and CPU detection\n");
    printf("  advanced     - Advanced statistical tests (diffusion, correlation, etc.)\n\n");
}

static void mode_simulation(golden_rng_t *rng, size_t output_size) {
    if (output_size == 0) output_size = 10000000;

    uint64_t inside = 0, total = output_size;
    clock_t start = clock();

    for (uint64_t i = 0; i < total; i++) {
        double x = rng_double(rng) * 2 - 1;
        double y = rng_double(rng) * 2 - 1;
        if (x*x + y*y <= 1.0) inside++;
    }

    clock_t end = clock();
    double pi = 4.0 * inside / total;
    double t = (double)(end - start) / CLOCKS_PER_SEC;

    printf("=== Monte Carlo Pi Estimation ===\n");
    printf("Points: %lu\n", (unsigned long)total);
    printf("Inside: %lu\n", (unsigned long)inside);
    printf("Pi estimated: %.8f\n", pi);
    printf("Pi actual: %.8f\n", 3.14159265);
    printf("Error: %.6f%%\n", fabs(pi - 3.14159265) / 3.14159265 * 100);
    printf("Time: %.2f sec\n", t);
}

static void mode_benchmark(golden_rng_t *rng, size_t buffer_size, int max_threads) {
    printf("=== Parallel Benchmark Mode ===\n\n");
    printf("Running benchmark with:\n");
    printf("  Buffer size: %.2f MB\n", buffer_size / (1024.0 * 1024.0));
    printf("  Max threads: %d\n\n", max_threads);
    
    int result = rng_benchmark_parallel(rng, buffer_size, max_threads);
    
    if (result == 0) {
        printf("\nBenchmark completed successfully.\n");
    } else {
        printf("\nBenchmark error.\n");
    }
}

static void mode_simd_benchmark(golden_rng_t *rng, size_t buffer_size) {
    printf("=== SIMD Benchmark Mode ===\n\n");
    printf("Running SIMD benchmark with:\n");
    printf("  Buffer size: %.2f MB\n", buffer_size / (1024.0 * 1024.0));
    printf("\n");
    
    rng_print_system_info();
    
    simd_benchmark_result_t result = rng_benchmark_simd(rng, buffer_size, 5);
    rng_benchmark_simd_print_report(&result);
}

static void mode_kat_tests(void) {
    printf("=== Known Answer Tests (KAT) Mode ===\n\n");
    
    determinism_result_t det_result = verify_determinism(
        (uint8_t*)"KAT-Test-Seed", 13, 1000);
    determinism_print_report(&det_result);
    
    if (!det_result.is_deterministic) {
        printf("\nERROR: RNG must be deterministic for KAT tests!\n");
        return;
    }
    
    printf("\n");
    
    kat_result_t kat_result = run_kat_tests();
    kat_print_report(&kat_result);
}

static void mode_unit_tests(void) {
    printf("=== Unit Tests Mode ===\n\n");
    
    unit_test_result_t result = run_all_unit_tests();
    unit_test_print_report(&result);
}

static void mode_determinism_test(void) {
    printf("=== Determinism Test Mode ===\n\n");
    
    printf("Test 1: Same seed -> Same sequence\n");
    determinism_result_t result1 = verify_determinism(
        (uint8_t*)"DeterminismTest1", 16, 10000);
    determinism_print_report(&result1);
    
    printf("\n");
    
    printf("Test 2: Different seeds -> Different sequences\n");
    rng_config_t cfg = rng_default_config();
    cfg.deterministic = 1;
    
    golden_rng_t *rng1 = rng_create(&cfg);
    rng_set_seed(rng1, (uint8_t*)"SeedA", 5);
    uint64_t seq1[10];
    for (int i = 0; i < 10; i++) {
        seq1[i] = rng_uint64(rng1);
    }
    rng_destroy(rng1);
    
    golden_rng_t *rng2 = rng_create(&cfg);
    rng_set_seed(rng2, (uint8_t*)"SeedB", 5);
    uint64_t seq2[10];
    for (int i = 0; i < 10; i++) {
        seq2[i] = rng_uint64(rng2);
    }
    rng_destroy(rng2);
    
    int different = 0;
    for (int i = 0; i < 10; i++) {
        if (seq1[i] != seq2[i]) different++;
    }
    
    printf("Comparison: %d/10 different values\n", different);
    printf("Status: %s\n", different > 0 ? "PASS" : "FAIL");
    
    printf("\n=== Determinism Summary ===\n");
    printf("Stable sequence: %s\n", result1.is_deterministic ? "YES" : "NO");
    printf("Different seeds: %s\n", different > 0 ? "YES" : "NO");
    printf("Verdict: %s\n", 
           (result1.is_deterministic && different > 0) ? "PASS" : "FAIL");
}

static void mode_advanced_tests(void) {
    printf("=== Advanced Tests Mode ===\n\n");
    rng_config_t cfg = rng_default_config();
    cfg.deterministic = 1;
    golden_rng_t *test_rng = rng_create(&cfg);
    if (test_rng) {
        rng_set_seed(test_rng, (uint8_t*)"AdvancedTest", 13);
        advanced_test_result_t result = run_advanced_tests(test_rng);
        advanced_test_print_summary(&result);
        
        printf("\n=== Monte Carlo Pi Estimation Tests ===\n\n");
        (void)run_monte_carlo_tests(test_rng);
        
        rng_destroy(test_rng);
    } else {
        printf("Error: Could not create RNG for advanced tests\n");
    }
}

static void write_output_with_distribution(golden_rng_t *rng, FILE *f, 
                                            size_t output_size, int dist,
                                            int verbose) {
    uint64_t block[4];
    size_t total = 0;
    size_t report_interval = output_size / 10;
    if (report_interval == 0) report_interval = 1024 * 1024;
    
    while (g_running && (output_size == 0 || total < output_size)) {
        if (dist == 0) {
            /* bytes mode - raw binary output */
            rng_generate_block(rng, block);
            fwrite(block, 32, 1, f);
            total += 32;
        } else if (dist == 1) {
            /* uniform mode - double values in [0, 1) */
            double val = rng_double(rng);
            fwrite(&val, sizeof(double), 1, f);
            total += sizeof(double);
        } else if (dist == 2) {
            /* normal mode - Gaussian distribution N(0, 1) */
            double val = rng_normal(rng, 0.0, 1.0);
            fwrite(&val, sizeof(double), 1, f);
            total += sizeof(double);
        }
        
        if (verbose && output_size > 0 && (total % report_interval) < 32) {
            printf("Progress: %.1f%% (%zu / %zu bytes)\n", 
                   100.0 * total / output_size, total, output_size);
        }
    }
}

/* Operation modes */
typedef enum {
    MODE_STDOUT = 0,
    MODE_FILE = 1,
    MODE_STATS = 2,
    MODE_TEST = 3,
    MODE_SIMULATE = 4,
    MODE_BENCHMARK = 5,
    MODE_SIMD = 6,
    MODE_KAT = 7,
    MODE_UNIT_TESTS = 8,
    MODE_DETERMINISM = 9,
    MODE_ADVANCED = 10
} operation_mode_t;

/* Output distribution */
typedef enum {
    DIST_BYTES = 0,
    DIST_UNIFORM = 1,
    DIST_NORMAL = 2
} distribution_t;

int main(int argc, char *argv[]) {
    operation_mode_t mode = MODE_STDOUT;
    distribution_t dist = DIST_BYTES;
    char output_file[512] = {0};
    size_t output_size = 0;
    int verbose = 0;
    char seed_str[256] = {0};
    int benchmark_threads = 0;
    int unknown_arg = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "stdout") == 0) mode = MODE_STDOUT;
            else if (strcmp(argv[i+1], "file") == 0) mode = MODE_FILE;
            else if (strcmp(argv[i+1], "stats") == 0) mode = MODE_STATS;
            else if (strcmp(argv[i+1], "test") == 0) mode = MODE_TEST;
            else if (strcmp(argv[i+1], "simulate") == 0) mode = MODE_SIMULATE;
            else if (strcmp(argv[i+1], "benchmark") == 0) mode = MODE_BENCHMARK;
            else if (strcmp(argv[i+1], "simd") == 0) mode = MODE_SIMD;
            else if (strcmp(argv[i+1], "kat") == 0) mode = MODE_KAT;
            else if (strcmp(argv[i+1], "unittests") == 0) mode = MODE_UNIT_TESTS;
            else if (strcmp(argv[i+1], "determinism") == 0) mode = MODE_DETERMINISM;
            else if (strcmp(argv[i+1], "advanced") == 0) mode = MODE_ADVANCED;
            else { unknown_arg = 1; }
            i++;
        }
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "bytes") == 0) dist = DIST_BYTES;
            else if (strcmp(argv[i+1], "uniform") == 0) dist = DIST_UNIFORM;
            else if (strcmp(argv[i+1], "normal") == 0) dist = DIST_NORMAL;
            else { unknown_arg = 1; }
            i++;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            /* Use snprintf to avoid buffer overflow */
            snprintf(output_file, sizeof(output_file), "%s", argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            char *endptr;
            unsigned long long temp_size = strtoull(argv[i+1], &endptr, 10);
            
            /* SECURITY: Fixed integer overflow check - check BEFORE multiplication */
            if (*endptr) {
                if (*endptr == 'K' || *endptr == 'k') {
                    /* Check for potential overflow before multiplying */
                    if (temp_size > (ULLONG_MAX / 1024)) {
                        fprintf(stderr, "Warning: size overflow, limiting to max\n");
                        temp_size = ULLONG_MAX;
                    } else {
                        temp_size *= 1024;
                    }
                }
                else if (*endptr == 'M' || *endptr == 'm') {
                    /* Check for potential overflow before multiplying */
                    if (temp_size > (ULLONG_MAX / (1024 * 1024))) {
                        fprintf(stderr, "Warning: size overflow, limiting to max\n");
                        temp_size = ULLONG_MAX;
                    } else {
                        temp_size *= (1024 * 1024);
                    }
                }
                else if (*endptr == 'G' || *endptr == 'g') {
                    /* Check for potential overflow before multiplying */
                    if (temp_size > (ULLONG_MAX / (1024ULL * 1024 * 1024))) {
                        fprintf(stderr, "Warning: size overflow, limiting to max\n");
                        temp_size = ULLONG_MAX;
                    } else {
                        temp_size *= (1024ULL * 1024 * 1024);
                    }
                }
                else {
                    /* Invalid suffix */
                    unknown_arg = 1;
                }
            }
            /* Validate maximum size (max 1TB) */
            if (temp_size > 1099511627776ULL) {
                fprintf(stderr, "Warning: size too large, limiting to 1TB\n");
                output_size = 1099511627776ULL;
            } else {
                output_size = (size_t)temp_size;
            }
            i++;
        }
        else if ((strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--seed") == 0) && i + 1 < argc) {
            /* Use snprintf to avoid buffer overflow */
            snprintf(seed_str, sizeof(seed_str), "%s", argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            /* Use strtol for proper validation */
            char *endptr;
            long val = strtol(argv[i+1], &endptr, 10);
            if (*endptr != '\0' || val < 0) {
                benchmark_threads = 0;
                unknown_arg = 1;
            } else if (val > 64) {
                benchmark_threads = 64;  /* Limit to 64 threads max */
            } else {
                benchmark_threads = (int)val;
            }
            i++;
        }
        else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("Golden RNG Enterprise v5.0.0\n");
            printf("AUR3ES Cryptographic PRNG\n");
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
        else {
            unknown_arg = 1;
        }
    }

    /* Show help if there was an unknown argument */
    if (unknown_arg) {
        fprintf(stderr, "Error: Invalid arguments\n");
        print_usage(argv[0]);
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    print_banner();

    rng_config_t cfg = rng_default_config();

    if (seed_str[0]) {
        cfg.deterministic = 1;
        if (verbose) {
            printf("Deterministic mode activated\n");
        }
        printf("Seed: %s\n", seed_str);
    }

    g_rng = rng_create(&cfg);

    if (!g_rng && mode != MODE_KAT && mode != MODE_UNIT_TESTS && mode != MODE_DETERMINISM && mode != MODE_ADVANCED) {
        fprintf(stderr, "Error: cannot create RNG\n");
        return 1;
    }

    if (seed_str[0] && g_rng) {
        rng_set_seed(g_rng, (const uint8_t*)seed_str, strlen(seed_str));
    }

    uint64_t block[4];
    size_t total = 0;
    uint64_t start = pl_time_ms();

    if (mode == MODE_STDOUT) {
        /* stdout mode - raw binary output */
        if (verbose) {
            printf("Generating binary data to stdout...\n");
            printf("Distribution: %s\n", dist == 0 ? "bytes" : 
                                          dist == 1 ? "uniform" : "normal");
        }
        while (g_running) {
            rng_generate_block(g_rng, block);
            fwrite(block, 32, 1, stdout);
            total += 32;
        }
    }
    else if (mode == MODE_FILE) {
        if (!output_file[0]) {
            fprintf(stderr, "Error: output file required (-o)\n");
            rng_destroy(g_rng);
            return 1;
        }
        FILE *f = fopen(output_file, "wb");
        if (!f) {
            fprintf(stderr, "Error: cannot create %s\n", output_file);
            rng_destroy(g_rng);
            return 1;
        }
        if (verbose) {
            printf("Writing to %s...\n", output_file);
            printf("Distribution: %s\n", dist == 0 ? "bytes" : 
                                          dist == 1 ? "uniform" : "normal");
            printf("Size: %zu bytes\n", output_size);
        } else {
            printf("Writing to %s...\n", output_file);
        }
        write_output_with_distribution(g_rng, f, output_size, dist, verbose);
        fclose(f);
    }
    else if (mode == MODE_STATS) {
        printf("System Statistics\n");
        printf("========================\n\n");
        char info[256];
        pl_system_info(info, sizeof(info));
        printf("System: %s\n\n", info);
        uint64_t blocks, reseeds, bytes;
        rng_get_stats(g_rng, &blocks, &reseeds, &bytes);
        printf("Blocks: %llu, Reseeds: %llu, Bytes: %llu\n",
               (unsigned long long)blocks, (unsigned long long)reseeds, (unsigned long long)bytes);
    }
    else if (mode == MODE_TEST) {
        printf("Random Number Test\n");
        printf("===========================\n\n");
        printf("uint64: 0x%016llX\n", (unsigned long long)rng_uint64(g_rng));
        printf("uint32: 0x%08X\n", rng_uint32(g_rng));
        printf("double: %.15f\n", rng_double(g_rng));
        printf("normal: %.6f\n", rng_normal(g_rng, 0.0, 1.0));
        if (verbose) {
            printf("\nDistribution: %s\n", dist == 0 ? "bytes" : 
                                           dist == 1 ? "uniform" : "normal");
        }
    }
    else if (mode == MODE_SIMULATE) {
        mode_simulation(g_rng, output_size);
    }
    else if (mode == MODE_BENCHMARK) {
        size_t bench_size = output_size > 0 ? output_size : 10 * 1024 * 1024;
        mode_benchmark(g_rng, bench_size, benchmark_threads);
    }
    else if (mode == MODE_SIMD) {
        size_t bench_size = output_size > 0 ? output_size : 10 * 1024 * 1024;
        mode_simd_benchmark(g_rng, bench_size);
    }
    else if (mode == MODE_KAT) {
        mode_kat_tests();
    }
    else if (mode == MODE_UNIT_TESTS) {
        mode_unit_tests();
    }
    else if (mode == MODE_DETERMINISM) {
        mode_determinism_test();
    }
    else if (mode == MODE_ADVANCED) {
        mode_advanced_tests();
    }
    else {
        print_usage(argv[0]);
    }

    uint64_t elapsed = pl_time_ms() - start;
    if (mode != MODE_KAT && mode != MODE_UNIT_TESTS && mode != MODE_DETERMINISM && mode != MODE_ADVANCED) {
        printf("\nTime: %.2f seconds\n", elapsed / 1000.0);
        if (verbose && total > 0) {
            printf("Total generated: %zu bytes\n", total);
            printf("Speed: %.2f MB/s\n", 
                   (double)total / (1024.0 * 1024.0) / (elapsed / 1000.0));
        }
        rng_destroy(g_rng);
    }
    printf("\nGolden RNG Enterprise v5.0 - Finished.\n");

    return 0;
}

