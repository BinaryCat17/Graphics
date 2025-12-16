#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static LogLevel g_current_level = LOG_LEVEL_INFO;

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

void logger_set_level(LogLevel level) {
    g_current_level = level;
}

LogLevel logger_get_level(void) {
    return g_current_level;
}

void logger_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < g_current_level) {
        return;
    }

    // Get time
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    // Determine output stream
    FILE* out = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;

    // Simplify file path (remove leading directories)
    const char* short_file = strrchr(file, '/');
    if (short_file) {
        short_file++; // Skip '/'
    } else {
        short_file = file; 
    }
    // Handle Windows paths if necessary
    const char* short_file_win = strrchr(short_file, '\\');
    if (short_file_win) {
        short_file = short_file_win + 1;
    }

    // Print Header: [Time] [Level] File:Line: 
    fprintf(out, "%s[%s] [%s]%s %s:%d: ", 
        level_colors[level], 
        time_str, 
        level_strings[level], 
        reset_color, 
        short_file, 
        line);

    // Print Message
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fprintf(out, "\n");
    
    // Flush to ensure logs appear immediately
    fflush(out);

    if (level == LOG_LEVEL_FATAL) {
        exit(1);
    }
}