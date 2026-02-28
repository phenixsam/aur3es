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

/**
 * @file csprng_entropy.c
 * @brief CSPRNG Entropy Collector - Professional Implementation
 * @version 2.0.0
 * 
 * No external crypto dependencies - uses built-in primitives:
 * - ChaCha20-based mixing function
 * - SHA-256-like compression for entropy pooling
 * - Hardware-accelerated entropy when available
 */

#include "csprng_entropy.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #define CSPRNG_PLATFORM_WINDOWS 1
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#else
    #define CSPRNG_PLATFORM_UNIX 1
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <time.h>
    
    #if defined(__linux__)
        #include <sys/random.h>
        #include <sys/syscall.h>
        #ifdef SYS_getrandom
            #define CSPRNG_HAS_GETRANDOM 1
        #endif
    #endif
    
    #if defined(__APPLE__) || defined(__BSD__)
        #include <sys/sysctl.h>
        #define CSPRNG_HAS_SYSCTL_ARND 1
    #endif
#endif

/* ============================================================================
 * CPU Feature Detection
 * ============================================================================ */

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define CSPRNG_ARCH_X86 1
    #include <cpuid.h>
    
    static bool has_rdrand(void) {
        uint32_t eax, ebx, ecx, edx;
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            return (ecx & (1 << 30)) != 0;  /* RDRAND bit */
        }
        return false;
    }
    
    static bool has_rdseed(void) {
        uint32_t eax, ebx, ecx, edx;
        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            return (ebx & (1 << 18)) != 0;  /* RDSEED bit */
        }
        return false;
    }
    
    static inline uint64_t rdtsc(void) {
        uint32_t lo, hi;
        __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    }
    
    static bool get_rdrand(uint64_t *value) {
        unsigned char ok;
        uint64_t rnd;
        __asm__ __volatile__ (
            "rdrand %0; setc %1"
            : "=r"(rnd), "=qm"(ok)
        );
        if (ok) {
            *value = rnd;
            return true;
        }
        return false;
    }
    
    static bool get_rdseed(uint64_t *value) {
        unsigned char ok;
        uint64_t rnd;
        __asm__ __volatile__ (
            "rdseed %0; setc %1"
            : "=r"(rnd), "=qm"(ok)
        );
        if (ok) {
            *value = rnd;
            return true;
        }
        return false;
    }

#elif defined(__aarch64__) || defined(_M_ARM64)
    #define CSPRNG_ARCH_ARM64 1
    
    static inline uint64_t read_cntvct(void) {
        uint64_t val;
        __asm__ __volatile__ ("mrs %0, cntvct_el0" : "=r"(val));
        return val;
    }
    
    static inline uint64_t read_cntfrq(void) {
        uint64_t val;
        __asm__ __volatile__ ("mrs %0, cntfrq_el0" : "=r"(val));
        return val;
    }
#endif

/* ============================================================================
 * Constant-Time Operations (Security)
 * ============================================================================ */

static inline void secure_zero(void *ptr, size_t len) {
    if (!ptr) return;
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len--) {
        *p++ = 0;
    }
}

static inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static inline uint64_t rotl64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

static inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

/* ============================================================================
 * Built-in Hash Functions (No External Dependencies)
 * ============================================================================ */

/**
 * @brief SHA-256-like compression function for entropy pooling
 * Based on FIPS 180-4 constants but simplified for entropy mixing
 */
