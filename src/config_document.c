#include "config_document.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_io.h"
#include "simple_yaml.h"

typedef struct {
    ConfigFormat format;
    int (*parse_text)(const char *text, ConfigNode **out_root, ConfigError *err);
} ConfigFormatOps;

static int ascii_strcasecmp(const char *a, const char *b)
{
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b) {
        int da = tolower((unsigned char)*a);
        int db = tolower((unsigned char)*b);
        if (da != db) return da - db;
        ++a;
        ++b;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static void assign_error(ConfigError *err, int line, int column, const char *msg)
{
    if (!err) return;
    err->line = line;
    err->column = column;
    strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = 0;
}

static ConfigScalarType detect_scalar_type(const char *text)
{
    if (!text || !*text) return CONFIG_SCALAR_STRING;

    const char *p = text;
    while (isspace((unsigned char)*p)) p++;
    if (*p == 0) return CONFIG_SCALAR_STRING;

    if (ascii_strcasecmp(p, "true") == 0 || ascii_strcasecmp(p, "false") == 0) return CONFIG_SCALAR_BOOL;
    if (ascii_strcasecmp(p, "null") == 0 || ascii_strcasecmp(p, "~") == 0) return CONFIG_SCALAR_NULL;

    char *end = NULL;
    (void)strtod(p, &end);
    if (end && *end == '\0') return CONFIG_SCALAR_NUMBER;

    return CONFIG_SCALAR_STRING;
}

static ConfigNode *config_node_new(ConfigNodeType type, int line)
{
    ConfigNode *n = (ConfigNode *)calloc(1, sizeof(ConfigNode));
    if (!n) return NULL;
    n->type = type;
    n->line = line;
    n->scalar_type = CONFIG_SCALAR_STRING;
    return n;
}

static int config_pair_append(ConfigNode *map, const char *key, ConfigNode *value)
{
    if (map->pair_count + 1 > map->pair_capacity) {
        size_t new_cap = map->pair_capacity == 0 ? 4 : map->pair_capacity * 2;
        ConfigPair *expanded = (ConfigPair *)realloc(map->pairs, new_cap * sizeof(ConfigPair));
        if (!expanded) return 0;
        map->pairs = expanded;
        map->pair_capacity = new_cap;
    }
    map->pairs[map->pair_count].key = strdup(key);
    map->pairs[map->pair_count].value = value;
    map->pair_count++;
    return 1;
}

static int config_sequence_append(ConfigNode *seq, ConfigNode *value)
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

static ConfigNodeType map_simple_yaml_type(SimpleYamlNodeType t)
{
    switch (t) {
    case SIMPLE_YAML_SCALAR: return CONFIG_NODE_SCALAR;
    case SIMPLE_YAML_MAP: return CONFIG_NODE_MAP;
    case SIMPLE_YAML_SEQUENCE: return CONFIG_NODE_SEQUENCE;
    default: return CONFIG_NODE_SCALAR;
    }
}

static ConfigNode *copy_yaml_node(const SimpleYamlNode *node)
{
    if (!node) return NULL;
    ConfigNodeType type = map_simple_yaml_type(node->type);
    ConfigNode *out = config_node_new(type, node->line);
    if (!out) return NULL;
    if (node->scalar) {
        out->scalar = strdup(node->scalar);
        out->scalar_type = detect_scalar_type(out->scalar);
    }
    if (node->type == SIMPLE_YAML_MAP) {
        for (size_t i = 0; i < node->pair_count; ++i) {
            ConfigNode *child = copy_yaml_node(node->pairs[i].value);
            if (!child || !config_pair_append(out, node->pairs[i].key, child)) {
                config_node_free(out);
                return NULL;
            }
        }
    } else if (node->type == SIMPLE_YAML_SEQUENCE) {
        for (size_t i = 0; i < node->item_count; ++i) {
            ConfigNode *child = copy_yaml_node(node->items[i]);
            if (!child || !config_sequence_append(out, child)) {
                config_node_free(out);
                return NULL;
            }
        }
    }
    return out;
}

static int parse_yaml_text(const char *text, ConfigNode **out_root, ConfigError *err)
{
    SimpleYamlNode *root = NULL;
    SimpleYamlError yaml_err = {0};
    if (!simple_yaml_parse(text, &root, &yaml_err)) {
        assign_error(err, yaml_err.line, yaml_err.column, yaml_err.message);
        return 0;
    }
    ConfigNode *copied = copy_yaml_node(root);
    simple_yaml_free(root);
    if (!copied) return 0;
    *out_root = copied;
    return 1;
}

typedef enum { JSMN_UNDEFINED = 0, JSMN_OBJECT = 1, JSMN_ARRAY = 2, JSMN_STRING = 3, JSMN_PRIMITIVE = 4 } jsmntype_t;
typedef struct { jsmntype_t type; int start; int end; int size; } jsmntok_t;
typedef struct { unsigned int pos; unsigned int toknext; int toksuper; } jsmn_parser;

static void jsmn_init(jsmn_parser *p) { p->pos = 0; p->toknext = 0; p->toksuper = -1; }

static int jsmn_alloc(jsmn_parser *p, jsmntok_t *toks, size_t nt)
{
    if (p->toknext >= nt) return -1;
    toks[p->toknext].start = toks[p->toknext].end = -1;
    toks[p->toknext].size = 0;
    toks[p->toknext].type = JSMN_UNDEFINED;
    return (int)p->toknext++;
}

static int jsmn_parse(jsmn_parser *p, const char *js, size_t len, jsmntok_t *toks, size_t nt)
{
    for (size_t i = p->pos; i < len; i++) {
        char c = js[i];
        switch (c) {
        case '{':
        case '[': {
            int tk = jsmn_alloc(p, toks, nt);
            if (tk < 0) return -1;
            toks[tk].type = (c == '{') ? JSMN_OBJECT : JSMN_ARRAY;
            toks[tk].start = (int)i;
            toks[tk].size = 0;
            p->toksuper = tk;
            break;
        }
        case '}':
        case ']': {
            jsmntype_t want = (c == '}') ? JSMN_OBJECT : JSMN_ARRAY;
            int found = -1;
            for (int j = (int)p->toknext - 1; j >= 0; j--) {
                if (toks[j].start != -1 && toks[j].end == -1 && toks[j].type == want) { found = j; break; }
            }
            if (found == -1) return -1;
            toks[found].end = (int)i + 1;
            p->toksuper = -1;
            break;
        }
        case '"': {
            int tk = jsmn_alloc(p, toks, nt);
            if (tk < 0) return -1;
            toks[tk].type = JSMN_STRING;
            toks[tk].start = (int)i + 1;
            i++;
            while (i < len && js[i] != '"') i++;
            if (i >= len) return -1;
            toks[tk].end = (int)i;
            break;
        }
        default:
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' || c == ',') break;
            {
                int tk = jsmn_alloc(p, toks, nt);
                if (tk < 0) return -1;
                toks[tk].type = JSMN_PRIMITIVE;
                toks[tk].start = (int)i;
                while (i < len && js[i] != ',' && js[i] != ']' && js[i] != '}' && js[i] != '\n' && js[i] != '\r' && js[i] != '\t' && js[i] != ' ') i++;
                toks[tk].end = (int)i;
                i--;
            }
        }
    }
    p->pos = (unsigned int)len;
    return 0;
}

static char *tok_copy(const char *json, const jsmntok_t *t)
{
    int n = t->end - t->start;
    char *s = (char *)malloc((size_t)n + 1);
    if (!s) return NULL;
    memcpy(s, json + t->start, (size_t)n);
    s[n] = 0;
    return s;
}

static ConfigNode *parse_json_value(const char *text, jsmntok_t *toks, size_t tokc, size_t *idx)
{
    if (!toks || *idx >= tokc) return NULL;
    jsmntok_t *tok = &toks[*idx];
    ConfigNode *node = NULL;
    if (tok->type == JSMN_STRING || tok->type == JSMN_PRIMITIVE) {
        node = config_node_new(CONFIG_NODE_SCALAR, 0);
        if (node) {
            node->scalar = tok_copy(text, tok);
            node->scalar_type = (tok->type == JSMN_STRING) ? CONFIG_SCALAR_STRING : detect_scalar_type(node->scalar);
        }
        (*idx)++;
    } else if (tok->type == JSMN_OBJECT) {
        node = config_node_new(CONFIG_NODE_MAP, 0);
        (*idx)++;
        while (*idx < tokc && toks[*idx].start >= tok->start && toks[*idx].end <= tok->end) {
            jsmntok_t *key_tok = &toks[*idx];
            (*idx)++;
            ConfigNode *val = parse_json_value(text, toks, tokc, idx);
            char *key = tok_copy(text, key_tok);
            if (!key || !val || !config_pair_append(node, key, val)) {
                free(key);
                config_node_free(val);
                config_node_free(node);
                return NULL;
            }
            free(key);
        }
    } else if (tok->type == JSMN_ARRAY) {
        node = config_node_new(CONFIG_NODE_SEQUENCE, 0);
        (*idx)++;
        while (*idx < tokc && toks[*idx].start >= tok->start && toks[*idx].end <= tok->end) {
            ConfigNode *val = parse_json_value(text, toks, tokc, idx);
            if (!val || !config_sequence_append(node, val)) {
                config_node_free(val);
                config_node_free(node);
                return NULL;
            }
        }
    }
    return node;
}

static int parse_json_text(const char *text, ConfigNode **out_root, ConfigError *err)
{
    if (!text || !out_root) return 0;
    size_t tokc = 2048;
    jsmntok_t *toks = (jsmntok_t *)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) return 0;
    memset(toks, 0, sizeof(jsmntok_t) * tokc);
    jsmn_parser p; jsmn_init(&p);
    if (jsmn_parse(&p, text, strlen(text), toks, tokc) < 0) {
        assign_error(err, 0, 0, "Failed to parse JSON");
        free(toks);
        return 0;
    }
    size_t idx = 0;
    ConfigNode *root = parse_json_value(text, toks, p.toknext, &idx);
    free(toks);
    if (!root) return 0;
    *out_root = root;
    return 1;
}

