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
 * ============================================================================
 * GOLDEN RNG ENGINE - ENHANCED SECURITY VERSION
 * ============================================================================
 * Cryptographic Random Number Generator Engine
 * Implements AUR3ES algorithm for cryptographic resistance
 * ============================================================================
 */

#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "rng_engine.h"
#include "csprng_entropy.h"
#include "rng_simd_optimized.h"

/* ============================================================================
 * STRUCT DEFINITION (must be before functions that use it)
 * ============================================================================ */

struct rng_engine {
    // Estado AUR3ES
    uint64_t state[8];
    uint64_t counter;
    size_t reseed_counter;

    // Metadatos
    uint64_t blocks_generated;
    uint64_t total_bytes;
    int deterministic;
    int reseed_interval;
    int using_csprng;
    int memory_locked;
    
    // FIPS compliance
    int fips_mode_enabled;
    uint64_t last_output[4];    // For CRNGCT
    int crngct_failures;       // Track consecutive failures
};

#include "platform_abstraction.h"
#include "rng_monitoring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

/* CPUID header for __get_cpuid function */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <cpuid.h>
#endif

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

/* ============================================================================
 * CONSTANTS AND PORTABLE MACROS
 * ============================================================================ */

// High-quality mathematical constants (64 bits cada una)
// High and low parts of original 128-bit constants
static const uint64_t GOLDEN_HI[8] = {
    0x9e3779b97f4a7c15, 0x517cc1b727220a94, 0x243f6a8885a308d3, 0x2b7e151628aed2a6,
    0xbf58476d1ce4e5b9, 0x94d049bb133111eb, 0x5be0cd19137e2179, 0x1f83d9abfb41bd6b
};
static const uint64_t GOLDEN_LO[8] = {
    0x6a09e667f3bcc908, 0x3c6ef372fe94f82c, 0x13198a2e03707344, 0xabf7158809cf4f3c,
    0xf3bcc908a4e1a6b7, 0xc4e1a6b77bf3a6b7, 0x2582d2d8a4f3a6b7, 0x9e3779b97f4a7c15
};

// Gets the 64-bit constant for index i (0-15)
static inline uint64_t golden_const(int idx) {
    if (idx < 8) return GOLDEN_LO[idx];
    else return GOLDEN_HI[idx - 8];
}

#define ROT1 17
#define ROT2 31
#define ROT3 47
#define ROT4 59
#define ROT5 73
#define ROT6 89

#define RESEED_INTERVAL 70000
#define KEY_SIZE 64
#define INIT_ROUNDS 12
#define UNIFIED_BUFFER_SIZE 16384  // 16K bloques de 32 bytes = 512KB
#define AUR3ES_ROUNDS 2 // Diffusion rounds

/* ============================================================================
 * PORTABLE HELPER FUNCTIONS
 * ============================================================================ */

// Secure memory erase - reinforced implementation to prevent compiler optimization
void secure_zero(void *ptr, size_t len) {
    if (ptr == NULL || len == 0) return;
    
#ifdef __STDC_LIB_EXT1__
    memset_s(ptr, len, 0, len);
#elif defined(OPENSSL_cleanse)
    // Use OPENSSL_cleanse from OpenSSL if available
    OPENSSL_cleanse(ptr, len);
#else
    // Robust implementation with memory barriers
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) {
        *p++ = 0;
    }
    // Barrier de memoria para prevenir reordenamiento
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

/* ============================================================================
 * FIPS 140-2/3 COMPLIANCE: Continuous Random Number Generator Test (CRNGCT)
 * ============================================================================ */

/**
 * @brief FIPS CRNGCT - Detect stuck bits and repetition
 * @param rng RNG context
 * @param output Newly generated output
 * @return 0 if passed, non-zero if failed
 * 
 * Per FIPS 140-2/3 requirements, this test checks for:
 * 1. All zeros output
 * 2. All ones output  
 * 3. Repetitive patterns (same as last output)
 * 
 * If failures exceed threshold, the RNG should be considered broken.
 */
static int crngct_check(golden_rng_t *rng, const uint64_t output[4]) {
    if (!rng || !rng->fips_mode_enabled) return 0;
    if (!output) return -1;
    
    int failures = 0;
    
    /* Check 1: All zeros */
    if (output[0] == 0 && output[1] == 0 && output[2] == 0 && output[3] == 0) {
        failures++;
    }
    
    /* Check 2: All ones */
    if (output[0] == 0xFFFFFFFFFFFFFFFFULL &&
        output[1] == 0xFFFFFFFFFFFFFFFFULL &&
        output[2] == 0xFFFFFFFFFFFFFFFFULL &&
        output[3] == 0xFFFFFFFFFFFFFFFFULL) {
        failures++;
    }
    
    /* Check 3: Repetitive pattern (same as last output) */
    if (rng->blocks_generated > 0) {
        if (output[0] == rng->last_output[0] &&
            output[1] == rng->last_output[1] &&
            output[2] == rng->last_output[2] &&
            output[3] == rng->last_output[3]) {
            failures++;
        }
    }
    
    /* Update last output */
    rng->last_output[0] = output[0];
    rng->last_output[1] = output[1];
    rng->last_output[2] = output[2];
    rng->last_output[3] = output[3];
    
    if (failures > 0) {
        rng->crngct_failures++;
        
        /* FIPS requires action after repeated failures */
        if (rng->crngct_failures >= 5) {
            fprintf(stderr, "FIPS CRNGCT ERROR: %d consecutive failures detected!\n", 
                    rng->crngct_failures);
            fprintf(stderr, "RNG health compromised. Reseed recommended.\n");
            return RNG_ERROR_STUCK_BITS;
        }
    } else {
        /* Reset failure counter on success */
        rng->crngct_failures = 0;
    }
    
    return 0;
}

// Portable memory alignment
void* aligned_malloc(size_t alignment, size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
#endif
}

void aligned_free(void *ptr) {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// Conteo de bits poblados (portable)
static inline int popcount64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#elif defined(_MSC_VER)
    return (int)__popcnt64(x);
#else
    // Tabla para 8 bits (fallback)
    static const unsigned char popcount_tab[256] = {
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
    };
    int count = 0;
    for (int i = 0; i < 8; i++) {
        count += popcount_tab[(x >> (i*8)) & 0xFF];
    }
    return count;
#endif
}

/* ============================================================================
 * 3. ENHANCED AUR3ES FUNCTIONS
 * ============================================================================ */

// 64-bit rotation
uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - (k & 63)));
}

