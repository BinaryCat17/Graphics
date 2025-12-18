#include "simple_yaml.h"
#include "foundation/platform/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int indent;
    ConfigNode *node;
} SimpleYamlContext;

static void set_error(ConfigError *err, int line, int column, const char *msg)
{
    if (!err) return;
    err->line = line;
    err->column = column;
    platform_strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = 0;
}

static char *dup_range(const char *begin, const char *end)
{
    size_t len = (size_t)(end - begin);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, begin, len);
    out[len] = 0;
    return out;
}

static ConfigNode *yaml_node_new(ConfigNodeType type, int line)
{
    ConfigNode *n = (ConfigNode *)calloc(1, sizeof(ConfigNode));
    if (!n) return NULL;
    n->type = type;
    n->line = line;
    return n;
}

static int yaml_pair_append(ConfigNode *map, const char *key, ConfigNode *value)
{
    if (map->pair_count + 1 > map->pair_capacity) {
        size_t new_cap = map->pair_capacity == 0 ? 4 : map->pair_capacity * 2;
        ConfigPair *expanded = (ConfigPair *)realloc(map->pairs, new_cap * sizeof(ConfigPair));
        if (!expanded) return 0;
        map->pairs = expanded;
        map->pair_capacity = new_cap;
    }
    map->pairs[map->pair_count].key = platform_strdup(key);
    map->pairs[map->pair_count].value = value;
    map->pair_count++;
    return 1;
}

static int yaml_sequence_append(ConfigNode *seq, ConfigNode *value)
{
    if (seq->item_count + 1 > seq->item_capacity) {
        size_t new_cap = seq->item_capacity == 0 ? 4 : seq->item_capacity * 2;
        ConfigNode **expanded = (ConfigNode **)realloc(seq->items, new_cap * sizeof(ConfigNode *));
        if (!expanded) return 0;
        seq->items = expanded;
        seq->item_capacity = new_cap;
    }
    seq->items[seq->item_count++] = value;
    return 1;
}

static const char *trim_left(const char *s)
{
    while (*s && isspace((unsigned char)*s)) ++s;
    return s;
}

static void rstrip(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = 0;
    }
}

static int parse_scalar_value(const char *raw, char **out_value)
{
    const char *start = trim_left(raw);
    size_t len = strlen(start);
    if (len >= 2 && ((start[0] == '"' && start[len - 1] == '"') || (start[0] == '\'' && start[len - 1] == '\''))) {
        start++;
        len -= 2;
    }
    *out_value = dup_range(start, start + len);
    return *out_value != NULL;
}

