#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

typedef enum LogLevel {
    LOG_LEVEL_TRACE, // Ultra-verbose, per-frame, variable tracing
    LOG_LEVEL_DEBUG, // Diagnostic information for developers
    LOG_LEVEL_INFO,  // Significant events (init, state changes)
    LOG_LEVEL_WARN,  // Potential issues that don't stop execution
    LOG_LEVEL_ERROR, // Errors that might impact functionality
    LOG_LEVEL_FATAL  // Critical errors causing shutdown
} LogLevel;

// Lifecycle
void logger_init(const char* log_file_path);
void logger_shutdown(void);

// Configuration
void logger_set_console_level(LogLevel level);
void logger_set_file_level(LogLevel level);
void logger_set_trace_interval(double seconds);
double logger_get_trace_interval(void);

LogLevel logger_get_level(void); // Gets Console Level

// Core logging function
void logger_log(LogLevel level, const char* file, int line, const char* fmt, ...);

// Macros for convenience
#define LOG_TRACE(...) logger_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  logger_log(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  logger_log(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif // LOGGER_H