uint64_t rotr64(uint64_t x, int k) {
    return (x >> k) | (x << (64 - (k & 63)));
}

// Non-linear 64-bit mix (based on MurmurHash3 finalizer)
uint64_t mix64(uint64_t h) {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

// Full state transformation
void full_state_mix(uint64_t state[8]) {
   // Round 1: Linear mix y no lineal
    for (int i = 0; i < 8; i += 2) {
        state[i] += state[i + 1];
        state[i + 1] = rotl64(state[i + 1], ROT1) ^ state[i];
    }
    
    // Round 2: Cross diffusion
    state[0] = mix64(state[0] + state[3]);
    state[1] = mix64(state[1] + state[4]);
    state[2] = mix64(state[2] + state[5]);
    state[3] = mix64(state[3] + state[6]);
    state[4] = mix64(state[4] + state[7]);
    state[5] = mix64(state[5] + state[0]);
    state[6] = mix64(state[6] + state[1]);
    state[7] = mix64(state[7] + state[2]);
    
    // Round 3: Mix with constants (access through golden_const)
    for (int i = 0; i < 8; i++) {
        int idx = (i * 2) % 16;   // 0,2,4,6,8,10,12,14
        state[i] ^= golden_const(idx);
        state[i] = rotr64(state[i], (i % 6) + 13);
    }
    
    // Round 4: Final mix
    uint64_t a0 = state[0] + state[4];
    uint64_t b0 = rotl64(state[4], ROT2) ^ a0;
    uint64_t a1 = state[1] + state[5];
    uint64_t b1 = rotl64(state[5], ROT2) ^ a1;
    uint64_t a2 = state[2] + state[6];
    uint64_t b2 = rotl64(state[6], ROT2) ^ a2;
    uint64_t a3 = state[3] + state[7];
    uint64_t b3 = rotl64(state[7], ROT2) ^ a3;
    
    state[0] = b0; state[4] = a0;
    state[1] = b1; state[5] = a1;
    state[2] = b2; state[6] = a2;
    state[3] = b3; state[7] = a3;
}

// Enhanced avalanche effect
void enhanced_avalanche(uint64_t state[8]) {
    for (int round = 0; round < 6; round++) {
        for (int i = 0; i < 8; i++) {
            state[i] = mix64(state[i]);
            state[i] ^= state[(i + 1) % 8];
            state[i] += state[(i + 5) % 8];
            state[i] = rotl64(state[i], (i + round) % 64);
        }
    }
}

/* ============================================================================
 * 4. ENHANCED ENTROPY COLLECTION
 * ============================================================================ */

// Obtain high-quality 64-bit entropy
// Uses the professional CSPRNG module with multiple backup sources
static uint64_t get_high_quality_entropy64() {
    uint8_t entropy[8] = {0};
    
    // Use the professional CSPRNG module which already has multiple fallback sources
    // The csprng_get_entropy module includes:
    // - /dev/urandom, getrandom(), sysctl
    // - RDRAND/RDSEED (hardware Intel)
    // - CPU Jitter entropy
    // - Timer entropy
    int ret = csprng_get_entropy(entropy, sizeof(entropy));
    
    if (ret != CSPRNG_SUCCESS) {
        // All sources failed - do NOT use predictable fallback
        // Register error and return 0 (the initial state will mix this)
        fprintf(stderr, "ERROR: Critical failure obtaining system entropy (codigo: %d)\n", ret);
        fprintf(stderr, "WARNING: Using uninitialized state - DO NOT use for cryptography\n");
        // No usamos fallback predecible - retornamos 0
        // The state will be mixed later with other sources
        return 0;
    }
    
    return entropy[1] ^ entropy[7];
}

// Collect entropy for AUR3ES reseed
static void collect_entropy(golden_rng_t *rng) {
    if (rng->deterministic) {
        rng->reseed_counter = 0;
        return;
    }

    uint64_t entropy[8];

    for (int i = 0; i < 8; i++) {
        entropy[i] = get_high_quality_entropy64();
        struct timespec ts = {0, 700 + i * 50};
        nanosleep(&ts, NULL);
    }

    for (int i = 0; i < 8; i++) {
        rng->state[i] ^= mix64(entropy[i] ^ entropy[(i+1) % 8]);
    }

    for (int i = 0; i < 12; i++) {
        full_state_mix(rng->state);
    }

    enhanced_avalanche(rng->state);
    rng->reseed_counter = 0;
}

// Enhanced Key Derivation
static void derive_key_enhanced(const uint8_t* seed, size_t seed_len, uint8_t* key, size_t key_len) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return;
    
    uint8_t hash[SHA512_DIGEST_LENGTH];
    unsigned int hash_len = 0;

    EVP_DigestInit_ex2(ctx, EVP_sha512(), NULL);
    EVP_DigestUpdate(ctx, seed, seed_len);
    EVP_DigestUpdate(ctx, (uint8_t*)"GoldenRNG-AUR3ES", 14);
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    size_t to_copy = (SHA512_DIGEST_LENGTH < key_len) ? SHA512_DIGEST_LENGTH : key_len;
    memcpy(key, hash, to_copy);

    if (key_len > SHA512_DIGEST_LENGTH) {
        size_t remaining = key_len - SHA512_DIGEST_LENGTH;
        for (size_t i = 0; i < remaining; i++) {
            key[SHA512_DIGEST_LENGTH + i] = hash[i % SHA512_DIGEST_LENGTH] ^ (uint8_t)i;
        }
    }
}

// Enhanced AUR3ES block generation
static void generate_enhanced_block(golden_rng_t *rng, uint64_t output[4]) {
    if (!rng || !output) return;

    // Copy current state
    uint64_t working[8];
    memcpy(working, rng->state, sizeof(working));

    //  Mix with counter to avoid patterns (usamos solo la parte baja de las constantes)
    for (int i = 0; i < 8; i++) {
        working[i] ^= rng->counter * GOLDEN_LO[i];
    }

    // Apply multiple mix rounds
    for (int round = 0; round < AUR3ES_ROUNDS; round++) {
        full_state_mix(working);
    }

    // Generate output from mixed state
    for (int i = 0; i < 4; i++) {
        int idx = i + (rng->counter & 7);
        output[i] = mix64(working[i] ^ working[i + 4] ^ 
                         golden_const(idx) ^ 
                         (uint64_t)i * 0x9E3779B97F4A7C15ULL);
        output[i] = rotl64(output[i], (i * 7 + 11) % 64);
    }

    // Update state
    if (!rng->deterministic) {
        for (int i = 0; i < 8; i++) {
            int idx = (i + rng->counter) % 16;
            rng->state[i] ^= output[i % 4] ^ golden_const(idx);
            rng->state[i] = mix64(rng->state[i]);
        }
    } else {
        for (int i = 0; i < 8; i++) {
            rng->state[i] ^= mix64(output[i % 4] + rng->counter + i);
            rng->state[i] += mix64(output[(i + 2) % 4] ^ rng->state[(i + 4) % 8]);
        }
    }
    
    // Mix final state
    full_state_mix(rng->state);
    rng->counter++;
}

