/*
 * ============================================================================
 * GOLDEN RNG - LOGGING IMPLEMENTATION
 * ============================================================================
 */

#include "rng_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

/* ============================================================================
 * STATIC VARIABLES
 * ============================================================================ */

static rng_log_config_t g_log_config = {
    .level = RNG_LOG_INFO,
    .output = RNG_LOG_OUTPUT_STDERR,
    .use_colors = true,
    .use_timestamps = true,
    .filename = {0},
    .max_file_size = 10 * 1024 * 1024,  /* 10 MB */
    .max_files = 5
};

static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_log_initialized = 0;

/* ============================================================================
 * COLOR CODES
 * ============================================================================ */

static const char* g_level_colors[] = {
    "\033[31m",  /* ERROR - Red */
    "\033[33m",  /* WARN - Yellow */
    "\033[32m",  /* INFO - Green */
    "\033[36m",  /* DEBUG - Cyan */
    "\033[90m"   /* TRACE - Gray */
};

static const char* g_color_reset = "\033[0m";

static const char* g_level_names[] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

static const char* get_timestamp(void) {
    static char buffer[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

static void open_log_file(void) {
    if (g_log_file) {
        fclose(g_log_file);
    }
    g_log_file = fopen(g_log_config.filename, "a");
}

static void rotate_log_files(void) {
    if (!g_log_config.filename[0]) return;

    char old_name[300];
    char new_name[300];

    /* Remove oldest file */
    snprintf(old_name, sizeof(old_name), "%s.%d",
             g_log_config.filename, g_log_config.max_files);
    remove(old_name);

    /* Rotate existing files */
    for (int i = g_log_config.max_files - 1; i >= 1; i--) {
        snprintf(old_name, sizeof(old_name), "%s.%d", g_log_config.filename, i);
        snprintf(new_name, sizeof(new_name), "%s.%d", g_log_config.filename, i + 1);
        rename(old_name, new_name);
    }

    /* Rename current to .1 */
    if (g_log_config.filename[0]) {
        snprintf(new_name, sizeof(new_name), "%s.1", g_log_config.filename);
        rename(g_log_config.filename, new_name);
    }
}

static void check_file_rotation(void) {
    if (!g_log_file || g_log_config.output != RNG_LOG_OUTPUT_FILE) return;

    long size = ftell(g_log_file);
    if (size >= (long)g_log_config.max_file_size && g_log_config.max_file_size > 0) {
        fclose(g_log_file);
        g_log_file = NULL;
        rotate_log_files();
        open_log_file();
    }
}

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================ */

void rng_log_init(const rng_log_config_t *config) {
    pthread_mutex_lock(&g_log_mutex);

    if (config) {
        g_log_config = *config;
    }

    /* Open file if configured */
    if (g_log_config.output == RNG_LOG_OUTPUT_FILE && g_log_config.filename[0]) {
        open_log_file();
    }

    g_log_initialized = 1;
    pthread_mutex_unlock(&g_log_mutex);
}

void rng_log_set_level(rng_log_level_t level) {
    pthread_mutex_lock(&g_log_mutex);
    g_log_config.level = level;
    pthread_mutex_unlock(&g_log_mutex);
}

void rng_log_set_output(rng_log_output_t output, const char *filename) {
    pthread_mutex_lock(&g_log_mutex);

    g_log_config.output = output;

    if (filename) {
        strncpy(g_log_config.filename, filename, sizeof(g_log_config.filename) - 1);
    }

    if (output == RNG_LOG_OUTPUT_FILE && filename[0]) {
        if (g_log_file) {
            fclose(g_log_file);
        }
        open_log_file();
    } else if (output != RNG_LOG_OUTPUT_FILE && g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    pthread_mutex_unlock(&g_log_mutex);
}

void rng_log(rng_log_level_t level, const char *format, ...) {
    if (!g_log_initialized) {
        /* Auto-initialize with defaults */
        rng_log_init(NULL);
    }

    /* Check level */
    if (level > g_log_config.level) {
        return;
    }

    pthread_mutex_lock(&g_log_mutex);

    FILE *output = stdout;
    if (g_log_config.output == RNG_LOG_OUTPUT_STDERR) {
        output = stderr;
    } else if (g_log_config.output == RNG_LOG_OUTPUT_FILE) {
        check_file_rotation();
        output = g_log_file;
        if (!output) {
            pthread_mutex_unlock(&g_log_mutex);
            return;
        }
    }

    /* Build message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* Write to output */
    if (g_log_config.use_colors && g_log_config.output != RNG_LOG_OUTPUT_FILE) {
        fprintf(output, "%s[%s]%s %s: %s\n",
                g_level_colors[level],
                get_timestamp(),
                g_color_reset,
                g_level_names[level],
                message);
    } else if (g_log_config.use_timestamps) {
        fprintf(output, "[%s] %s: %s\n",
                get_timestamp(),
                g_level_names[level],
                message);
    } else {
        fprintf(output, "%s: %s\n",
                g_level_names[level],
                message);
    }

    fflush(output);
    pthread_mutex_unlock(&g_log_mutex);
}

void rng_log_flush(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_file) {
        fflush(g_log_file);
    }
    pthread_mutex_unlock(&g_log_mutex);
}

void rng_log_shutdown(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    g_log_initialized = 0;
    pthread_mutex_unlock(&g_log_mutex);
}