int simple_yaml_parse(const char *text, ConfigNode **out_root, ConfigError *err)
{
    ConfigNode *root = yaml_node_new(CONFIG_NODE_MAP, 1);
    if (!root) return 0;

    SimpleYamlContext stack[128];
    size_t depth = 0;
    stack[depth++] = (SimpleYamlContext){-1, root};

    int line_number = 0;
    const char *cursor = text;
    while (*cursor) {
        const char *line_start = cursor;
        while (*cursor && *cursor != '\n' && *cursor != '\r') ++cursor;
        size_t line_len = (size_t)(cursor - line_start);
        char *line = (char *)malloc(line_len + 1);
        if (!line) {
            config_node_free(root);
            return 0;
        }
        memcpy(line, line_start, line_len);
        line[line_len] = 0;
        if (*cursor == '\r' && *(cursor + 1) == '\n') cursor += 2; else if (*cursor) ++cursor;

        line_number++;
        rstrip(line);
        char *comment = strchr(line, '#');
        if (comment) {
            *comment = 0;
            rstrip(line);
        }

        const char *p = line;
        int indent = 0;
        while (*p == ' ') {
            ++indent;
            ++p;
        }

        p = trim_left(p);
        if (*p == 0) {
            free(line);
            continue;
        }

        while (depth > 0 && indent <= stack[depth - 1].indent) {
            depth--;
        }
        if (depth == 0) {
            free(line);
            config_node_free(root);
            set_error(err, line_number, 1, "Invalid indentation");
            return 0;
        }

        ConfigNode *parent = stack[depth - 1].node;
        if (parent->type == CONFIG_NODE_UNKNOWN) {
            parent->type = (*p == '-') ? CONFIG_NODE_SEQUENCE : CONFIG_NODE_MAP;
        }

        if (*p == '-') {
            p = trim_left(p + 1);
            if (parent->type != CONFIG_NODE_SEQUENCE) {
                config_node_free(root);
                free(line);
                set_error(err, line_number, indent + 1, "Sequence item in non-sequence");
                return 0;
            }

            ConfigNode *item = yaml_node_new(CONFIG_NODE_UNKNOWN, line_number);
            if (!item || !yaml_sequence_append(parent, item)) {
                free(line);
                config_node_free(root);
                return 0;
            }

            char *colon = strchr(p, ':');
            if (colon) {
                item->type = CONFIG_NODE_MAP;
                const char *key_start = p;
                const char *key_end = colon;
                while (key_end > key_start && isspace((unsigned char)*(key_end - 1))) --key_end;
                char *key = dup_range(key_start, key_end);
                char *value_text = NULL;
                const char *value_start = colon + 1;
                if (*value_start) {
                    if (!parse_scalar_value(value_start, &value_text)) {
                        free(line);
                        config_node_free(root);
                        return 0;
                    }
                    ConfigNode *scalar_node = yaml_node_new(CONFIG_NODE_SCALAR, line_number);
                    scalar_node->scalar = value_text;
                    yaml_pair_append(item, key, scalar_node);
                    free(key);
                } else {
                    yaml_pair_append(item, key, yaml_node_new(CONFIG_NODE_UNKNOWN, line_number));
                    free(key);
                }
            } else if (*p) {
                char *value_text = NULL;
                if (parse_scalar_value(p, &value_text)) {
                    item->type = CONFIG_NODE_SCALAR;
                    item->scalar = value_text;
                }
            }

            stack[depth++] = (SimpleYamlContext){indent, item};
        } else {
            if (parent->type != CONFIG_NODE_MAP) {
                config_node_free(root);
                free(line);
                set_error(err, line_number, indent + 1, "Mapping entry in non-map");
                return 0;
            }

            char *colon = strchr(p, ':');
            if (!colon) {
                config_node_free(root);
                free(line);
                set_error(err, line_number, indent + 1, "Missing ':' in mapping entry");
                return 0;
            }
            const char *key_start = p;
            const char *key_end = colon;
            while (key_end > key_start && isspace((unsigned char)*(key_end - 1))) --key_end;
            char *key = dup_range(key_start, key_end);
            const char *value_start = colon + 1;

            char *value_text = NULL;
            if (*value_start) {
                if (!parse_scalar_value(value_start, &value_text)) {
                    free(line);
                    free(key);
                    config_node_free(root);
                    return 0;
                }
                ConfigNode *scalar = yaml_node_new(CONFIG_NODE_SCALAR, line_number);
                scalar->scalar = value_text;
                yaml_pair_append(parent, key, scalar);
                stack[depth++] = (SimpleYamlContext){indent, scalar};
            } else {
                ConfigNode *child = yaml_node_new(CONFIG_NODE_UNKNOWN, line_number);
                yaml_pair_append(parent, key, child);
                stack[depth++] = (SimpleYamlContext){indent, child};
            }
            free(key);
        }
        free(line);
    }

    *out_root = root;
    return 1;
}

const ConfigNode *config_node_map_get(const ConfigNode *map, const char *key)
{
    if (!map || map->type != CONFIG_NODE_MAP) return NULL;
    for (size_t i = 0; i < map->pair_count; ++i) {
        if (map->pairs[i].key && strcmp(map->pairs[i].key, key) == 0) return map->pairs[i].value;
    }
    return NULL;
}