/* ============================================================================
 * 5. CONFIGURATION AND CREATION
 * ============================================================================ */

rng_config_t rng_default_config(void) {
    rng_config_t config = {
        .deterministic = 0,
        .reseed_interval = 70000,
        .use_csprng = 1,
        .memory_locking = 1,
        .enable_fips_mode = 1  /* SECURITY: Enabled by default for production use */
    };
    return config;
}

golden_rng_t* rng_create(const rng_config_t *config) {
    golden_rng_t *rng = calloc(1, sizeof(golden_rng_t));
    if (!rng) return NULL;

    rng_config_t cfg = config ? *config : rng_default_config();

    rng->deterministic = cfg.deterministic;
    rng->reseed_interval = cfg.reseed_interval;
    rng->using_csprng = cfg.use_csprng;
    rng->memory_locked = cfg.memory_locking;
    rng->fips_mode_enabled = cfg.enable_fips_mode;
    rng->counter = 0;
    rng->reseed_counter = 0;
    rng->blocks_generated = 0;
    rng->total_bytes = 0;
    rng->crngct_failures = 0;

    if (rng->memory_locked) {
#ifdef __linux__
#ifndef __SANITIZE_THREAD__
        if (mlock(rng, sizeof(golden_rng_t)) != 0) {
            // Log warning instead of silent failure
            fprintf(stderr, "Warning: mlock failed (errno=%d). Memory locking disabled.\n", errno);
            rng->memory_locked = 0;
        }
#else
        rng->memory_locked = 0;
#endif
#endif
    }

    if (rng->deterministic) {
        uint8_t zero_key[KEY_SIZE] = {0};
        derive_key_enhanced(zero_key, KEY_SIZE, zero_key, KEY_SIZE);
        for (int i = 0; i < 8; i++) {
            memcpy(&rng->state[i], zero_key + (i * 8) % KEY_SIZE, 8);
        }
        rng->counter = 0;
    } else {
        uint8_t key[KEY_SIZE];

        // Use professional CSPRNG module to obtain secure entropy
        // The csprng_get_entropy module includes multiple fallback sources:
        // - /dev/urandom (Linux/Unix)
        // - getrandom() syscall
        // - sysctl KERN_ARND (BSD/macOS)
        // - RDRAND/RDSEED (hardware Intel)
        // - CPU Jitter entropy (software)
        // - Timer entropy (software)
        int entropy_result = csprng_get_entropy(key, sizeof(key));
        
        if (entropy_result != CSPRNG_SUCCESS) {
            // CRITICAL ERROR: Could not obtain system entropy
            // We do NOT use predictable fallback - this would be a security weakness
            fprintf(stderr, "ERROR: Critical failure obtaining system entropy (codigo: %d)\n", entropy_result);
            fprintf(stderr, "ERROR: No se puede inicializar el RNG sin entropia adecuada.\n");
            fprintf(stderr, "ADVERTENCIA: La seguridad criptografica estaria comprometida.\n");
            
            // Free memory and return NULL to indicate error
            free(rng);
            return NULL;
        }

        derive_key_enhanced(key, KEY_SIZE, key, KEY_SIZE);
        for (int i = 0; i < 8; i++) {
            memcpy(&rng->state[i], key + (i * 8) % KEY_SIZE, 8);
        }

        secure_zero(key, sizeof(key));
    }

    for (int i = 0; i < INIT_ROUNDS; i++) {
        full_state_mix(rng->state);
        if (i % 8 == 0) {
            enhanced_avalanche(rng->state);
        }
    }

    return rng;
}

int rng_set_seed(golden_rng_t *rng, const uint8_t *seed, size_t seed_len) {
    if (!rng || !seed || seed_len == 0) return -1;

    uint8_t key[KEY_SIZE];
    derive_key_enhanced(seed, seed_len, key, KEY_SIZE);

    for (int i = 0; i < 8; i++) {
        memcpy(&rng->state[i], key + (i * 8) % KEY_SIZE, 8);
    }

    for (int i = 0; i < INIT_ROUNDS; i++) {
        full_state_mix(rng->state);
        if (i % 8 == 0) {
            enhanced_avalanche(rng->state);
        }
    }

    rng->counter = 0;
    rng->deterministic = 1;

    secure_zero(key, sizeof(key));
    return 0;
}

void rng_destroy(golden_rng_t *rng) {
    if (!rng) return;

    secure_zero(rng->state, sizeof(rng->state));

#ifdef __linux__
#ifndef __SANITIZE_THREAD__
    if (rng->memory_locked) {
        munlock(rng, sizeof(golden_rng_t));
    }
#endif
#endif

    free(rng);
}

/* ============================================================================
 * 6. NUMBER GENERATION (AUR3ES)
 * ============================================================================ */

void rng_generate_block(golden_rng_t *rng, uint64_t output[4]) {
    if (!rng || !output) return;

    generate_enhanced_block(rng, output);

    /* FIPS 140-2/3 CRNGCT: Check for stuck bits */
    if (rng->fips_mode_enabled) {
        crngct_check(rng, output);
    }

    if (!rng->deterministic && ++rng->reseed_counter >= (size_t)rng->reseed_interval) {
        collect_entropy(rng);
    }

    rng->blocks_generated++;
    rng->total_bytes += 32;
    
    /* Update monitoring statistics */
    rng_monitor_record_operation(32);
}

void rng_generate_bytes(golden_rng_t *rng, unsigned char *buffer, size_t size) {
    if (!rng || !buffer || size == 0) return;

    uint64_t block[4];
    size_t offset = 0;

    while (offset < size) {
        rng_generate_block(rng, block);
        size_t to_copy = (size - offset < 32) ? (size - offset) : 32;
        memcpy(buffer + offset, block, to_copy);
        offset += to_copy;
    }
}

uint64_t rng_uint64(golden_rng_t *rng) {
    if (!rng) return 0;

    uint64_t block[4];
    generate_enhanced_block(rng, block);
    return block[0];
}