static void entropy_compress(uint32_t state[8], const uint32_t block[16]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    
    /* Prepare message schedule */
    for (int i = 0; i < 16; i++) {
        W[i] = block[i];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(W[i-15], 7) ^ rotr32(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = rotr32(W[i-2], 17) ^ rotr32(W[i-2], 19) ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }
    
    /* Initialize working variables */
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    /* Main loop */
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        T1 = h + S1 + ch + K[i] + W[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        T2 = S0 + maj;
        
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }
    
    /* Compute intermediate hash value */
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    
    /* Secure cleanup */
    secure_zero(W, sizeof(W));
}

/**
 * @brief Simple entropy pool hash (256-bit output)
 */
static void entropy_hash(const uint8_t *input, size_t len, uint8_t output[32]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    /* Process input in 64-byte blocks */
    uint8_t buffer[128] = {0};
    size_t processed = 0;
    
    while (processed < len) {
        size_t block_len = (len - processed) > 64 ? 64 : (len - processed);
        memcpy(buffer, input + processed, block_len);
        
        /* Pad last block */
        if (block_len < 64) {
            buffer[block_len] = 0x80;
            uint64_t bit_len = len * 8;
            for (int i = 0; i < 8; i++) {
                buffer[63 - i] = (bit_len >> (i * 8)) & 0xff;
            }
        }
        
        /* Convert to big-endian words and compress */
        uint32_t block[16];
        for (int i = 0; i < 16; i++) {
            block[i] = ((uint32_t)buffer[i*4] << 24) |
                       ((uint32_t)buffer[i*4+1] << 16) |
                       ((uint32_t)buffer[i*4+2] << 8) |
                       ((uint32_t)buffer[i*4+3]);
        }
        
        entropy_compress(state, block);
        processed += block_len;
        
        if (block_len < 64) break;
    }
    
    /* Output state as bytes (big-endian) */
    for (int i = 0; i < 8; i++) {
        output[i*4]   = (state[i] >> 24) & 0xff;
        output[i*4+1] = (state[i] >> 16) & 0xff;
        output[i*4+2] = (state[i] >> 8) & 0xff;
        output[i*4+3] = state[i] & 0xff;
    }
    
    secure_zero(buffer, sizeof(buffer));
    secure_zero(state, sizeof(state));
}

/* ============================================================================
 * ChaCha20 Core (for PRNG)
 * ============================================================================ */

#define CHACHA_ROTL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))
#define CHACHA_QUARTER(a, b, c, d) \
    a += b; d ^= a; d = CHACHA_ROTL(d, 16); \
    c += d; b ^= c; b = CHACHA_ROTL(b, 12); \
    a += b; d ^= a; d = CHACHA_ROTL(d, 8); \
    c += d; b ^= c; b = CHACHA_ROTL(b, 7)

static void chacha20_block(uint32_t out[16], const uint32_t key[8], 
                           uint32_t counter, const uint32_t nonce[3]) {
    uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,  /* "expand 32-byte k" */
        key[0], key[1], key[2], key[3],
        key[4], key[5], key[6], key[7],
        counter, nonce[0], nonce[1], nonce[2]
    };
    
    uint32_t working[16];
    memcpy(working, state, sizeof(working));
    
    /* 20 rounds (10 double-rounds) */
    for (int i = 0; i < 10; i++) {
        /* Column round */
        CHACHA_QUARTER(working[0], working[4], working[8],  working[12]);
        CHACHA_QUARTER(working[1], working[5], working[9],  working[13]);
        CHACHA_QUARTER(working[2], working[6], working[10], working[14]);
        CHACHA_QUARTER(working[3], working[7], working[11], working[15]);
        /* Diagonal round */
        CHACHA_QUARTER(working[0], working[5], working[10], working[15]);
        CHACHA_QUARTER(working[1], working[6], working[11], working[12]);
        CHACHA_QUARTER(working[2], working[7], working[8],  working[13]);
        CHACHA_QUARTER(working[3], working[4], working[9],  working[14]);
    }
    
    /* Add original state */
    for (int i = 0; i < 16; i++) {
        out[i] = working[i] + state[i];
    }
    
    secure_zero(working, sizeof(working));
}

#undef CHACHA_ROTL
#undef CHACHA_QUARTER

/* ============================================================================
 * Entropy Sources Implementation
 * ============================================================================ */

#ifdef CSPRNG_PLATFORM_WINDOWS

/**
 * @brief Windows: Use BCrypt (or RtlGenRandom as fallback)
 */
static int entropy_from_windows(uint8_t *buffer, size_t length) {
    NTSTATUS status = BCryptGenRandom(
        NULL,
        buffer,
        (ULONG)length,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    
    if (status == 0) {  /* STATUS_SUCCESS */
        return CSPRNG_SUCCESS;
    }
    
    /* Fallback to RtlGenRandom */
    typedef BOOLEAN (WINAPI *RtlGenRandomFunc)(PVOID, ULONG);
    HMODULE hAdvApi32 = LoadLibraryA("advapi32.dll");
    if (hAdvApi32) {
        RtlGenRandomFunc pRtlGenRandom = 
            (RtlGenRandomFunc)GetProcAddress(hAdvApi32, "SystemFunction036");
        if (pRtlGenRandom && pRtlGenRandom(buffer, (ULONG)length)) {
            FreeLibrary(hAdvApi32);
            return CSPRNG_SUCCESS;
        }
        FreeLibrary(hAdvApi32);
    }
    
    return CSPRNG_ERROR_SOURCE;
}

#else  /* Unix-like systems */

/**
 * @brief Linux: Use getrandom() syscall
 */
#ifdef CSPRNG_HAS_GETRANDOM
static int entropy_from_getrandom(uint8_t *buffer, size_t length) {
    ssize_t ret;
    size_t total = 0;
    
    while (total < length) {
        /* getrandom() may return less than requested */
        ret = syscall(SYS_getrandom, buffer + total, length - total, 0);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return CSPRNG_ERROR_SOURCE;
        }
        if (ret == 0) {
            return CSPRNG_ERROR_INSUFFICIENT;
        }
        total += (size_t)ret;
    }
    
    return CSPRNG_SUCCESS;
}
#endif

