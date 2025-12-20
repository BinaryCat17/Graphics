#ifndef CONFIG_SYSTEM_H
#define CONFIG_SYSTEM_H

#include <stdbool.h>

// Initialize the global config system
void config_system_init(void);

// Shutdown the config system and free resources
void config_system_shutdown(void);

// Load configuration from CLI arguments and optional config file
// 1. Scans args for --config <path>
// 2. Loads YAML from path (or "config.yaml" default)
// 3. Parses all other args as overrides (--key value or --key=value)
void config_system_load(int argc, char** argv);

// Get a string value. Returns default_value if not found.
const char* config_get_string(const char* key, const char* default_value);

// Get an integer value. Returns default_value if not found.
int config_get_int(const char* key, int default_value);

// Get a float value. Returns default_value if not found.
float config_get_float(const char* key, float default_value);

// Get a boolean value. Returns default_value if not found.
bool config_get_bool(const char* key, bool default_value);

#endif // CONFIG_SYSTEM_H