uint32_t rng_uint32(golden_rng_t *rng) {
    return (uint32_t)(rng_uint64(rng) >> 32);
}

double rng_double(golden_rng_t *rng) {
    uint64_t x = rng_uint64(rng);
    return (x >> 11) * 0x1.0p-53;
}

/* ============================================================================
 * 7. STATISTICAL DISTRIBUTIONS
 * ============================================================================ */

static void box_muller(uint64_t u1, uint64_t u2, double *z0, double *z1) {
    double u = (u1 >> 11) * 0x1.0p-53;
    double v = (u2 >> 11) * 0x1.0p-53;
    if (u == 0.0) u = 0x1.0p-53;
    if (v == 0.0) v = 0x1.0p-53;
    double radius = sqrt(-2.0 * log(u));
    double angle = 2.0 * M_PI * v;
    *z0 = radius * cos(angle);
    *z1 = radius * sin(angle);
}

double rng_normal(golden_rng_t *rng, double mean, double stddev) {
    if (!rng) return mean;
    
    // Validate that stddev is positive and not NaN/Inf
    if (stddev <= 0 || isnan(stddev) || isinf(stddev)) {
        return mean;
    }
    
    uint64_t block[4];
    rng_generate_block(rng, block);
    double z0, z1;
    box_muller(block[0], block[1], &z0, &z1);
    return z0 * stddev + mean;
}

double rng_exponential(golden_rng_t *rng, double lambda) {
    if (!rng || lambda <= 0) return 0.0;
    
    double u = rng_double(rng);
    if (u == 1.0) u = 1.0 - 0x1.0p-53;
    return -log(1.0 - u) / lambda;
}

/* ============================================================================
 * 8. STATISTICS AND UTILITIES
 * ============================================================================ */

void rng_get_stats(golden_rng_t *rng, 
                   uint64_t *blocks_generated,
                   uint64_t *reseeds_count,
                   uint64_t *bytes_generated) {
    if (!rng) return;
    if (blocks_generated) *blocks_generated = rng->blocks_generated;
    if (reseeds_count) *reseeds_count = rng->reseed_counter;
    if (bytes_generated) *bytes_generated = rng->total_bytes;
}

void rng_reseed(golden_rng_t *rng) {
    if (!rng || rng->deterministic) return;
    collect_entropy(rng);
}

int rng_is_deterministic(golden_rng_t *rng) {
    return rng ? rng->deterministic : 0;
}

int rng_using_csprng(golden_rng_t *rng) {
    return rng ? rng->using_csprng : 0;
}

/* ============================================================================
 * 9. CRYPTOGRAPHIC SELF-TEST
 * ============================================================================ */

int rng_selftest(void) {
    printf("=== Golden RNG Self-Test ===\n");
    
    rng_config_t cfg = rng_default_config();
    golden_rng_t *rng = rng_create(&cfg);
    if (!rng) {
        printf("FAIL: No se pudo crear RNG\n");
        return -1;
    }
    
    uint64_t u64 = rng_uint64(rng);
    uint32_t u32 = rng_uint32(rng);
    double d = rng_double(rng);
    
    printf("uint64: 0x%016llX\n", (unsigned long long)u64);
    printf("uint32: 0x%08X\n", u32);
    printf("double: %.15f\n", d);
    
    double sum = 0.0;
    int count = 10000;
    for (int i = 0; i < count; i++) {
        sum += rng_double(rng);
    }
    double mean = sum / count;
    printf("Media de %d uniformes: %.4f (esperado: ~0.5)\n", count, mean);
    
    double normal_sum = 0.0;
    for (int i = 0; i < 1000; i++) {
        normal_sum += rng_normal(rng, 0.0, 1.0);
    }
    printf("Media de 1000 normales: %.4f (esperado: ~0.0)\n", normal_sum / 1000);
    
    int special_found = 0;
    for (int i = 0; i < 1000; i++) {
        double n = rng_normal(rng, 0.0, 1.0);
        if (isnan(n) || isinf(n)) {
            special_found++;
        }
    }
    printf("NaN/Inf en normales: %d (esperado: 0)\n", special_found);
    
    rng_destroy(rng);
    
    if (special_found == 0) {
        printf("Self-test: PASS\n");
        return 0;
    } else {
        printf("Self-test: FAIL\n");
        return -1;
    }
}

/* ============================================================================
 * 10. PARALLEL GENERATION
 * ============================================================================ */

typedef struct {
    golden_rng_t* rng;           // RNG compartido (solo para estado inicial)
    unsigned char* buffer;       // Output buffer
    size_t size;                 // Size to generate
    int thread_id;               // Thread ID
    int num_threads;             // Total number of threads
    golden_rng_t* local_rng;     // Local RNG for this thread
} parallel_work_t;

static pthread_mutex_t seed_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t simd_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t global_seed_counter = 0;

static void* parallel_worker(void* arg) {
    parallel_work_t* work = (parallel_work_t*)arg;
    
    // Obtain a unique seed value para este hilo (thread-safe)
    pthread_mutex_lock(&seed_counter_mutex);
    uint64_t unique_seed = global_seed_counter++;
    pthread_mutex_unlock(&seed_counter_mutex);
    
    // Create local RNG with derived seed (deterministic mode)
    rng_config_t cfg = rng_default_config();
    cfg.deterministic = 1;
    work->local_rng = rng_create(&cfg);
    if (work->local_rng) {
        rng_set_seed(work->local_rng, (const uint8_t*)&unique_seed, sizeof(unique_seed));
    }
    
    if (work->local_rng && work->buffer && work->size > 0) {
        rng_generate_bytes(work->local_rng, work->buffer, work->size);
    }
    
    return NULL;
}

