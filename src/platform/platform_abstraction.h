/*
 * ============================================================================
 * PLATFORM ABSTRACTION LAYER (PAL) - HEADER
 * ============================================================================
 * Abstracción multiplataforma para threading, mutex, tiempo y señales
 * ============================================================================
 */

#ifndef PLATFORM_ABSTRACTION_H
#define PLATFORM_ABSTRACTION_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * 1. DETECCIÓN DE PLATAFORMA
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_NAME "Windows"
    #if defined(_M_X64) || defined(__x86_64__)
        #define PLATFORM_ARCH "x64"
    #elif defined(_M_IX86) || defined(__i386__)
        #define PLATFORM_ARCH "x86"
    #elif defined(_M_ARM64) || defined(__aarch64__)
        #define PLATFORM_ARCH "arm64"
    #else
        #define PLATFORM_ARCH "unknown"
    #endif
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "Linux"
    #if defined(__x86_64__)
        #define PLATFORM_ARCH "x64"
    #elif defined(__i386__)
        #define PLATFORM_ARCH "x86"
    #elif defined(__aarch64__)
        #define PLATFORM_ARCH "arm64"
    #else
        #define PLATFORM_ARCH "unknown"
    #endif
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
    #define PLATFORM_NAME "macOS"
    #if defined(__x86_64__)
        #define PLATFORM_ARCH "x64"
    #elif defined(__aarch64__) || defined(__arm64__)
        #define PLATFORM_ARCH "arm64"
    #else
        #define PLATFORM_ARCH "unknown"
    #endif
#else
    #error "Plataforma no soportada"
#endif

/* ============================================================================
 * 2. TIPOS CONCRETOS PARA PLATAFORMA
 * ============================================================================ */

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    typedef HANDLE pl_thread_t;
    typedef CRITICAL_SECTION pl_mutex_t;
#else
    #include <pthread.h>
    typedef pthread_t pl_thread_t;
    typedef pthread_mutex_t pl_mutex_t;
#endif

/* ============================================================================
 * 3. DECLARACIONES
 * ============================================================================ */

// Threading
int pl_thread_create(pl_thread_t *thread, void* (*func)(void*), void *arg);
int pl_thread_join(pl_thread_t thread, void **retval);
uint64_t pl_thread_current_id(void);
void pl_thread_sleep(unsigned int ms);

// Mutex
void pl_mutex_init(pl_mutex_t *mutex);
void pl_mutex_lock(pl_mutex_t *mutex);
void pl_mutex_unlock(pl_mutex_t *mutex);
void pl_mutex_destroy(pl_mutex_t *mutex);

// Tiempo
uint64_t pl_time_ns(void);
uint64_t pl_time_ms(void);
void pl_datetime(char *buffer, size_t size);

// Señales
void pl_setup_signals(void);
int pl_shutdown_requested(void);

// Utilidades
int pl_cpu_count(void);
void pl_memory_usage(size_t *rss, size_t *virt);
void pl_system_info(char *buffer, size_t size);
#endif /* PLATFORM_ABSTRACTION_H */
