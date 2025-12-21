#include "simple_yaml.h"
#include "foundation/platform/platform.h"
#include "foundation/memory/arena.h"

#include <ctype.h>
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

static char *dup_range(MemoryArena* arena, const char *begin, const char *end)
{
    size_t len = (size_t)(end - begin);
    char *out = (char *)arena_alloc(arena, len + 1);
    if (!out) return NULL;
    memcpy(out, begin, len);
    out[len] = 0;
    return out;
}

static ConfigNode *yaml_node_new(MemoryArena* arena, ConfigNodeType type, int line)
{
    ConfigNode *n = (ConfigNode *)arena_alloc_zero(arena, sizeof(ConfigNode));
    if (!n) return NULL;
    n->type = type;
    n->line = line;
    return n;
}

static int yaml_pair_append(MemoryArena* arena, ConfigNode *map, char *key, ConfigNode *value)
{
    if (map->pair_count + 1 > map->pair_capacity) {
        size_t new_cap = map->pair_capacity == 0 ? 4 : map->pair_capacity * 2;
        ConfigPair *new_pairs = (ConfigPair *)arena_alloc(arena, new_cap * sizeof(ConfigPair));
        if (!new_pairs) return 0;
        
        if (map->pairs) {
            memcpy(new_pairs, map->pairs, map->pair_count * sizeof(ConfigPair));
        }
        
        map->pairs = new_pairs;
        map->pair_capacity = new_cap;
    }
    // We take ownership of key (allocated in arena)
    map->pairs[map->pair_count].key = key; 
    map->pairs[map->pair_count].value = value;
    map->pair_count++;
    return 1;
}

static int yaml_sequence_append(MemoryArena* arena, ConfigNode *seq, ConfigNode *value)
{
    if (seq->item_count + 1 > seq->item_capacity) {
        size_t new_cap = seq->item_capacity == 0 ? 4 : seq->item_capacity * 2;
        ConfigNode **new_items = (ConfigNode **)arena_alloc(arena, new_cap * sizeof(ConfigNode *));
        if (!new_items) return 0;
        
        if (seq->items) {
            memcpy((void*)new_items, (const void*)seq->items, seq->item_count * sizeof(ConfigNode *));
        }
        
        seq->items = new_items;
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

static int parse_scalar_value(MemoryArena* arena, const char *raw, char **out_value)
{
    const char *start = trim_left(raw);
    size_t len = strlen(start);
    if (len >= 2 && ((start[0] == '"' && start[len - 1] == '"') || (start[0] == '\'' && start[len - 1] == '\''))) {
        start++;
        len -= 2;
    }
    *out_value = dup_range(arena, start, start + len);
    return *out_value != NULL;
}

int simple_yaml_parse(MemoryArena* arena, const char *text, ConfigNode **out_root, ConfigError *err)
{
    if (!arena || !text) return 0;

    ConfigNode *root = yaml_node_new(arena, CONFIG_NODE_MAP, 1);
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
        
        // Use arena for line buffer (temporary, but arena is scratch so it's fine)
        char *line = (char *)arena_alloc(arena, line_len + 1);
        if (!line) {
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
            continue;
        }

        while (depth > 0 && indent <= stack[depth - 1].indent) {
            depth--;
        }
        if (depth == 0) {
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
                set_error(err, line_number, indent + 1, "Sequence item in non-sequence");
                return 0;
            }

            ConfigNode *item = yaml_node_new(arena, CONFIG_NODE_UNKNOWN, line_number);
            if (!item || !yaml_sequence_append(arena, parent, item)) {
                return 0;
            }

            char *colon = strchr(p, ':');
            if (colon) {
                item->type = CONFIG_NODE_MAP;
                const char *key_start = p;
                const char *key_end = colon;
                while (key_end > key_start && isspace((unsigned char)*(key_end - 1))) --key_end;
                
                char *key = dup_range(arena, key_start, key_end);
                char *value_text = NULL;
                const char *value_start = colon + 1;
                if (*value_start) {
                    if (!parse_scalar_value(arena, value_start, &value_text)) {
                        return 0;
                    }
                    ConfigNode *scalar_node = yaml_node_new(arena, CONFIG_NODE_SCALAR, line_number);
                    scalar_node->scalar = value_text;
                    yaml_pair_append(arena, item, key, scalar_node);
                } else {
                    yaml_pair_append(arena, item, key, yaml_node_new(arena, CONFIG_NODE_UNKNOWN, line_number));
                }
            } else if (*p) {
                char *value_text = NULL;
                if (parse_scalar_value(arena, p, &value_text)) {
                    item->type = CONFIG_NODE_SCALAR;
                    item->scalar = value_text;
                }
            }

            stack[depth++] = (SimpleYamlContext){indent, item};
        } else {
            if (parent->type != CONFIG_NODE_MAP) {
                set_error(err, line_number, indent + 1, "Mapping entry in non-map");
                return 0;
            }

            char *colon = strchr(p, ':');
            if (!colon) {
                set_error(err, line_number, indent + 1, "Missing ':' in mapping entry");
                return 0;
            }
            const char *key_start = p;
            const char *key_end = colon;
            while (key_end > key_start && isspace((unsigned char)*(key_end - 1))) --key_end;
            
            char *key = dup_range(arena, key_start, key_end);
            const char *value_start = colon + 1;

            char *value_text = NULL;
            if (*value_start) {
                if (!parse_scalar_value(arena, value_start, &value_text)) {
                    return 0;
                }
                ConfigNode *scalar = yaml_node_new(arena, CONFIG_NODE_SCALAR, line_number);
                scalar->scalar = value_text;
                yaml_pair_append(arena, parent, key, scalar);
                stack[depth++] = (SimpleYamlContext){indent, scalar};
            } else {
                ConfigNode *child = yaml_node_new(arena, CONFIG_NODE_UNKNOWN, line_number);
                yaml_pair_append(arena, parent, key, child);
                stack[depth++] = (SimpleYamlContext){indent, child};
            }
        }
    }

    *out_root = root;
    return 1;
}