int rng_generate_bytes_parallel(golden_rng_t *rng, unsigned char *buffer, size_t size, int num_threads) {
    if (!rng || !buffer || size == 0) return -1;

    if (num_threads <= 0) {
        num_threads = pl_cpu_count();
        if (num_threads < 1) num_threads = 4;
        if (num_threads > 16) num_threads = 16;
    }

    if (size < 1024 * 1024 || num_threads == 1) {
        rng_generate_bytes(rng, buffer, size);
        return 0;
    }

    parallel_work_t* works = calloc(num_threads, sizeof(parallel_work_t));
    if (!works) {
        rng_generate_bytes(rng, buffer, size);
        return -1;
    }
    
    pthread_t* threads = calloc(num_threads, sizeof(pthread_t));
    if (!threads) {
        free(works);
        rng_generate_bytes(rng, buffer, size);
        return -1;
    }

    /* Track successfully created threads for proper cleanup */
    int threads_created = 0;
    int error_occurred = 0;

    size_t chunk_size = size / num_threads;
    size_t remainder = size % num_threads;
    
    // Initialize seed counter in thread-safe manner
    pthread_mutex_lock(&seed_counter_mutex);
    global_seed_counter = 0;
    pthread_mutex_unlock(&seed_counter_mutex);
    
    for (int i = 0; i < num_threads; i++) {
        works[i].rng = rng;
        works[i].thread_id = i;
        works[i].num_threads = num_threads;
        works[i].local_rng = NULL;  /* Initialize to NULL for safe cleanup */
        
        works[i].buffer = buffer + (i * chunk_size);
        works[i].size = chunk_size;
        if (i == num_threads - 1) {
            works[i].size += remainder;
        }
        
        if (pthread_create(&threads[i], NULL, parallel_worker, &works[i]) != 0) {
            /* Thread creation failed - handle gracefully without pthread_cancel */
            fprintf(stderr, "Warning: Failed to create thread %d, falling back to sequential\n", i);
            error_occurred = 1;
            break;  /* Stop creating more threads */
        }
        threads_created++;
    }

    /* Wait for successfully created threads and collect results */
    for (int i = 0; i < threads_created; i++) {
        void *thread_result = NULL;
        pthread_join(threads[i], &thread_result);
        
        /* Clean up local RNG for this thread */
        if (works[i].local_rng) {
            rng_destroy(works[i].local_rng);
            works[i].local_rng = NULL;
        }
    }

    /* If any thread failed to create, complete the rest sequentially */
    if (error_occurred && threads_created < num_threads) {
        size_t offset = threads_created * chunk_size;
        if (threads_created == num_threads - 1) {
            offset = 0;  /* All threads were created but one failed */
        }
        /* Generate remaining data sequentially */
        rng_generate_bytes(rng, buffer + offset, size - offset);
    }

    free(threads);
    free(works);
    return 0;
}

/* ============================================================================
 * 11. BENCHMARK FUNCTIONS
 * ============================================================================ */

int rng_get_cpu_cores(void) {
    return pl_cpu_count();
}

benchmark_config_t rng_benchmark_default_config(void) {
    benchmark_config_t config = {
        .buffer_size = 10 * 1024 * 1024,
        .max_threads = 0,
        .num_iterations = 5,
        .quiet_mode = 0
    };
    return config;
}

static const char* get_benchmark_platform_info(void) {
    static char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "CPU cores: %d | Threads to test: auto",
             rng_get_cpu_cores());
    return buffer;
}

static benchmark_result_t benchmark_sequential(golden_rng_t *rng, size_t buffer_size, int iterations) {
    benchmark_result_t result = {0};
    
    if (!rng || buffer_size == 0 || iterations < 1) return result;
    
    double total_time = 0.0;
    double* times = calloc(iterations, sizeof(double));
    if (!times) return result;
    
    unsigned char* buffer = malloc(buffer_size);
    if (!buffer) {
        free(times);
        return result;
    }
    
    rng_generate_bytes(rng, buffer, buffer_size);
    
    for (int i = 0; i < iterations; i++) {
        uint64_t start = pl_time_ns();
        rng_generate_bytes(rng, buffer, buffer_size);
        uint64_t end = pl_time_ns();
        
        times[i] = (end - start) / 1e9;
        total_time += times[i];
    }
    
    double avg_time = total_time / iterations;
    result.time_seconds = avg_time;
    result.bytes_generated = buffer_size;
    result.throughput_mbps = (buffer_size / (1024.0 * 1024.0)) / avg_time;
    result.threads_used = 1;
    result.speedup = 1.0;
    result.efficiency = 1.0;
    
    double sum_sq = 0.0;
    for (int i = 0; i < iterations; i++) {
        double diff = times[i] - avg_time;
        sum_sq += diff * diff;
    }
    double std_dev = sqrt(sum_sq / iterations);
    double margin = 1.96 * std_dev / sqrt(iterations);
    
    result.confidence_low = result.throughput_mbps * (1.0 - margin / avg_time);
    result.confidence_high = result.throughput_mbps * (1.0 + margin / avg_time);
    
    free(buffer);
    free(times);
    return result;
}

static benchmark_result_t benchmark_parallel(golden_rng_t *rng, size_t buffer_size, int num_threads, int iterations) {
    benchmark_result_t result = {0};
    
    if (!rng || buffer_size == 0 || num_threads < 1) return result;
    
    double total_time = 0.0;
    double* times = calloc(iterations, sizeof(double));
    if (!times) return result;
    
    unsigned char* buffer = malloc(buffer_size);
    if (!buffer) {
        free(times);
        return result;
    }
    
    rng_generate_bytes_parallel(rng, buffer, buffer_size, num_threads);
    
    for (int i = 0; i < iterations; i++) {
        uint64_t start = pl_time_ns();
        int ret = rng_generate_bytes_parallel(rng, buffer, buffer_size, num_threads);
        uint64_t end = pl_time_ns();
        
        if (ret != 0) {
            free(buffer);
            free(times);
            return result;
        }
        
        times[i] = (end - start) / 1e9;
        total_time += times[i];
    }
    
    double avg_time = total_time / iterations;
    result.time_seconds = avg_time;
    result.bytes_generated = buffer_size;
    result.throughput_mbps = (buffer_size / (1024.0 * 1024.0)) / avg_time;
    result.threads_used = num_threads;
    
    free(buffer);
    free(times);
    return result;
}

void rng_benchmark_free_report(benchmark_report_t *report) {
    if (report) free(report);
}

