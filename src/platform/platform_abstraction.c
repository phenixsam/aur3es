/*
 * ============================================================================
 * PLATFORM ABSTRACTION LAYER (PAL) - IMPLEMENTATION
 * ============================================================================
 */

#include "platform_abstraction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <psapi.h>
    #include <time.h>
#else
    #include <unistd.h>
    #include <sys/time.h>
    #include <signal.h>
    #include <errno.h>
    #ifdef PLATFORM_MACOS
        #include <mach/mach.h>
        #include <mach/mach_time.h>
        #include <sys/sysctl.h>
    #else
        #include <sys/sysinfo.h>
        #ifndef CLOCK_MONOTONIC
            #define CLOCK_MONOTONIC 1
        #endif
    #endif
#endif

static volatile int g_shutdown_requested = 0;

/* ============================================================================
 * THREADING
 * ============================================================================ */

int pl_thread_create(pl_thread_t *thread, void* (*func)(void*), void *arg) {
    if (!thread || !func) return -1;
#ifdef PLATFORM_WINDOWS
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return (*thread != NULL) ? 0 : -1;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

int pl_thread_join(pl_thread_t thread, void **retval) {
#ifdef PLATFORM_WINDOWS
    WaitForSingleObject(thread, INFINITE);
    DWORD exit_code = 0;
    if (retval) {
        GetExitCodeThread(thread, &exit_code);
        *retval = (void*)(intptr_t)exit_code;
    }
    CloseHandle(thread);
    return 0;
#else
    return pthread_join(thread, retval);
#endif
}

uint64_t pl_thread_current_id(void) {
#ifdef PLATFORM_WINDOWS
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

void pl_thread_sleep(unsigned int ms) {
#ifdef PLATFORM_WINDOWS
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* ============================================================================
 * MUTEX
 * ============================================================================ */

void pl_mutex_init(pl_mutex_t *mutex) {
    if (!mutex) return;
#ifdef PLATFORM_WINDOWS
    InitializeCriticalSection(mutex);
#else
    pthread_mutex_init(mutex, NULL);
#endif
}

void pl_mutex_lock(pl_mutex_t *mutex) {
    if (!mutex) return;
#ifdef PLATFORM_WINDOWS
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

void pl_mutex_unlock(pl_mutex_t *mutex) {
    if (!mutex) return;
#ifdef PLATFORM_WINDOWS
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

void pl_mutex_destroy(pl_mutex_t *mutex) {
    if (!mutex) return;
#ifdef PLATFORM_WINDOWS
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

/* ============================================================================
 * TIME
 * ============================================================================ */

uint64_t pl_time_ns(void) {
#ifdef PLATFORM_WINDOWS
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter;
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#elif defined(PLATFORM_MACOS)
    static mach_timebase_info_data_t info = {0};
    if (info.denom == 0) {
        mach_timebase_info(&info);
    }
    uint64_t time = mach_absolute_time();
    return time * info.numer / info.denom;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t pl_time_ms(void) {
#ifdef PLATFORM_WINDOWS
    return (uint64_t)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000;
#endif
}

void pl_datetime(char *buffer, size_t size) {
    if (!buffer || size == 0) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* ============================================================================
 * SIGNALS
 * ============================================================================ */

#ifdef PLATFORM_WINDOWS
static BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_shutdown_requested = 1;
        return TRUE;
    }
    return FALSE;
}
#else
static void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_shutdown_requested = 1;
    }
}
#endif

void pl_setup_signals(void) {
#ifdef PLATFORM_WINDOWS
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#ifndef PLATFORM_MACOS
    signal(SIGPIPE, SIG_IGN);
#endif
#endif
}

int pl_shutdown_requested(void) {
    return g_shutdown_requested;
}

/* ============================================================================
 * UTILITIES
 * ============================================================================ */

int pl_cpu_count(void) {
#ifdef PLATFORM_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#elif defined(PLATFORM_MACOS)
    int nm[2];
    size_t len = 2;
    int count = 0;
    nm[0] = CTL_HW;
    nm[1] = HW_NCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);
    return count > 0 ? count : 1;
#else
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    return (nproc > 0) ? (int)nproc : 1;
#endif
}

void pl_memory_usage(size_t *rss, size_t *virt) {
    if (!rss || !virt) return;
    *rss = 0;
    *virt = 0;
#ifdef PLATFORM_WINDOWS
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        *rss = pmc.WorkingSetSize;
        *virt = pmc.PagefileUsage;
    }
#elif defined(PLATFORM_MACOS)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &count) == KERN_SUCCESS) {
        *rss = info.resident_size;
        *virt = info.virtual_size;
    }
#else
    FILE *statm = fopen("/proc/self/statm", "r");
    if (statm) {
        size_t pages = (size_t)sysconf(_SC_PAGESIZE);
        if (fscanf(statm, "%zu %zu", virt, rss) == 2) {
            *rss *= pages;
            *virt *= pages;
        }
        fclose(statm);
    }
#endif
}

void pl_system_info(char *buffer, size_t size) {
    if (!buffer || size == 0) return;
    snprintf(buffer, size,
             "Platform: %s (%s), CPUs: %d, Time: %llu ms",
             PLATFORM_NAME,
             PLATFORM_ARCH,
             pl_cpu_count(),
             (unsigned long long)pl_time_ms());
}
