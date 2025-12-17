#define _POSIX_C_SOURCE 200809L // For localtime_r
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static LogLevel g_console_level = LOG_LEVEL_INFO;
static LogLevel g_file_level = LOG_LEVEL_TRACE;

static FILE* g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_initialized = false;

static const char* level_strings[] = {
    "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"
};

static const char* level_colors[] = {
    "\033[90m", // TRACE - Gray
    "\033[36m", // DEBUG - Cyan
    "\033[32m", // INFO  - Green
    "\033[33m", // WARN  - Yellow
    "\033[31m", // ERROR - Red
    "\033[41m"  // FATAL - Red Background
};

static const char* reset_color = "\033[0m";

static void create_dir_if_needed(const char* path) {
    char temp[256];
    char* p = NULL;
    size_t len;

    if (!path) return;
    
    // Copy path to temp
    len = strlen(path);
    if (len >= sizeof(temp)) return; // Too long
    strncpy(temp, path, sizeof(temp));
    temp[sizeof(temp) - 1] = 0;

    // Remove file name
    p = strrchr(temp, '/');
    if (!p) p = strrchr(temp, '\\');
    if (p) {
        *p = 0; // Truncate to directory
    } else {
        return; // No directory part
    }

    // Attempt create
#ifdef _WIN32
    _mkdir(temp);
#else
    mkdir(temp, 0755);
#endif
}

void logger_init(const char* log_file_path) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_initialized) {
        pthread_mutex_unlock(&g_log_mutex);
        return;
    }

    if (log_file_path) {
        create_dir_if_needed(log_file_path);
        g_log_file = fopen(log_file_path, "w");
        if (!g_log_file) {
            fprintf(stderr, "Logger: Failed to open log file '%s'\n", log_file_path);
        }
    }

    g_initialized = true;
    pthread_mutex_unlock(&g_log_mutex);
}

void logger_shutdown(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    g_initialized = false;
    pthread_mutex_unlock(&g_log_mutex);
}

void logger_set_level(LogLevel level) {
    // Legacy behavior: Sets Console Level
    g_console_level = level;
}

void logger_set_console_level(LogLevel level) {
    g_console_level = level;
}

void logger_set_file_level(LogLevel level) {
    g_file_level = level;
}

LogLevel logger_get_level(void) {
    return g_console_level;
}

static double g_trace_interval = 5.0; // Default 5s

void logger_set_trace_interval(double seconds) {
    g_trace_interval = seconds;
}

double logger_get_trace_interval(void) {
    return g_trace_interval;
}

void logger_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    // Quick check before lock (optimization)
    // Note: Technically racy but harmless for logging levels
    if (level < g_console_level && level < g_file_level) {
        return;
    }

    // Get time
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t); // Thread-safe
    
    char time_short[16];
    char time_long[32];
    strftime(time_short, sizeof(time_short), "%H:%M:%S", &t);
    strftime(time_long, sizeof(time_long), "%Y-%m-%d %H:%M:%S", &t);

    // Simplify file path
    const char* short_file = strrchr(file, '/');
    if (short_file) {
        short_file++; 
    } else {
        short_file = file; 
    }

    pthread_mutex_lock(&g_log_mutex);

    // Console Output
    if (level >= g_console_level) {
        FILE* out = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        fprintf(out, "%s[%s] [%s]%s %s:%d: ", 
            level_colors[level], 
            time_short, 
            level_strings[level], 
            reset_color, 
            short_file, 
            line);
        
        va_list args;
        va_start(args, fmt);
        vfprintf(out, fmt, args);
        va_end(args);
        fprintf(out, "\n");
        fflush(out); // Ensure console is snappy
    }

    // File Output
    if (g_log_file && level >= g_file_level) {
        fprintf(g_log_file, "[%s] [%s] %s:%d: ", 
            time_long, 
            level_strings[level], 
            short_file, 
            line);
        
        va_list args;
        va_start(args, fmt);
        vfprintf(g_log_file, fmt, args);
        va_end(args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file); // Flush file to ensure logs are saved on crash
    }

    pthread_mutex_unlock(&g_log_mutex);

    if (level == LOG_LEVEL_FATAL) {
        logger_shutdown(); // Close file properly
        exit(1);
    }
}