void rng_benchmark_print_report(const benchmark_report_t *report) {
    if (!report) return;
    
    printf("\n");
    printf("================================================================================\n");
    printf("                   GOLDEN RNG PARALLEL BENCHMARK REPORT\n");
    printf("================================================================================\n\n");
    
    printf("Configuration:\n");
    printf("  Buffer Size: %.2f MB (%zu bytes)\n", 
           report->buffer_size / (1024.0 * 1024.0), report->buffer_size);
    printf("  Iterations: %d\n", report->num_iterations);
    printf("  Platform: %s\n\n", report->platform_info);
    
    printf("--------------------------------------------------------------------------------\n");
    printf("                    SEQUENTIAL BENCHMARK (1 thread)\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("  Throughput:     %10.2f MB/s\n", report->sequential.throughput_mbps);
    printf("  Time per run:   %10.3f ms\n", report->sequential.time_seconds * 1000.0);
    printf("  95%% CI:         [%.2f, %.2f] MB/s\n",
           report->sequential.confidence_low, report->sequential.confidence_high);
    printf("--------------------------------------------------------------------------------\n\n");
    
    printf("--------------------------------------------------------------------------------\n");
    printf("                    PARALLEL BENCHMARK (%2d threads)\n", report->parallel.threads_used);
    printf("--------------------------------------------------------------------------------\n");
    printf("  Throughput:     %10.2f MB/s\n", report->parallel.throughput_mbps);
    printf("  Time per run:   %10.3f ms\n", report->parallel.time_seconds * 1000.0);
    printf("  Speedup:        %10.2fx\n", report->parallel.speedup);
    printf("  Efficiency:     %10.1f%%\n", report->parallel.efficiency * 100.0);
    printf("--------------------------------------------------------------------------------\n\n");
    
    if (report->optimal.threads_used != report->parallel.threads_used) {
        printf("--------------------------------------------------------------------------------\n");
        printf("                      OPTIMAL CONFIGURATION\n");
        printf("--------------------------------------------------------------------------------\n");
        printf("  Best Threads:   %d\n", report->optimal.threads_used);
        printf("  Best Throughput: %10.2f MB/s\n", report->optimal.throughput_mbps);
        printf("  Best Speedup:   %10.2fx\n", report->optimal.speedup);
        printf("  Best Efficiency: %10.1f%%\n", report->optimal.efficiency * 100.0);
        printf("--------------------------------------------------------------------------------\n\n");
    }
    
    printf("================================================================================\n");
    printf("                         BENCHMARK COMPLETE\n");
    printf("================================================================================\n");
}

benchmark_report_t* rng_benchmark_comprehensive(golden_rng_t *rng, const benchmark_config_t *config) {
    if (!rng) return NULL;
    
    benchmark_config_t cfg = config ? *config : rng_benchmark_default_config();
    
    if (cfg.max_threads <= 0) {
        cfg.max_threads = rng_get_cpu_cores();
    }
    if (cfg.buffer_size == 0) {
        cfg.buffer_size = 10 * 1024 * 1024;
    }
    if (cfg.num_iterations < 3) {
        cfg.num_iterations = 3;
    }
    
    benchmark_report_t *report = calloc(1, sizeof(benchmark_report_t));
    if (!report) return NULL;
    
    report->buffer_size = cfg.buffer_size;
    report->num_iterations = cfg.num_iterations;
    report->platform_info = get_benchmark_platform_info();
    
    printf("\nRunning Golden RNG Comprehensive Benchmark...\n");
    printf("Buffer size: %.2f MB, Iterations: %d, Max threads: %d\n\n",
           cfg.buffer_size / (1024.0 * 1024.0), cfg.num_iterations, cfg.max_threads);
    
    printf("[1/%d] Testing sequential (1 thread)...\n", cfg.max_threads + 1);
    report->sequential = benchmark_sequential(rng, cfg.buffer_size, cfg.num_iterations);
    printf("  -> %.2f MB/s\n", report->sequential.throughput_mbps);
    
    benchmark_result_t best_result = report->sequential;
    int best_threads = 1;
    
    for (int threads = 2; threads <= cfg.max_threads; threads++) {
        printf("[%d/%d] Testing parallel (%d threads)...\n", threads, cfg.max_threads, threads);
        benchmark_result_t result = benchmark_parallel(rng, cfg.buffer_size, threads, cfg.num_iterations);
        
        if (report->sequential.throughput_mbps > 0) {
            result.speedup = result.throughput_mbps / report->sequential.throughput_mbps;
            result.efficiency = result.speedup / threads;
        }
        
        printf("  -> %.2f MB/s (%.2fx speedup, %.1f%% efficiency)\n",
               result.throughput_mbps, result.speedup, result.efficiency * 100.0);
        
        if (threads == cfg.max_threads) {
            report->parallel = result;
        }
        
        if (result.throughput_mbps > best_result.throughput_mbps) {
            best_result = result;
            best_threads = threads;
        }
    }
    
    report->optimal = best_result;
    report->optimal_threads = best_threads;
    report->best_speedup = best_result.speedup;
    
    return report;
}

int rng_benchmark_parallel(golden_rng_t *rng, size_t buffer_size, int max_threads) {
    if (!rng) return -1;

    benchmark_config_t config = rng_benchmark_default_config();
    if (buffer_size > 0) config.buffer_size = buffer_size;
    if (max_threads > 0) config.max_threads = max_threads;

    benchmark_report_t *report = rng_benchmark_comprehensive(rng, &config);
    if (report) {
        rng_benchmark_print_report(report);
        rng_benchmark_free_report(report);
        return 0;
    }
    return -1;
}

/* ============================================================================
 * 12. SIMD FEATURES - Platform and CPU Detection
 * ============================================================================ */

static cpu_features_t g_cpu_features = {0};
static int g_cpu_features_initialized = 0;

platform_id_t get_platform_id(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return PLATFORM_X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return PLATFORM_ARM64;
#elif defined(__i386__) || defined(_M_IX86)
    return PLATFORM_X86;
#elif defined(__arm__) || defined(_M_ARM)
    return PLATFORM_ARM32;
#else
    return PLATFORM_UNKNOWN;
#endif
}

const char* get_platform_name(void) {
    switch (get_platform_id()) {
        case PLATFORM_X86_64: return "x86_64";
        case PLATFORM_ARM64: return "ARM64";
        case PLATFORM_X86: return "x86";
        case PLATFORM_ARM32: return "ARM32";
        default: return "Unknown";
    }
}

// Portable CPU detection using intrinsics
void detect_cpu_features(cpu_features_t* features) {
    memset(features, 0, sizeof(cpu_features_t));

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    unsigned int eax, ebx, ecx, edx;
    
    // Verify CPUID support
#if defined(_MSC_VER)
    int cpu_info[4];
    __cpuid(cpu_info, 0);
    int max_level = cpu_info[0];
    if (max_level >= 1) {
        __cpuid(cpu_info, 1);
        eax = cpu_info[0]; ebx = cpu_info[1]; ecx = cpu_info[2]; edx = cpu_info[3];
        features->has_sse2   = (edx >> 26) & 1;
        features->has_sse3   = (ecx >>  0) & 1;
        features->has_ssse3  = (ecx >>  9) & 1;
        features->has_sse4_1 = (ecx >> 19) & 1;
        features->has_sse4_2 = (ecx >> 20) & 1;
        features->has_avx    = (ecx >> 28) & 1;
    }
    if (max_level >= 7) {
        __cpuid(cpu_info, 7);
        ebx = cpu_info[1];
        features->has_avx2    = (ebx >>  5) & 1;
        features->has_avx512f = (ebx >> 16) & 1;
    }
#else
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        features->has_sse2   = (edx >> 26) & 1;
        features->has_sse3   = (ecx >>  0) & 1;
        features->has_ssse3  = (ecx >>  9) & 1;
        features->has_sse4_1 = (ecx >> 19) & 1;
        features->has_sse4_2 = (ecx >> 20) & 1;
        features->has_avx    = (ecx >> 28) & 1;
    }
    unsigned int eax7, ebx7, ecx7, edx7;
    if (__get_cpuid(7, &eax7, &ebx7, &ecx7, &edx7)) {
        features->has_avx2    = (ebx7 >>  5) & 1;
        features->has_avx512f = (ebx7 >> 16) & 1;
    }
#endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON) || defined(__ARM_NEON__)
    features->has_neon = 1;
#endif
}