/**
 * @brief Unix: Read from /dev/urandom
 */
static int entropy_from_urandom(uint8_t *buffer, size_t length) {
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return CSPRNG_ERROR_SOURCE;
    }
    
    size_t total = 0;
    while (total < length) {
        ssize_t n = read(fd, buffer + total, length - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return CSPRNG_ERROR_SOURCE;
        }
        if (n == 0) {
            close(fd);
            return CSPRNG_ERROR_INSUFFICIENT;
        }
        total += (size_t)n;
    }
    
    close(fd);
    return CSPRNG_SUCCESS;
}

/**
 * @brief BSD/macOS: Use sysctl KERN_ARND
 */
#ifdef CSPRNG_HAS_SYSCTL_ARND
static int entropy_from_sysctl(uint8_t *buffer, size_t length) {
#if defined(__APPLE__)
    /* macOS uses entropysysctl */
    int mib[2] = { CTL_KERN, KERN_ARND };
#else
    /* BSD uses KERN_ARND with sysctl */
    int mib[2] = { CTL_KERN, KERN_ARND };
#endif
    size_t len = length;
    
    if (sysctl(mib, 2, buffer, &len, NULL, 0) == 0 && len == length) {
        return CSPRNG_SUCCESS;
    }
    
    return CSPRNG_ERROR_SOURCE;
}
#endif

#endif /* Platform-specific entropy */

/**
 * @brief Hardware RNG: Intel RDRAND
 */
#ifdef CSPRNG_ARCH_X86
static int entropy_from_rdrand(uint8_t *buffer, size_t length) {
    if (!has_rdrand()) {
        return CSPRNG_ERROR_SOURCE;
    }
    
    size_t total = 0;
    int retry_count = 0;
    
    while (total < length && retry_count < 100) {
        uint64_t rnd;
        if (get_rdrand(&rnd)) {
            size_t copy_len = (length - total) > 8 ? 8 : (length - total);
            memcpy(buffer + total, &rnd, copy_len);
            total += copy_len;
            retry_count = 0;
        } else {
            retry_count++;
        }
    }
    
    if (total == length) {
        return CSPRNG_SUCCESS;
    }
    
    return CSPRNG_ERROR_SOURCE;
}

/**
 * @brief Hardware RNG: Intel RDSEED (higher quality)
 */
static int entropy_from_rdseed(uint8_t *buffer, size_t length) {
    if (!has_rdseed()) {
        return CSPRNG_ERROR_SOURCE;
    }
    
    size_t total = 0;
    int retry_count = 0;
    
    while (total < length && retry_count < 100) {
        uint64_t rnd;
        if (get_rdseed(&rnd)) {
            size_t copy_len = (length - total) > 8 ? 8 : (length - total);
            memcpy(buffer + total, &rnd, copy_len);
            total += copy_len;
            retry_count = 0;
        } else {
            retry_count++;
        }
    }
    
    if (total == length) {
        return CSPRNG_SUCCESS;
    }
    
    return CSPRNG_ERROR_SOURCE;
}
#endif

/**
 * @brief CPU Jitter entropy (timing variations)
 * Based on timing variations in CPU execution
 */
static int entropy_from_jitter(uint8_t *buffer, size_t length) {
    uint64_t samples[64];
    size_t sample_count = 64;
    
#if defined(CSPRNG_ARCH_X86)
    /* x86/x64: Use RDTSC for high-precision timing */
    for (size_t i = 0; i < sample_count; i++) {
        uint64_t t1 = rdtsc();
        
        /* Memory barrier and variable work */
        __asm__ __volatile__ ("" ::: "memory");
        volatile int dummy = 0;
        for (volatile int j = 0; j < (int)(100 + (i * 7)); j++) {
            dummy = dummy * 3 + j;  /* Variable work */
        }
        __asm__ __volatile__ ("" ::: "memory");
        
        uint64_t t2 = rdtsc();
        samples[i] = t2 - t1;
        
        /* Mix in CPUID for additional jitter */
        uint32_t eax, ebx, ecx, edx;
        __cpuid(0, eax, ebx, ecx, edx);
        samples[i] ^= ((uint64_t)eax << 32) | ebx;
    }
    
#elif defined(CSPRNG_ARCH_ARM64)
    /* ARM64: Use CNTPCT for timing */
    for (size_t i = 0; i < sample_count; i++) {
        uint64_t t1 = read_cntvct();
        
        volatile int dummy = 0;
        for (volatile int j = 0; j < (int)(100 + (i * 7)); j++) {
            dummy = dummy * 3 + j;
        }
        
        uint64_t t2 = read_cntvct();
        samples[i] = t2 - t1;
    }
    
#else
    /* Generic: Use clock_gettime */
    struct timespec ts1, ts2;
    for (size_t i = 0; i < sample_count; i++) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts1);
        
        volatile int dummy = 0;
        for (volatile int j = 0; j < (int)(100 + (i * 7)); j++) {
            dummy = dummy * 3 + j;
        }
        
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts2);
        
        samples[i] = ((uint64_t)(ts2.tv_sec - ts1.tv_sec) * 1000000000ULL) +
                     ((uint64_t)(ts2.tv_nsec - ts1.tv_nsec));
    }
