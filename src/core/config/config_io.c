#include "config_io.h"
#include "core/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_text_file(const char *path)
{
    if (!path) return NULL;
    FILE *f = platform_fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)len + 1);
    if (!text) {
        fclose(f);
        return NULL;
    }
    fread(text, 1, (size_t)len, f);
    text[len] = 0;
    fclose(f);
    return text;
}

int parse_config(const char *path, ConfigFormat fmt, ConfigNode **out_root, ConfigError *err)
{
    if (!path || !out_root) return 0;
    char *text = read_text_file(path);
    if (!text) return 0;
    int ok = parse_config_text(text, fmt, out_root, err);
    free(text);
    return ok;
}