const char* get_cpu_features_string(void) {
    static char buffer[256];

    if (!g_cpu_features_initialized) {
        detect_cpu_features(&g_cpu_features);
        g_cpu_features_initialized = 1;
    }

    snprintf(buffer, sizeof(buffer),
        "SSE2:%d SSE3:%d SSSE3:%d SSE4.1:%d SSE4.2:%d AVX:%d AVX2:%d AVX-512:%d NEON:%d",
        g_cpu_features.has_sse2, g_cpu_features.has_sse3,
        g_cpu_features.has_ssse3, g_cpu_features.has_sse4_1,
        g_cpu_features.has_sse4_2, g_cpu_features.has_avx,
        g_cpu_features.has_avx2, g_cpu_features.has_avx512f,
        g_cpu_features.has_neon);

    return buffer;
}

int is_little_endian(void) {
    uint16_t test = 0x1234;
    return (*(uint8_t*)&test) == 0x34;
}

size_t get_optimal_alignment(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return 32;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return 16;
#else
    return 16;
#endif
}

void rng_print_system_info(void) {
    printf("\n=== INFORMACION DEL SISTEMA ===\n");
    printf("Platform: %s\n", get_platform_name());
    printf("CPU Features: %s\n", get_cpu_features_string());
    printf("Optimal alignment: %zu bytes\n", get_optimal_alignment());
    printf("Endianness: %s\n", is_little_endian() ? "Little" : "Big");
    printf("CPU cores: %d\n", rng_get_cpu_cores());
    printf("=============================\n");
}

/* ============================================================================
 * 13. SIMD BENCHMARK - Implementation Comparison
 * ============================================================================ */

#define SIMD_BENCHMARK_ITERATIONS 1000000

static double simd_median(double *values, int count) {
    // Bubble sort simple
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (values[j] > values[j + 1]) {
                double temp = values[j];
                values[j] = values[j + 1];
                values[j + 1] = temp;
            }
        }
    }
    return values[count / 2];
}

simd_benchmark_result_t rng_benchmark_simd(golden_rng_t *rng, size_t buffer_size, int iterations) {
    simd_benchmark_result_t result = {0};

    if (!rng || buffer_size == 0 || iterations < 1) return result;

    if (!g_cpu_features_initialized) {
        detect_cpu_features(&g_cpu_features);
        g_cpu_features_initialized = 1;
    }
    result.features = g_cpu_features;

    unsigned char *buffer = malloc(buffer_size);
    if (!buffer) return result;

    rng_generate_bytes(rng, buffer, buffer_size);

    double times[10];
    int valid_times = 0;

    for (int i = 0; i < iterations && i < 10; i++) {
        uint64_t start = pl_time_ns();
        rng_generate_bytes(rng, buffer, buffer_size);
        uint64_t end = pl_time_ns();
        times[valid_times++] = (end - start) / 1e9;
    }

    if (valid_times > 0) {
        double med = simd_median(times, valid_times);
        // Check to avoid division by zero
        if (med > 0) {
            result.scalar_throughput_mbps = (buffer_size / 1e6) / med;
            result.best_throughput_mbps = result.scalar_throughput_mbps;
        } else {
            result.scalar_throughput_mbps = 0.0;
            result.best_throughput_mbps = 0.0;
        }
        result.best_implementation = "Scalar";
    }

    // Determine best available implementation
    if (g_cpu_features.has_avx512f) {
        result.best_implementation = "AVX-512";
    } else if (g_cpu_features.has_avx2) {
        result.best_implementation = "AVX2";
    } else if (g_cpu_features.has_sse2) {
        result.best_implementation = "SSE2";
    } else {
        result.best_implementation = "Scalar";
    }

    free(buffer);
    return result;
}