#endif
    
    /* Hash all samples to extract entropy */
    entropy_hash((const uint8_t *)samples, sizeof(samples), buffer);
    
    /* Extend output if needed */
    if (length > 32) {
        uint64_t counter = 0;
        size_t generated = 32;
        
        while (generated < length) {
            uint8_t more[32];
            uint8_t input[40];
            memcpy(input, buffer, 32);
            memcpy(input + 32, &counter, 8);
            
            entropy_hash(input, 40, more);
            
            size_t copy_len = (length - generated) > 32 ? 32 : (length - generated);
            memcpy(buffer + generated, more, copy_len);
            generated += copy_len;
            counter++;
        }
    }
    
    secure_zero(samples, sizeof(samples));
    return CSPRNG_SUCCESS;
}

/**
 * @brief High-resolution timer entropy
 */
static int entropy_from_timer(uint8_t *buffer, size_t length) {
    uint64_t timers[32];
    
#if defined(CSPRNG_ARCH_X86)
    timers[0] = rdtsc();
#elif defined(CSPRNG_ARCH_ARM64)
    timers[0] = read_cntvct();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    timers[0] = ((uint64_t)ts.tv_sec << 20) ^ ts.tv_nsec;
#endif
    
    /* Collect multiple timer samples with varying delays */
    for (int i = 1; i < 32; i++) {
        /* Small delay to get different values */
        for (volatile int j = 0; j < i * 3; j++) { }
        
#if defined(CSPRNG_ARCH_X86)
        timers[i] = rdtsc();
#elif defined(CSPRNG_ARCH_ARM64)
        timers[i] = read_cntvct();
#else
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        timers[i] = ((uint64_t)ts.tv_sec << 20) ^ ts.tv_nsec;
#endif
        timers[i] ^= (uint64_t)i * 0x9e3779b97f4a7c15ULL;
    }
    
    /* Mix in real-time clock */
    time_t now = time(NULL);
    timers[31] ^= (uint64_t)now;
    
    /* Hash to extract entropy */
    entropy_hash((const uint8_t *)timers, sizeof(timers), buffer);
    
    /* Extend if needed */
    if (length > 32) {
        uint8_t temp[64];
        memcpy(temp, buffer, 32);
        memset(temp + 32, 0x5a, 32);
        entropy_hash(temp, 64, buffer + 32);
        if (length > 64) {
            entropy_hash(buffer, 64, buffer + 64);
        }
    }
    
    secure_zero(timers, sizeof(timers));
    return CSPRNG_SUCCESS;
}

/* ============================================================================
 * Health Tests (NIST SP 800-90B)
 * ============================================================================ */

/**
 * @brief Repetition Count Test
 * Detects stuck-at faults by checking for consecutive identical values
 */
static bool health_repetition_test(uint64_t value, uint64_t *last_value, int *count) {
    if (value == *last_value) {
        (*count)++;
        if (*count >= 5) {  /* C = 5 for conservative check */
            return false;  /* Health test failed */
        }
    } else {
        *last_value = value;
        *count = 1;
    }
    return true;
}

/**
 * @brief Adaptive Proportion Test
 * Detects bias in entropy source output
 */
static bool health_adaptive_proportion_test(csprng_ctx_t *ctx, uint64_t value) {
    /* Store sample in circular buffer */
    ctx->health_samples[ctx->health_index] = value;
    
    /* Count occurrences of current value in window */
    int count = 0;
    for (size_t i = 0; i < CSPRNG_HEALTH_WINDOW; i++) {
        if (ctx->health_samples[i] == value) {
            count++;
        }
    }
    
    /* Update index */
    ctx->health_index = (ctx->health_index + 1) % CSPRNG_HEALTH_WINDOW;
    
    /* Check threshold (for 64-bit values, conservative threshold) */
    if (count > 10) {  /* Unlikely for true random */
        return false;
    }
    
    return true;
}

/* ============================================================================
 * Core API Implementation
 * ============================================================================ */

const char* csprng_strerror(int error_code) {
    switch (error_code) {
        case CSPRNG_SUCCESS:           return "Success";
        case CSPRNG_ERROR_NULL:        return "Null pointer";
        case CSPRNG_ERROR_LENGTH:      return "Invalid length";
        case CSPRNG_ERROR_SOURCE:      return "Entropy source error";
        case CSPRNG_ERROR_HEALTH:      return "Health test failed";
        case CSPRNG_ERROR_INSUFFICIENT: return "Insufficient entropy";
        default:                       return "Unknown error";
    }
}

