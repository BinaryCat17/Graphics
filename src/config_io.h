#pragma once

#include "config_document.h"

char *read_text_file(const char *path);

int parse_config(const char *path, ConfigFormat fmt, ConfigNode **out_root, ConfigError *err);
int parse_config_text(const char *text, ConfigFormat format, ConfigNode **out_root, ConfigError *err);