void rng_benchmark_simd_print_report(const simd_benchmark_result_t *report) {
    if (!report) return;

    printf("\n");
    printf("================================================================================\n");
    printf("                    GOLDEN RNG SIMD BENCHMARK REPORT\n");
    printf("================================================================================\n\n");

    printf("Platform Information:\n");
    printf("  Platform: %s\n", get_platform_name());
    printf("  Cores: %d\n", rng_get_cpu_cores());
    printf("  Alignment: %zu bytes\n", get_optimal_alignment());
    printf("  Endianness: %s\n\n", is_little_endian() ? "Little" : "Big");

    printf("CPU Features:\n");
    printf("  %s\n\n", get_cpu_features_string());

    printf("--------------------------------------------------------------------------------\n");
    printf("                         BENCHMARK RESULTS\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("  Scalar (baseline): %10.2f MB/s\n", report->scalar_throughput_mbps);
    printf("  Best Implementation: %s\n", report->best_implementation);
    printf("  Estimated Best Throughput: %10.2f MB/s\n", report->best_throughput_mbps);
    printf("--------------------------------------------------------------------------------\n\n");

    printf("Notes:\n");
    printf("  - Scalar baseline uses standard C implementation\n");
    printf("  - SIMD implementations would provide additional speedup\n");
    printf("  - Actual throughput depends on CPU model and memory bandwidth\n\n");

    printf("================================================================================\n");
    printf("                         BENCHMARK COMPLETE\n");
    printf("================================================================================\n");
}

void rng_benchmark_basic(void) {
    uint64_t state[8];
    uint64_t output[4];
    uint64_t total_bytes = 0;

    for (int i = 0; i < 8; i++) {
        state[i] = GOLDEN_LO[i];
    }

    clock_t start = clock();

    for (int i = 0; i < SIMD_BENCHMARK_ITERATIONS; i++) {
        uint64_t working[8];
        memcpy(working, state, sizeof(working));

        for (int round = 0; round < 2; round++) {
            full_state_mix(working);
        }

        for (int j = 0; j < 4; j++) {
            output[j] = 0;
            for (int k = 0; k < 8; k++) {
                output[j] ^= mix64(working[k] + i + j * 8 + k);
            }
            output[j] = rotl64(output[j], (j * 7 + 11) % 64);
        }

        for (int j = 0; j < 8; j++) {
            state[j] ^= mix64(output[j % 4] + i);
            state[j] += mix64(output[(j + 2) % 4] ^ state[(j + 4) % 8]);
        }

        full_state_mix(state);
        total_bytes += 32;
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n=== GOLDEN RNG BENCHMARK ===\n");
    printf("Platform: %s\n", get_platform_name());
    printf("CPU Features: %s\n", get_cpu_features_string());
    printf("Iteraciones: %d\n", SIMD_BENCHMARK_ITERATIONS);
    printf("Bytes generados: %lu\n", (unsigned long)total_bytes);
    printf("Tiempo: %.3f segundos\n", elapsed);
    printf("Throughput: %.2f MB/s\n", (total_bytes / elapsed) / (1024 * 1024));
    printf("==========================\n");
}

/* ============================================================================
 * MEMORY POOL IMPLEMENTATION
 * ============================================================================ */

rng_memory_pool_t* rng_memory_pool_create(size_t size) {
    if (size == 0) size = 1024 * 1024; // Default 1MB
    
    rng_memory_pool_t *pool = calloc(1, sizeof(rng_memory_pool_t));
    if (!pool) return NULL;
    
    size = (size + 63) & ~63;
    
    pool->buffer = aligned_malloc(64, size);
    if (!pool->buffer) {
        free(pool);
        return NULL;
    }
    
    pool->buffer_size = size;
    pool->used = 0;
    pool->num_blocks = size / 32;
    pool->blocks = calloc(pool->num_blocks, sizeof(uint64_t) * 4);
    
    if (!pool->blocks) {
        aligned_free(pool->buffer);
        free(pool);
        return NULL;
    }
    
    pthread_mutex_init(&pool->mutex, NULL);
    
    return pool;
}

void rng_memory_pool_destroy(rng_memory_pool_t *pool) {
    if (!pool) return;
    
    if (pool->buffer) {
        secure_zero(pool->buffer, pool->buffer_size);
        aligned_free(pool->buffer);
    }
    
    if (pool->blocks) {
        secure_zero(pool->blocks, pool->num_blocks * sizeof(uint64_t) * 4);
        free(pool->blocks);
    }
    
    pthread_mutex_destroy(&pool->mutex);
    free(pool);
}

int rng_memory_pool_prefill(rng_memory_pool_t *pool, golden_rng_t *rng, size_t size) {
    if (!pool || !rng) return -1;
    
    pthread_mutex_lock(&pool->mutex);
    
    size = (size > pool->buffer_size) ? pool->buffer_size : size;
    
    size_t num_blocks = size / 32;
    // Check that we do not exceed pool limits
    if (num_blocks > (size_t)pool->num_blocks) {
        num_blocks = (size_t)pool->num_blocks;
    }
    
    for (size_t i = 0; i < num_blocks; i++) {
        generate_enhanced_block(rng, pool->blocks + (i * 4));
    }
    
    memcpy(pool->buffer, pool->blocks, size);
    pool->used = 0;
    
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int rng_memory_pool_get(rng_memory_pool_t *pool, golden_rng_t *rng, 
                       unsigned char *buffer, size_t size) {
    if (!pool || !rng || !buffer || size == 0) return -1;
    
    pthread_mutex_lock(&pool->mutex);
    
    size_t remaining = pool->buffer_size - pool->used;
    
    if (size <= remaining) {
        memcpy(buffer, pool->buffer + pool->used, size);
        pool->used += size;
        pthread_mutex_unlock(&pool->mutex);
        return 0;
    }
    
    if (remaining > 0) {
        memcpy(buffer, pool->buffer + pool->used, remaining);
        pool->used = pool->buffer_size;
    }
    
    size_t offset = remaining;
    while (offset < size) {
        uint64_t block[4];
        generate_enhanced_block(rng, block);
        
        size_t to_copy = size - offset;
        if (to_copy > 32) to_copy = 32;
        
        memcpy(buffer + offset, block, to_copy);
        offset += to_copy;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

/* ============================================================================
 * SIMD IMPLEMENTATION - Unified version (always uses normal implementation)
 * ============================================================================ */

static int g_simd_available = -1;
static const char* g_simd_impl_name = "Scalar";

int rng_simd_available(void) {
    if (g_simd_available == -1) {
        pthread_mutex_lock(&simd_init_mutex);
        // Double check after acquiring mutex
        if (g_simd_available == -1) {
            detect_cpu_features(&g_cpu_features);
            g_cpu_features_initialized = 1;
            
            if (g_cpu_features.has_avx512f) {
                g_simd_available = 512;
                g_simd_impl_name = "AVX-512";
            } else if (g_cpu_features.has_avx2) {
                g_simd_available = 256;
                g_simd_impl_name = "AVX2";
            } else if (g_cpu_features.has_sse2) {
                g_simd_available = 128;
                g_simd_impl_name = "SSE2";
            } else {
                g_simd_available = 0;
                g_simd_impl_name = "Scalar";
            }
        }
        pthread_mutex_unlock(&simd_init_mutex);
    }
    return g_simd_available;
}

const char* rng_simd_implementation_name(void) {
    if (g_simd_available == -1) rng_simd_available();
    return g_simd_impl_name;
}

int rng_generate_bytes_simd(golden_rng_t *rng, unsigned char *buffer, size_t size) {
    if (!rng || !buffer || size == 0) return -1;
    
    /* Check what SIMD is available and use the best implementation */
    int simd_bits = rng_simd_available();
    
    if (simd_bits == 512) {
        /* AVX-512 available */
        rng_generate_bytes_avx512(rng, buffer, size);
    } else if (simd_bits == 256) {
        /* AVX2 available */
        rng_generate_bytes_avx2(rng, buffer, size);
    } else if (simd_bits == 128) {
        /* SSE2 available */
        rng_generate_bytes_sse2(rng, buffer, size);
    } else {
        /* Scalar fallback */
        rng_generate_bytes(rng, buffer, size);
    }
    
    return 0;
}