const char* csprng_version(void) {
    static char version[16];
    snprintf(version, sizeof(version), "%d.%d.%d", 
             CSPRNG_VERSION_MAJOR, CSPRNG_VERSION_MINOR, CSPRNG_VERSION_PATCH);
    return version;
}

int csprng_init(csprng_ctx_t *ctx) {
    if (!ctx) return CSPRNG_ERROR_NULL;
    
    /* Zero the context */
    secure_zero(ctx, sizeof(csprng_ctx_t));
    
    /* Initialize source list */
    ctx->source_count = 0;
    
#ifdef CSPRNG_PLATFORM_WINDOWS
    /* Windows: BCrypt/RtlGenRandom is always available */
    ctx->sources[ctx->source_count++] = (csprng_source_info_t){
        .name = "BCrypt/RtlGenRandom",
        .flag = CSPRNG_SOURCE_WIN32_CRYPTO,
        .available = true,
        .healthy = true
    };
    ctx->sources_available |= CSPRNG_SOURCE_WIN32_CRYPTO;
    ctx->sources_healthy |= CSPRNG_SOURCE_WIN32_CRYPTO;
    
else  /* Unix-like */

#ifdef CSPRNG_HAS_GETRANDOM
    /* Test getrandom availability */
    {
        uint8_t test_getrandom[1];
        if (entropy_from_getrandom(test_getrandom, 1) == CSPRNG_SUCCESS) {
            ctx->sources[ctx->source_count++] = (csprng_source_info_t){
                .name = "getrandom()",
                .flag = CSPRNG_SOURCE_GETRANDOM,
                .available = true,
                .healthy = true
            };
            ctx->sources_available |= CSPRNG_SOURCE_GETRANDOM;
            ctx->sources_healthy |= CSPRNG_SOURCE_GETRANDOM;
        }
    }
#endif

    /* Test /dev/urandom */
    {
        uint8_t test_urandom[1];
        if (entropy_from_urandom(test_urandom, 1) == CSPRNG_SUCCESS) {
        ctx->sources[ctx->source_count++] = (csprng_source_info_t){
            .name = "/dev/urandom",
            .flag = CSPRNG_SOURCE_URANDOM,
            .available = true,
            .healthy = true
        };
        ctx->sources_available |= CSPRNG_SOURCE_URANDOM;
        ctx->sources_healthy |= CSPRNG_SOURCE_URANDOM;
    }

#ifdef CSPRNG_HAS_SYSCTL_ARND
    if (entropy_from_sysctl(test, 1) == CSPRNG_SUCCESS) {
        ctx->sources[ctx->source_count++] = (csprng_source_info_t){
            .name = "sysctl KERN_ARND",
            .flag = CSPRNG_SOURCE_SYSCTL,
            .available = true,
            .healthy = true
        };
        ctx->sources_available |= CSPRNG_SOURCE_SYSCTL;
        ctx->sources_healthy |= CSPRNG_SOURCE_SYSCTL;
    }
#endif

#endif  /* Platform */

#ifdef CSPRNG_ARCH_X86
    /* Test RDRAND */
    if (has_rdrand()) {
        uint8_t test[8];
        bool works = (entropy_from_rdrand(test, 8) == CSPRNG_SUCCESS);
        ctx->sources[ctx->source_count++] = (csprng_source_info_t){
            .name = "RDRAND",
            .flag = CSPRNG_SOURCE_RDRAND,
            .available = works,
            .healthy = works
        };
        if (works) {
            ctx->sources_available |= CSPRNG_SOURCE_RDRAND;
            ctx->sources_healthy |= CSPRNG_SOURCE_RDRAND;
        }
    }
    
    /* Test RDSEED */
    if (has_rdseed()) {
        uint8_t test[8];
        bool works = (entropy_from_rdseed(test, 8) == CSPRNG_SUCCESS);
        ctx->sources[ctx->source_count++] = (csprng_source_info_t){
            .name = "RDSEED",
            .flag = CSPRNG_SOURCE_RDSEED,
            .available = works,
            .healthy = works
        };
        if (works) {
            ctx->sources_available |= CSPRNG_SOURCE_RDSEED;
            ctx->sources_healthy |= CSPRNG_SOURCE_RDSEED;
        }
    }
#endif

    /* Jitter entropy is always available */
    ctx->sources[ctx->source_count++] = (csprng_source_info_t){
        .name = "CPU Jitter",
        .flag = CSPRNG_SOURCE_JITTER,
        .available = true,
        .healthy = true
    };
    ctx->sources_available |= CSPRNG_SOURCE_JITTER;
    ctx->sources_healthy |= CSPRNG_SOURCE_JITTER;
    
    /* Timer entropy is always available */
    ctx->sources[ctx->source_count++] = (csprng_source_info_t){
        .name = "Timer",
        .flag = CSPRNG_SOURCE_TIMER,
        .available = true,
        .healthy = true
    };
    ctx->sources_available |= CSPRNG_SOURCE_TIMER;
    ctx->sources_healthy |= CSPRNG_SOURCE_TIMER;
    
    /* Initialize internal state with entropy */
    if (csprng_reseed(ctx) != CSPRNG_SUCCESS) {
        return CSPRNG_ERROR_SOURCE;
    }
    
    ctx->initialized = true;
    return CSPRNG_SUCCESS;
}

void csprng_cleanup(csprng_ctx_t *ctx) {
    if (ctx) {
        secure_zero(ctx, sizeof(csprng_ctx_t));
    }
}

int csprng_reseed(csprng_ctx_t *ctx) {
    if (!ctx) return CSPRNG_ERROR_NULL;
    
    uint8_t entropy[128];
    int result = csprng_get_entropy(entropy, sizeof(entropy));
    
    if (result == CSPRNG_SUCCESS) {
        /* Mix entropy into state */
        entropy_hash(entropy, sizeof(entropy), ctx->state);
        ctx->counter = 0;
        
        /* Mix counter into second half of state */
        uint8_t temp[64];
        memcpy(temp, ctx->state, 32);
        memset(temp + 32, 0, 32);
        entropy_hash(temp, 64, ctx->state + 32);
    }
    
    secure_zero(entropy, sizeof(entropy));
    return result;
}

int csprng_get_entropy(uint8_t *buffer, size_t length) {
    if (!buffer || length == 0) return CSPRNG_ERROR_NULL;
    
    uint8_t temp[64];
    size_t generated = 0;
    
    /* Try each entropy source in order of quality */
    
#ifdef CSPRNG_PLATFORM_WINDOWS
    
    /* Windows: Use BCrypt */
    if (entropy_from_windows(buffer, length) == CSPRNG_SUCCESS) {
        return CSPRNG_SUCCESS;
    }
    
#else  /* Unix-like */

#ifdef CSPRNG_HAS_GETRANDOM
    /* Linux: getrandom() is preferred */
    if (entropy_from_getrandom(buffer, length) == CSPRNG_SUCCESS) {
        return CSPRNG_SUCCESS;
    }
#endif

#ifdef CSPRNG_HAS_SYSCTL_ARND
    /* BSD/macOS: sysctl KERN_ARND */
    if (entropy_from_sysctl(buffer, length) == CSPRNG_SUCCESS) {
        return CSPRNG_SUCCESS;
    }
#endif

    /* Unix fallback: /dev/urandom */
    if (entropy_from_urandom(buffer, length) == CSPRNG_SUCCESS) {
        return CSPRNG_SUCCESS;
    }

#endif  /* Platform */

#ifdef CSPRNG_ARCH_X86
    /* Try RDSEED first (highest quality hardware RNG) */
    if ((generated == 0) && 
        (entropy_from_rdseed(buffer, length) == CSPRNG_SUCCESS)) {
        return CSPRNG_SUCCESS;
    }
    
    /* Try RDRAND */
    if ((generated == 0) && 
        (entropy_from_rdrand(buffer, length) == CSPRNG_SUCCESS)) {
        return CSPRNG_SUCCESS;
    }
#endif
    
    /* Fallback: Combine jitter + timer entropy */
    uint8_t jitter[64];
    uint8_t timer[64];
    uint8_t combined[128];
    
    if (entropy_from_jitter(jitter, 64) == CSPRNG_SUCCESS &&
        entropy_from_timer(timer, 64) == CSPRNG_SUCCESS) {
        
        /* Combine both sources */
        memcpy(combined, jitter, 64);
        memcpy(combined + 64, timer, 64);
        
        /* Hash to extract entropy */
        entropy_hash(combined, 128, temp);
        
        /* XOR with original jitter for additional mixing */
        for (int i = 0; i < 32; i++) {
            temp[i] ^= jitter[i] ^ timer[i];
        }
        
        /* Second round for more output */
        if (length > 32) {
            entropy_hash(temp, 32, temp + 32);
        }
        
        /* Copy to output */
        size_t copy_len = length > 64 ? 64 : length;
        memcpy(buffer, temp, copy_len);
        
        /* Extend if needed */
        if (length > 64) {
            uint64_t counter = 1;
            size_t off = 64;
            while (off < length) {
                memcpy(combined, temp, 64);
                memcpy(combined + 64, &counter, 8);
                entropy_hash(combined, 72, temp);
                
                copy_len = (length - off) > 32 ? 32 : (length - off);
                memcpy(buffer + off, temp, copy_len);
                off += copy_len;
                counter++;
            }
        }
        
        secure_zero(jitter, sizeof(jitter));
        secure_zero(timer, sizeof(timer));
        secure_zero(combined, sizeof(combined));
        secure_zero(temp, sizeof(temp));
        
        return CSPRNG_SUCCESS;
    }
    
    secure_zero(temp, sizeof(temp));
    return CSPRNG_ERROR_INSUFFICIENT;
}

int csprng_add_entropy(csprng_ctx_t *ctx, const uint8_t *data, 
                       size_t length, size_t estimated_bits) {
    if (!ctx || !data) return CSPRNG_ERROR_NULL;
    if (length == 0) return CSPRNG_ERROR_LENGTH;
    
    (void)estimated_bits;  /* Not used in this simple implementation */
    
    /* Mix new entropy into state */
    uint8_t temp[96];
    memcpy(temp, ctx->state, 64);
    memcpy(temp + 64, data, length > 32 ? 32 : length);
    
    entropy_hash(temp, 64 + (length > 32 ? 32 : length), ctx->state);
    
    secure_zero(temp, sizeof(temp));
    return CSPRNG_SUCCESS;
}

int csprng_get_bytes(csprng_ctx_t *ctx, uint8_t *buffer, size_t length) {
    if (!ctx || !buffer) return CSPRNG_ERROR_NULL;
    if (length == 0) return CSPRNG_ERROR_LENGTH;
    if (!ctx->initialized) return CSPRNG_ERROR_SOURCE;
    
    /* Use ChaCha20 as PRNG */
    uint32_t key[8];
    uint32_t nonce[3] = {0};
    uint32_t block[16];
    
    /* Derive key from state */
    for (int i = 0; i < 8; i++) {
        key[i] = ((uint32_t)ctx->state[i*4] << 24) |
                 ((uint32_t)ctx->state[i*4+1] << 16) |
                 ((uint32_t)ctx->state[i*4+2] << 8) |
                 ((uint32_t)ctx->state[i*4+3]);
    }
    for (int i = 0; i < 3; i++) {
        nonce[i] = ((uint32_t)ctx->state[32 + i*4] << 24) |
                   ((uint32_t)ctx->state[32 + i*4+1] << 16) |
                   ((uint32_t)ctx->state[32 + i*4+2] << 8) |
                   ((uint32_t)ctx->state[32 + i*4+3]);
    }
    
    /* Generate output */
    size_t generated = 0;
    uint64_t counter = ctx->counter;
    
    while (generated < length) {
        chacha20_block(block, key, (uint32_t)(counter & 0xFFFFFFFF), nonce);
        
        /* Convert block to bytes */
        uint8_t block_bytes[64];
        for (int i = 0; i < 16; i++) {
            block_bytes[i*4]   = (block[i] >> 24) & 0xff;
            block_bytes[i*4+1] = (block[i] >> 16) & 0xff;
            block_bytes[i*4+2] = (block[i] >> 8) & 0xff;
            block_bytes[i*4+3] = block[i] & 0xff;
        }
        
        /* Copy to output */
        size_t copy_len = (length - generated) > 64 ? 64 : (length - generated);
        memcpy(buffer + generated, block_bytes, copy_len);
        
        generated += copy_len;
        counter++;
        
        secure_zero(block_bytes, sizeof(block_bytes));
    }
    
    /* Update state counter */
    ctx->counter = counter;
    
    /* Periodically reseed (every 1MB or so) */
    if (ctx->counter > 16384) {  /* 16384 * 64 = 1MB */
        csprng_reseed(ctx);
    }
    
    secure_zero(key, sizeof(key));
    secure_zero(nonce, sizeof(nonce));
    secure_zero(block, sizeof(block));
    
    return CSPRNG_SUCCESS;
}

csprng_health_status_t csprng_health_check(csprng_ctx_t *ctx) {
    if (!ctx) return CSPRNG_HEALTH_FAIL;
    
    /* Check if we have at least one healthy source */
    if (ctx->sources_healthy == 0) {
        return CSPRNG_HEALTH_FAIL;
    }
    
    /* Check error rates */
    uint32_t total_errors = 0;
    for (size_t i = 0; i < ctx->source_count; i++) {
        total_errors += ctx->sources[i].error_count;
    }
    
    /* Too many errors */
    if (total_errors > 100) {
        return CSPRNG_HEALTH_FAIL;
    }
    
    /* Degraded if only software sources available */
    uint32_t hw_sources = CSPRNG_SOURCE_RDRAND | CSPRNG_SOURCE_RDSEED |
                          CSPRNG_SOURCE_WIN32_CRYPTO;
    if ((ctx->sources_healthy & hw_sources) == 0) {
        /* No hardware sources, but check for OS sources */
        uint32_t os_sources = CSPRNG_SOURCE_GETRANDOM | CSPRNG_SOURCE_URANDOM |
                              CSPRNG_SOURCE_SYSCTL;
        if ((ctx->sources_healthy & os_sources) == 0) {
            return CSPRNG_HEALTH_DEGRADED;
        }
    }
    
    return CSPRNG_HEALTH_PASS;
}

uint32_t csprng_get_sources(csprng_ctx_t *ctx) {
    return ctx ? ctx->sources_available : 0;
}

const csprng_source_info_t* csprng_get_source_info(
    csprng_ctx_t *ctx, csprng_source_flags_t source) {
    
    if (!ctx) return NULL;
    
    for (size_t i = 0; i < ctx->source_count; i++) {
        if (ctx->sources[i].flag == source) {
            return &ctx->sources[i];
        }
    }
    
    return NULL;
}

bool csprng_self_test(void) {
    csprng_ctx_t ctx;
    
    /* Test initialization */
    if (csprng_init(&ctx) != CSPRNG_SUCCESS) {
        return false;
    }
    
    /* Test entropy collection */
    uint8_t entropy[64];
    if (csprng_get_entropy(entropy, sizeof(entropy)) != CSPRNG_SUCCESS) {
        csprng_cleanup(&ctx);
        return false;
    }
    
    /* Test that entropy is not all zeros */
    bool all_zero = true;
    for (int i = 0; i < 64; i++) {
        if (entropy[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        csprng_cleanup(&ctx);
        return false;
    }
    
    /* Test PRNG generation */
    uint8_t output[256];
    if (csprng_get_bytes(&ctx, output, sizeof(output)) != CSPRNG_SUCCESS) {
        csprng_cleanup(&ctx);
        return false;
    }
    
    /* Test that output is not all zeros */
    all_zero = true;
    for (int i = 0; i < 256; i++) {
        if (output[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        csprng_cleanup(&ctx);
        return false;
    }
    
    /* Test reseed */
    if (csprng_reseed(&ctx) != CSPRNG_SUCCESS) {
        csprng_cleanup(&ctx);
        return false;
    }
    
    /* Test health check */
    if (csprng_health_check(&ctx) == CSPRNG_HEALTH_FAIL) {
        csprng_cleanup(&ctx);
        return false;
    }
    
    csprng_cleanup(&ctx);
    return true;
}

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

/**
 * @brief Thread-local context for simple API
 */
#ifdef _MSC_VER
    static __declspec(thread) csprng_ctx_t tls_ctx = {0};
    static __declspec(thread) bool tls_initialized = false;
#else
    static __thread csprng_ctx_t tls_ctx = {0};
    static __thread bool tls_initialized = false;
#endif

/**
 * @brief Simple API: Get random bytes using thread-local context
 */
int get_system_entropy(uint8_t *buffer, size_t length) {
    if (!buffer || length == 0) return CSPRNG_ERROR_NULL;
    
    if (!tls_initialized) {
        if (csprng_init(&tls_ctx) != CSPRNG_SUCCESS) {
            return CSPRNG_ERROR_SOURCE;
        }
        tls_initialized = true;
    }
    
    return csprng_get_bytes(&tls_ctx, buffer, length);
}

/**
 * @brief Simple API: Get entropy directly (bypasses PRNG)
 */
int get_system_entropy_direct(uint8_t *buffer, size_t length) {
    return csprng_get_entropy(buffer, length);
}

/**
 * @brief Get 32-bit random number
 */
uint32_t csprng_random32(csprng_ctx_t *ctx) {
    uint32_t value;
    if (csprng_get_bytes(ctx, (uint8_t *)&value, sizeof(value)) == CSPRNG_SUCCESS) {
        return value;
    }
    return 0;
}

/**
 * @brief Get 64-bit random number
 */
uint64_t csprng_random64(csprng_ctx_t *ctx) {
    uint64_t value;
    if (csprng_get_bytes(ctx, (uint8_t *)&value, sizeof(value)) == CSPRNG_SUCCESS) {
        return value;
    }
    return 0;
}

/**
 * @brief Get random number in range [0, max)
 */
uint64_t csprng_random_range(csprng_ctx_t *ctx, uint64_t max) {
    if (max == 0) return 0;
    
    /* Avoid modulo bias */
    uint64_t threshold = -max % max;
    uint64_t value;
    
    do {
        value = csprng_random64(ctx);
    } while (value < threshold);
    
    return value % max;
}

/**
 * @brief Fill buffer with random bytes (simple API)
 */
void csprng_fill_random(uint8_t *buffer, size_t length) {
    get_system_entropy(buffer, length);
}