void config_node_free(ConfigNode *node)
{
    if (!node) return;
    free(node->scalar);
    for (size_t i = 0; i < node->pair_count; ++i) {
        free(node->pairs[i].key);
        config_node_free(node->pairs[i].value);
    }
    free(node->pairs);
    for (size_t i = 0; i < node->item_count; ++i) {
        config_node_free(node->items[i]);
    }
    free(node->items);
    free(node);
}

static int emit_scalar_json(const ConfigNode *node, char **out)
{
    if (!node || !node->scalar) return 0;
    const char *s = node->scalar;
    int is_number = 1;
    char *endptr = NULL;
    strtod(s, &endptr);
    if (!s[0] || (endptr && *endptr != '\0')) is_number = 0;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0 || strcmp(s, "null") == 0) is_number = 1;

    size_t len = strlen(s);
    size_t buf_size = len + 4;
    char *buf = (char *)malloc(buf_size);
    if (!buf) return 0;
    if (is_number) {
        snprintf(buf, buf_size, "%s", s);
    } else {
        snprintf(buf, buf_size, "\"%s\"", s);
    }
    *out = buf;
    return 1;
}

static int emit_json_internal(const ConfigNode *node, char **out)
{
    if (!node) return 0;
    if (node->type == CONFIG_NODE_SCALAR) {
        return emit_scalar_json(node, out);
    }
    if (node->type == CONFIG_NODE_SEQUENCE) {
        size_t total = 2; // []
        char **children = (char **)calloc(node->item_count, sizeof(char *));
        if (!children) return 0;
        for (size_t i = 0; i < node->item_count; ++i) {
            if (!emit_json_internal(node->items[i], &children[i])) { free(children); return 0; }
            total += strlen(children[i]) + 1;
        }
        char *buf = (char *)malloc(total + 1);
        if (!buf) { for (size_t i = 0; i < node->item_count; ++i) free(children[i]); free(children); return 0; }
        buf[0] = '['; size_t pos = 1;
        for (size_t i = 0; i < node->item_count; ++i) {
            size_t len = strlen(children[i]);
            memcpy(buf + pos, children[i], len);
            pos += len;
            if (i + 1 < node->item_count) buf[pos++] = ',';
            free(children[i]);
        }
        buf[pos++] = ']'; buf[pos] = 0;
        free(children);
        *out = buf;
        return 1;
    }
    if (node->type == CONFIG_NODE_MAP || node->type == CONFIG_NODE_UNKNOWN) {
        size_t total = 2; // {}
        char **pairs = (char **)calloc(node->pair_count, sizeof(char *));
        if (!pairs) return 0;
        for (size_t i = 0; i < node->pair_count; ++i) {
            char *val_json = NULL;
            if (!emit_json_internal(node->pairs[i].value, &val_json)) {
                for (size_t j = 0; j < i; ++j) free(pairs[j]);
                free(pairs);
                return 0;
            }
            size_t key_len = strlen(node->pairs[i].key);
            size_t val_len = strlen(val_json);
            size_t len = key_len + val_len + 6;
            pairs[i] = (char *)malloc(len);
            snprintf(pairs[i], len, "\"%s\":%s", node->pairs[i].key, val_json);
            free(val_json);
            total += strlen(pairs[i]) + 1;
        }
        char *buf = (char *)malloc(total + 1);
        if (!buf) { for (size_t i = 0; i < node->pair_count; ++i) free(pairs[i]); free(pairs); return 0; }
        buf[0] = '{'; size_t pos = 1;
        for (size_t i = 0; i < node->pair_count; ++i) {
            size_t len = strlen(pairs[i]);
            memcpy(buf + pos, pairs[i], len);
            pos += len;
            if (i + 1 < node->pair_count) buf[pos++] = ',';
            free(pairs[i]);
        }
        buf[pos++] = '}'; buf[pos] = 0;
        free(pairs);
        *out = buf;
        return 1;
    }
    return 0;
}

int config_node_emit_json(const ConfigNode *node, char **out_json)
{
    return emit_json_internal(node, out_json);
}