static ConfigFormatOps FORMAT_OPS[] = {
    { CONFIG_FORMAT_YAML, parse_yaml_text },
    { CONFIG_FORMAT_JSON, parse_json_text },
};

const ConfigNode *config_map_get(const ConfigNode *map, const char *key)
{
    if (!map || map->type != CONFIG_NODE_MAP || !key) return NULL;
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

void config_document_free(ConfigDocument *doc)
{
    if (!doc) return;
    free(doc->source_path);
    config_node_free(doc->root);
    doc->root = NULL;
    doc->source_path = NULL;
}

static int emit_json_internal(const ConfigNode *node, char **out)
{
    if (!node || !out) return 0;
    if (node->type == CONFIG_NODE_SCALAR) {
        if (!node->scalar) return 0;
        if (node->scalar_type == CONFIG_SCALAR_STRING) {
            size_t len = strlen(node->scalar) + 3;
            char *buf = (char *)malloc(len);
            if (!buf) return 0;
            snprintf(buf, len, "\"%s\"", node->scalar);
            *out = buf;
            return 1;
        }
        *out = strdup(node->scalar);
        return *out != NULL;
    }
    if (node->type == CONFIG_NODE_SEQUENCE) {
        char **items = (char **)calloc(node->item_count, sizeof(char *));
        if (!items) return 0;
        size_t total = 0;
        for (size_t i = 0; i < node->item_count; ++i) {
            if (!emit_json_internal(node->items[i], &items[i])) { for (size_t j = 0; j < i; ++j) free(items[j]); free(items); return 0; }
            total += strlen(items[i]) + 1;
        }
        char *buf = (char *)malloc(total + 3);
        if (!buf) { for (size_t i = 0; i < node->item_count; ++i) free(items[i]); free(items); return 0; }
        buf[0] = '['; size_t pos = 1;
        for (size_t i = 0; i < node->item_count; ++i) {
            size_t len = strlen(items[i]);
            memcpy(buf + pos, items[i], len);
            pos += len;
            if (i + 1 < node->item_count) buf[pos++] = ',';
            free(items[i]);
        }
        buf[pos++] = ']'; buf[pos] = 0;
        free(items);
        *out = buf;
        return 1;
    }
    if (node->type == CONFIG_NODE_MAP) {
        char **pairs = (char **)calloc(node->pair_count, sizeof(char *));
        if (!pairs) return 0;
        size_t total = 2; // for '{' and '}'
        for (size_t i = 0; i < node->pair_count; ++i) {
            char *val_json = NULL;
            if (!emit_json_internal(node->pairs[i].value, &val_json)) { for (size_t j = 0; j < i; ++j) free(pairs[j]); free(pairs); return 0; }
            size_t key_len = strlen(node->pairs[i].key);
            size_t val_len = strlen(val_json);
            size_t len = key_len + val_len + 6;
            pairs[i] = (char *)malloc(len);
            snprintf(pairs[i], len, "\"%s\":%s", node->pairs[i].key, val_json);
            free(val_json);
            total += strlen(pairs[i]);
            if (i + 1 < node->pair_count) total += 1; // comma
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

int config_emit_json(const ConfigNode *node, char **out_json)
{
    return emit_json_internal(node, out_json);
}

int load_config_document(const char *path, ConfigFormat format, ConfigDocument *out_doc, ConfigError *err)
{
    if (!path || !out_doc) return 0;
    memset(out_doc, 0, sizeof(*out_doc));

    const ConfigFormatOps *ops = NULL;
    for (size_t i = 0; i < sizeof(FORMAT_OPS) / sizeof(FORMAT_OPS[0]); ++i) {
        if (FORMAT_OPS[i].format == format) { ops = &FORMAT_OPS[i]; break; }
    }
    if (!ops) return 0;

    char *text = read_text_file(path);
    if (!text) {
        assign_error(err, 0, 0, "Failed to read file");
        return 0;
    }

    ConfigNode *root = NULL;
    int ok = ops->parse_text(text, &root, err);
    free(text);
    if (!ok) return 0;

    out_doc->format = format;
    out_doc->root = root;
    out_doc->source_path = strdup(path);
    return 1;
}

int parse_config_text(const char *text, ConfigFormat format, ConfigNode **out_root, ConfigError *err)
{
    if (!text || !out_root) return 0;
    const ConfigFormatOps *ops = NULL;
    for (size_t i = 0; i < sizeof(FORMAT_OPS) / sizeof(FORMAT_OPS[0]); ++i) {
        if (FORMAT_OPS[i].format == format) { ops = &FORMAT_OPS[i]; break; }
    }
    if (!ops) return 0;
    return ops->parse_text(text, out_root, err);
}

