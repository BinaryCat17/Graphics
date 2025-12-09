#include "cad_scene.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum YamlNodeType {
    YAML_UNKNOWN,
    YAML_SCALAR,
    YAML_MAP,
    YAML_SEQUENCE,
} YamlNodeType;

typedef struct YamlNode YamlNode;

typedef struct {
    char *key;
    YamlNode *value;
} YamlPair;

typedef struct YamlNode {
    YamlNodeType type;
    int line;
    char *scalar;
    YamlPair *pairs;
    size_t pair_count;
    size_t pair_capacity;
    YamlNode **items;
    size_t item_count;
    size_t item_capacity;
} YamlNode;

typedef struct {
    int indent;
    YamlNode *node;
} Context;

static void set_error(SceneError *err, int line, int column, const char *msg)
{
    if (!err) {
        return;
    }
    err->line = line;
    err->column = column;
    strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = 0;
}

static char *dup_range(const char *begin, const char *end)
{
    size_t len = (size_t)(end - begin);
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, begin, len);
    out[len] = 0;
    return out;
}

static YamlNode *yaml_node_new(YamlNodeType type, int line)
{
    YamlNode *n = (YamlNode *)calloc(1, sizeof(YamlNode));
    if (!n) {
        return NULL;
    }
    n->type = type;
    n->line = line;
    return n;
}

static void yaml_node_free(YamlNode *node)
{
    if (!node) {
        return;
    }
    free(node->scalar);
    for (size_t i = 0; i < node->pair_count; ++i) {
        free(node->pairs[i].key);
        yaml_node_free(node->pairs[i].value);
    }
    free(node->pairs);
    for (size_t i = 0; i < node->item_count; ++i) {
        yaml_node_free(node->items[i]);
    }
    free(node->items);
    free(node);
}

static int yaml_pair_append(YamlNode *map, const char *key, YamlNode *value)
{
    if (map->pair_count + 1 > map->pair_capacity) {
        size_t new_cap = map->pair_capacity == 0 ? 4 : map->pair_capacity * 2;
        YamlPair *expanded = (YamlPair *)realloc(map->pairs, new_cap * sizeof(YamlPair));
        if (!expanded) {
            return 0;
        }
        map->pairs = expanded;
        map->pair_capacity = new_cap;
    }
    map->pairs[map->pair_count].key = strdup(key);
    map->pairs[map->pair_count].value = value;
    map->pair_count++;
    return 1;
}

static int yaml_sequence_append(YamlNode *seq, YamlNode *value)
{
    if (seq->item_count + 1 > seq->item_capacity) {
        size_t new_cap = seq->item_capacity == 0 ? 4 : seq->item_capacity * 2;
        YamlNode **expanded = (YamlNode **)realloc(seq->items, new_cap * sizeof(YamlNode *));
        if (!expanded) {
            return 0;
        }
        seq->items = expanded;
        seq->item_capacity = new_cap;
    }
    seq->items[seq->item_count++] = value;
    return 1;
}

static const char *trim_left(const char *s)
{
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
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

static int yaml_parse(const char *text, YamlNode **out_root, SceneError *err)
{
    YamlNode *root = yaml_node_new(YAML_MAP, 1);
    if (!root) {
        return 0;
    }

    Context stack[128];
    size_t depth = 0;
    stack[depth++] = (Context){-1, root};

    int line_number = 0;
    const char *cursor = text;
    while (*cursor) {
        const char *line_start = cursor;
        while (*cursor && *cursor != '\n' && *cursor != '\r') {
            ++cursor;
        }
        size_t line_len = (size_t)(cursor - line_start);
        char *line = (char *)malloc(line_len + 1);
        if (!line) {
            yaml_node_free(root);
            return 0;
        }
        memcpy(line, line_start, line_len);
        line[line_len] = 0;
        if (*cursor == '\r' && *(cursor + 1) == '\n') {
            cursor += 2;
        } else if (*cursor) {
            ++cursor;
        }

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
            yaml_node_free(root);
            set_error(err, line_number, 1, "Invalid indentation");
            return 0;
        }

        YamlNode *parent = stack[depth - 1].node;
        if (parent->type == YAML_UNKNOWN) {
            parent->type = (*p == '-') ? YAML_SEQUENCE : YAML_MAP;
        }

        if (*p == '-') {
            p = trim_left(p + 1);
            if (parent->type != YAML_SEQUENCE) {
                yaml_node_free(root);
                free(line);
                set_error(err, line_number, indent + 1, "Sequence item in non-sequence");
                return 0;
            }

            YamlNode *item = yaml_node_new(YAML_UNKNOWN, line_number);
            if (!item || !yaml_sequence_append(parent, item)) {
                free(line);
                yaml_node_free(root);
                return 0;
            }

            char *colon = strchr(p, ':');
            if (colon) {
                item->type = YAML_MAP;
                const char *key_start = p;
                const char *key_end = colon;
                while (key_end > key_start && isspace((unsigned char)*(key_end - 1))) {
                    --key_end;
                }
                char *key = dup_range(key_start, key_end);
                char *value_text = NULL;
                const char *value_start = colon + 1;
                if (*value_start) {
                    if (!parse_scalar_value(value_start, &value_text)) {
                        free(line);
                        yaml_node_free(root);
                        return 0;
                    }
                    YamlNode *scalar_node = yaml_node_new(YAML_SCALAR, line_number);
                    scalar_node->scalar = value_text;
                    yaml_pair_append(item, key, scalar_node);
                    free(key);
                } else {
                    yaml_pair_append(item, key, yaml_node_new(YAML_UNKNOWN, line_number));
                    free(key);
                }
            }

            stack[depth++] = (Context){indent, item};
        } else {
            if (parent->type != YAML_MAP) {
                yaml_node_free(root);
                free(line);
                set_error(err, line_number, indent + 1, "Mapping entry in non-map");
                return 0;
            }

            char *colon = strchr(p, ':');
            if (!colon) {
                yaml_node_free(root);
                free(line);
                set_error(err, line_number, indent + 1, "Missing ':' in mapping entry");
                return 0;
            }
            const char *key_start = p;
            const char *key_end = colon;
            while (key_end > key_start && isspace((unsigned char)*(key_end - 1))) {
                --key_end;
            }
            char *key = dup_range(key_start, key_end);
            const char *value_start = colon + 1;

            char *value_text = NULL;
            if (*value_start) {
                if (!parse_scalar_value(value_start, &value_text)) {
                    free(line);
                    free(key);
                    yaml_node_free(root);
                    return 0;
                }
                YamlNode *scalar = yaml_node_new(YAML_SCALAR, line_number);
                scalar->scalar = value_text;
                yaml_pair_append(parent, key, scalar);
                stack[depth++] = (Context){indent, scalar};
            } else {
                YamlNode *child = yaml_node_new(YAML_UNKNOWN, line_number);
                yaml_pair_append(parent, key, child);
                stack[depth++] = (Context){indent, child};
            }
            free(key);
        }
        free(line);
    }

    *out_root = root;
    return 1;
}

static YamlNode *yaml_map_get(const YamlNode *map, const char *key)
{
    if (!map || map->type != YAML_MAP) {
        return NULL;
    }
    for (size_t i = 0; i < map->pair_count; ++i) {
        if (map->pairs[i].key && strcmp(map->pairs[i].key, key) == 0) {
            return map->pairs[i].value;
        }
    }
    return NULL;
}

static float parse_float(const char *s)
{
    if (!s) {
        return 0.0f;
    }
    return (float)strtod(s, NULL);
}

static int parse_float_array(const YamlNode *node, float *out, size_t expected, SceneError *err)
{
    if (!node) {
        return 0;
    }
    if (node->type == YAML_SCALAR) {
        const char *s = node->scalar;
        size_t idx = 0;
        const char *p = s;
        while (*p && idx < expected) {
            while (*p && (*p == '[' || *p == ']' || *p == ',' || isspace((unsigned char)*p))) {
                ++p;
            }
            if (!*p) {
                break;
            }
            out[idx++] = (float)strtod(p, (char **)&p);
        }
        return idx == expected;
    }
    if (node->type == YAML_SEQUENCE) {
        if (node->item_count < expected) {
            return 0;
        }
        for (size_t i = 0; i < expected; ++i) {
            if (!node->items[i] || node->items[i]->type != YAML_SCALAR) {
                return 0;
            }
            out[i] = parse_float(node->items[i]->scalar);
        }
        return 1;
    }
    (void)err;
    return 0;
}

static GeometryPrimitiveType parse_primitive_type(const char *s)
{
    if (strcmp(s, "box") == 0) return GEO_PRIM_BOX;
    if (strcmp(s, "cylinder") == 0) return GEO_PRIM_CYLINDER;
    if (strcmp(s, "sphere") == 0) return GEO_PRIM_SPHERE;
    return GEO_PRIM_EXTRUDE;
}

static GeometryBooleanType parse_boolean_type(const char *s)
{
    if (strcmp(s, "difference") == 0) return GEO_BOOL_DIFFERENCE;
    if (strcmp(s, "intersection") == 0) return GEO_BOOL_INTERSECTION;
    return GEO_BOOL_UNION;
}

static JointType parse_joint_type(const char *s)
{
    if (strcmp(s, "prismatic") == 0) return JOINT_PRISMATIC;
    if (strcmp(s, "fixed") == 0) return JOINT_FIXED;
    return JOINT_REVOLUTE;
}

static void free_geometry(GeometryNode *node)
{
    if (!node) {
        return;
    }
    if (node->kind == GEO_BOOLEAN) {
        free_geometry(node->data.boolean.left);
        free_geometry(node->data.boolean.right);
    } else if (node->kind == GEO_SKETCH) {
        free(node->data.sketch.path);
    } else if (node->kind == GEO_STEP) {
        free(node->data.step.path);
    }
    free(node);
}

static void free_parts(Scene *scene)
{
    for (size_t i = 0; i < scene->part_count; ++i) {
        free(scene->parts[i].id);
        free(scene->parts[i].material_id);
        free_geometry(scene->parts[i].geometry);
    }
    free(scene->parts);
    scene->parts = NULL;
    scene->part_count = 0;
}

static void free_materials(Scene *scene)
{
    for (size_t i = 0; i < scene->material_count; ++i) {
        free(scene->materials[i].id);
    }
    free(scene->materials);
    scene->materials = NULL;
    scene->material_count = 0;
}

static void free_joints(Scene *scene)
{
    for (size_t i = 0; i < scene->joint_count; ++i) {
        free(scene->joints[i].id);
        free(scene->joints[i].parent);
        free(scene->joints[i].child);
    }
    free(scene->joints);
    scene->joints = NULL;
    scene->joint_count = 0;
}

static void free_assemblies(Scene *scene)
{
    for (size_t i = 0; i < scene->assembly_count; ++i) {
        free(scene->assemblies[i].id);
        free(scene->assemblies[i].root);
        for (size_t j = 0; j < scene->assemblies[i].child_count; ++j) {
            free(scene->assemblies[i].children[j].joint);
            free(scene->assemblies[i].children[j].child);
        }
        free(scene->assemblies[i].children);
    }
    free(scene->assemblies);
    scene->assemblies = NULL;
    scene->assembly_count = 0;
}

static void free_analysis(Scene *scene)
{
    for (size_t i = 0; i < scene->analysis_count; ++i) {
        free(scene->analysis[i].id);
        free(scene->analysis[i].type);
        for (size_t j = 0; j < scene->analysis[i].load_count; ++j) {
            free(scene->analysis[i].loads[j].target);
        }
        free(scene->analysis[i].loads);
    }
    free(scene->analysis);
    scene->analysis = NULL;
    scene->analysis_count = 0;
}

static void free_motion(Scene *scene)
{
    for (size_t i = 0; i < scene->motion_count; ++i) {
        free(scene->motion_profiles[i].id);
        free(scene->motion_profiles[i].joint);
        free(scene->motion_profiles[i].type);
    }
    free(scene->motion_profiles);
    scene->motion_profiles = NULL;
    scene->motion_count = 0;
}

void scene_dispose(Scene *scene)
{
    if (!scene) {
        return;
    }
    free(scene->metadata.name);
    free(scene->metadata.author);
    free_materials(scene);
    free_parts(scene);
    free_joints(scene);
    free_assemblies(scene);
    free_analysis(scene);
    free_motion(scene);
}

static GeometryNode *parse_geometry_node(const YamlNode *node)
{
    if (!node || node->type != YAML_MAP) {
        return NULL;
    }
    YamlNode *primitive = yaml_map_get(node, "primitive");
    YamlNode *boolean = yaml_map_get(node, "boolean");
    YamlNode *sketch = yaml_map_get(node, "sketch");
    YamlNode *step = yaml_map_get(node, "step");

    if (primitive && primitive->type == YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_PRIMITIVE;
        YamlNode *type_node = yaml_map_get(primitive, "type");
        const char *type = type_node && type_node->scalar ? type_node->scalar : "";
        g->data.primitive.type = parse_primitive_type(type);
        YamlNode *size_node = yaml_map_get(primitive, "size");
        if (size_node) {
            parse_float_array(size_node, g->data.primitive.size, 3, NULL);
        }
        YamlNode *radius = yaml_map_get(primitive, "radius");
        if (radius && radius->scalar) {
            g->data.primitive.radius = parse_float(radius->scalar);
        }
        YamlNode *height = yaml_map_get(primitive, "height");
        if (height && height->scalar) {
            g->data.primitive.height = parse_float(height->scalar);
        }
        YamlNode *fillet = yaml_map_get(primitive, "fillet");
        if (fillet && fillet->scalar) {
            g->data.primitive.fillet = parse_float(fillet->scalar);
        }
        return g;
    }

    if (boolean && boolean->type == YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_BOOLEAN;
        YamlNode *op = yaml_map_get(boolean, "op");
        g->data.boolean.op = parse_boolean_type(op && op->scalar ? op->scalar : "union");
        g->data.boolean.left = parse_geometry_node(yaml_map_get(boolean, "left"));
        g->data.boolean.right = parse_geometry_node(yaml_map_get(boolean, "right"));
        return g;
    }

    if (sketch && sketch->type == YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_SKETCH;
        YamlNode *path = yaml_map_get(sketch, "path");
        if (path && path->scalar) {
            g->data.sketch.path = strdup(path->scalar);
        }
        return g;
    }

    if (step && step->type == YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_STEP;
        YamlNode *path = yaml_map_get(step, "path");
        if (path && path->scalar) {
            g->data.step.path = strdup(path->scalar);
        }
        YamlNode *scale = yaml_map_get(step, "scale");
        g->data.step.scale = (scale && scale->scalar) ? parse_float(scale->scalar) : 1.0f;
        return g;
    }

    return NULL;
}

static void parse_materials(Scene *scene, const YamlNode *root)
{
    const YamlNode *materials = yaml_map_get(root, "materials");
    if (!materials || materials->type != YAML_SEQUENCE) {
        return;
    }
    scene->material_count = materials->item_count;
    scene->materials = (Material *)calloc(scene->material_count, sizeof(Material));
    for (size_t i = 0; i < materials->item_count; ++i) {
        const YamlNode *m = materials->items[i];
        if (!m || m->type != YAML_MAP) continue;
        YamlNode *id = yaml_map_get(m, "id");
        YamlNode *density = yaml_map_get(m, "density");
        YamlNode *young = yaml_map_get(m, "young_modulus");
        YamlNode *pr = yaml_map_get(m, "poisson_ratio");
        scene->materials[i].id = id && id->scalar ? strdup(id->scalar) : NULL;
        scene->materials[i].density = density && density->scalar ? parse_float(density->scalar) : 0.0f;
        scene->materials[i].young_modulus = young && young->scalar ? parse_float(young->scalar) : 0.0f;
        scene->materials[i].poisson_ratio = pr && pr->scalar ? parse_float(pr->scalar) : 0.0f;
    }
}

static void parse_parts(Scene *scene, const YamlNode *root)
{
    const YamlNode *parts = yaml_map_get(root, "parts");
    if (!parts || parts->type != YAML_SEQUENCE) {
        return;
    }
    scene->part_count = parts->item_count;
    scene->parts = (Part *)calloc(scene->part_count, sizeof(Part));
    for (size_t i = 0; i < parts->item_count; ++i) {
        const YamlNode *p = parts->items[i];
        if (!p || p->type != YAML_MAP) continue;
        YamlNode *id = yaml_map_get(p, "id");
        YamlNode *material = yaml_map_get(p, "material");
        YamlNode *geometry = yaml_map_get(p, "geometry");
        scene->parts[i].id = id && id->scalar ? strdup(id->scalar) : NULL;
        scene->parts[i].material_id = material && material->scalar ? strdup(material->scalar) : NULL;
        scene->parts[i].geometry = parse_geometry_node(geometry);
        YamlNode *transform = yaml_map_get(p, "transform");
        if (transform && transform->type == YAML_MAP) {
            YamlNode *translate = yaml_map_get(transform, "translate");
            if (translate && parse_float_array(translate, scene->parts[i].transform.translate, 3, NULL)) {
                scene->parts[i].transform.has_translate = 1;
            }
        }
    }
}

static void parse_joints(Scene *scene, const YamlNode *root)
{
    const YamlNode *joints = yaml_map_get(root, "joints");
    if (!joints || joints->type != YAML_SEQUENCE) {
        return;
    }
    scene->joint_count = joints->item_count;
    scene->joints = (Joint *)calloc(scene->joint_count, sizeof(Joint));
    for (size_t i = 0; i < joints->item_count; ++i) {
        const YamlNode *j = joints->items[i];
        if (!j || j->type != YAML_MAP) continue;
        YamlNode *id = yaml_map_get(j, "id");
        YamlNode *parent = yaml_map_get(j, "parent");
        YamlNode *child = yaml_map_get(j, "child");
        YamlNode *type = yaml_map_get(j, "type");
        scene->joints[i].id = id && id->scalar ? strdup(id->scalar) : NULL;
        scene->joints[i].parent = parent && parent->scalar ? strdup(parent->scalar) : NULL;
        scene->joints[i].child = child && child->scalar ? strdup(child->scalar) : NULL;
        scene->joints[i].type = parse_joint_type(type && type->scalar ? type->scalar : "revolute");
        parse_float_array(yaml_map_get(j, "origin"), scene->joints[i].origin, 3, NULL);
        parse_float_array(yaml_map_get(j, "axis"), scene->joints[i].axis, 3, NULL);
        YamlNode *limits = yaml_map_get(j, "limits");
        if (limits && limits->type == YAML_MAP) {
            scene->joints[i].limits.has_limits = 1;
            YamlNode *lower = yaml_map_get(limits, "lower");
            YamlNode *upper = yaml_map_get(limits, "upper");
            YamlNode *vel = yaml_map_get(limits, "velocity");
            YamlNode *acc = yaml_map_get(limits, "accel");
            if (lower && lower->scalar) scene->joints[i].limits.lower = parse_float(lower->scalar);
            if (upper && upper->scalar) scene->joints[i].limits.upper = parse_float(upper->scalar);
            if (vel && vel->scalar) scene->joints[i].limits.velocity = parse_float(vel->scalar);
            if (acc && acc->scalar) scene->joints[i].limits.accel = parse_float(acc->scalar);
        }
    }
}

static void parse_assemblies(Scene *scene, const YamlNode *root)
{
    const YamlNode *assemblies = yaml_map_get(root, "assemblies");
    if (!assemblies || assemblies->type != YAML_SEQUENCE) {
        return;
    }
    scene->assembly_count = assemblies->item_count;
    scene->assemblies = (Assembly *)calloc(scene->assembly_count, sizeof(Assembly));
    for (size_t i = 0; i < assemblies->item_count; ++i) {
        const YamlNode *a = assemblies->items[i];
        if (!a || a->type != YAML_MAP) continue;
        YamlNode *id = yaml_map_get(a, "id");
        YamlNode *root_id = yaml_map_get(a, "root");
        scene->assemblies[i].id = id && id->scalar ? strdup(id->scalar) : NULL;
        scene->assemblies[i].root = root_id && root_id->scalar ? strdup(root_id->scalar) : NULL;
        YamlNode *children = yaml_map_get(a, "children");
        if (children && children->type == YAML_SEQUENCE) {
            scene->assemblies[i].child_count = children->item_count;
            scene->assemblies[i].children = (AssemblyChild *)calloc(children->item_count, sizeof(AssemblyChild));
            for (size_t j = 0; j < children->item_count; ++j) {
                const YamlNode *c = children->items[j];
                if (!c || c->type != YAML_MAP) continue;
                YamlNode *joint = yaml_map_get(c, "joint");
                YamlNode *child = yaml_map_get(c, "child");
                scene->assemblies[i].children[j].joint = joint && joint->scalar ? strdup(joint->scalar) : NULL;
                scene->assemblies[i].children[j].child = child && child->scalar ? strdup(child->scalar) : NULL;
            }
        }
    }
}

static void parse_analysis(Scene *scene, const YamlNode *root)
{
    const YamlNode *analysis = yaml_map_get(root, "analysis");
    if (!analysis || analysis->type != YAML_SEQUENCE) {
        return;
    }
    scene->analysis_count = analysis->item_count;
    scene->analysis = (AnalysisCase *)calloc(scene->analysis_count, sizeof(AnalysisCase));
    for (size_t i = 0; i < analysis->item_count; ++i) {
        const YamlNode *a = analysis->items[i];
        if (!a || a->type != YAML_MAP) continue;
        YamlNode *id = yaml_map_get(a, "id");
        YamlNode *type = yaml_map_get(a, "type");
        scene->analysis[i].id = id && id->scalar ? strdup(id->scalar) : NULL;
        scene->analysis[i].type = type && type->scalar ? strdup(type->scalar) : NULL;
        YamlNode *loads = yaml_map_get(a, "loads");
        if (loads && loads->type == YAML_SEQUENCE) {
            scene->analysis[i].load_count = loads->item_count;
            scene->analysis[i].loads = (AnalysisLoad *)calloc(loads->item_count, sizeof(AnalysisLoad));
            for (size_t j = 0; j < loads->item_count; ++j) {
                const YamlNode *l = loads->items[j];
                if (!l || l->type != YAML_MAP) continue;
                YamlNode *target = yaml_map_get(l, "target");
                YamlNode *force = yaml_map_get(l, "force");
                YamlNode *moment = yaml_map_get(l, "moment");
                YamlNode *point = yaml_map_get(l, "point");
                YamlNode *fixed = yaml_map_get(l, "fixed");
                scene->analysis[i].loads[j].target = target && target->scalar ? strdup(target->scalar) : NULL;
                if (force && parse_float_array(force, scene->analysis[i].loads[j].force, 3, NULL)) {
                    scene->analysis[i].loads[j].has_force = 1;
                }
                if (moment && parse_float_array(moment, scene->analysis[i].loads[j].moment, 3, NULL)) {
                    scene->analysis[i].loads[j].has_moment = 1;
                }
                if (point && parse_float_array(point, scene->analysis[i].loads[j].point, 3, NULL)) {
                    scene->analysis[i].loads[j].has_point = 1;
                }
                if (fixed && fixed->scalar) {
                    scene->analysis[i].loads[j].fixed = strcmp(fixed->scalar, "true") == 0 || strcmp(fixed->scalar, "1") == 0;
                }
            }
        }
    }
}

static void parse_motion(Scene *scene, const YamlNode *root)
{
    const YamlNode *motion = yaml_map_get(root, "motion");
    if (!motion || motion->type != YAML_SEQUENCE) {
        return;
    }
    scene->motion_count = motion->item_count;
    scene->motion_profiles = (MotionProfile *)calloc(scene->motion_count, sizeof(MotionProfile));
    for (size_t i = 0; i < motion->item_count; ++i) {
        const YamlNode *m = motion->items[i];
        if (!m || m->type != YAML_MAP) continue;
        YamlNode *id = yaml_map_get(m, "id");
        YamlNode *joint = yaml_map_get(m, "joint");
        YamlNode *profile = yaml_map_get(m, "profile");
        scene->motion_profiles[i].id = id && id->scalar ? strdup(id->scalar) : NULL;
        scene->motion_profiles[i].joint = joint && joint->scalar ? strdup(joint->scalar) : NULL;
        if (profile && profile->type == YAML_MAP) {
            YamlNode *type = yaml_map_get(profile, "type");
            scene->motion_profiles[i].type = type && type->scalar ? strdup(type->scalar) : NULL;
            YamlNode *start = yaml_map_get(profile, "start");
            YamlNode *end = yaml_map_get(profile, "end");
            YamlNode *v_max = yaml_map_get(profile, "v_max");
            YamlNode *amp = yaml_map_get(profile, "amplitude");
            YamlNode *freq = yaml_map_get(profile, "frequency");
            if (start && start->scalar) scene->motion_profiles[i].start = parse_float(start->scalar);
            if (end && end->scalar) scene->motion_profiles[i].end = parse_float(end->scalar);
            if (v_max && v_max->scalar) scene->motion_profiles[i].v_max = parse_float(v_max->scalar);
            if (amp && amp->scalar) scene->motion_profiles[i].amplitude = parse_float(amp->scalar);
            if (freq && freq->scalar) scene->motion_profiles[i].frequency = parse_float(freq->scalar);
        }
    }
}

static float unit_scale(const char *unit, const char *mm, float mm_scale, const char *cm, float cm_scale, const char *m, float m_scale)
{
    if (!unit) return 1.0f;
    if (strcmp(unit, mm) == 0) return mm_scale;
    if (strcmp(unit, cm) == 0) return cm_scale;
    if (strcmp(unit, m) == 0) return m_scale;
    return 1.0f;
}

int parse_scene_yaml(const char *path, Scene *out, SceneError *err)
{
    if (!out || !path) {
        return 0;
    }
    memset(out, 0, sizeof(Scene));

    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error(err, 0, 0, "Failed to open scene file");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)len + 1);
    if (!text) {
        fclose(f);
        return 0;
    }
    fread(text, 1, (size_t)len, f);
    text[len] = 0;
    fclose(f);

    YamlNode *root = NULL;
    if (!yaml_parse(text, &root, err)) {
        free(text);
        return 0;
    }
    free(text);

    YamlNode *version = yaml_map_get(root, "version");
    out->version = version && version->scalar ? (int)strtol(version->scalar, NULL, 10) : 1;

    YamlNode *metadata = yaml_map_get(root, "metadata");
    if (metadata && metadata->type == YAML_MAP) {
        YamlNode *name = yaml_map_get(metadata, "name");
        YamlNode *author = yaml_map_get(metadata, "author");
        if (name && name->scalar) out->metadata.name = strdup(name->scalar);
        if (author && author->scalar) out->metadata.author = strdup(author->scalar);
    }

    YamlNode *units = yaml_map_get(root, "units");
    if (units && units->type == YAML_MAP) {
        YamlNode *length = yaml_map_get(units, "length");
        YamlNode *angle = yaml_map_get(units, "angle");
        const char *length_s = length && length->scalar ? length->scalar : "mm";
        const char *angle_s = angle && angle->scalar ? angle->scalar : "deg";
        out->units.length_scale = unit_scale(length_s, "mm", 0.001f, "cm", 0.01f, "m", 1.0f);
        out->units.angle_scale = unit_scale(angle_s, "deg", (float)(3.14159265358979323846 / 180.0), "rad", 1.0f, "grad", 0.015707963f);
    } else {
        out->units.length_scale = 0.001f;
        out->units.angle_scale = (float)(3.14159265358979323846 / 180.0);
    }

    parse_materials(out, root);
    parse_parts(out, root);
    parse_joints(out, root);
    parse_assemblies(out, root);
    parse_analysis(out, root);
    parse_motion(out, root);

    yaml_node_free(root);
    return 1;
}

int load_step_mesh(const char *path, float scale, Mesh *out, MeshError *err)
{
    if (!path || !out) {
        return 0;
    }
    memset(out, 0, sizeof(Mesh));
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (err) {
            set_error((SceneError *)err, 0, 0, "Failed to open STEP file");
        }
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = (char *)malloc((size_t)len + 1);
    if (!buffer) {
        fclose(f);
        return 0;
    }
    fread(buffer, 1, (size_t)len, f);
    buffer[len] = 0;
    fclose(f);

    // Minimal stub: populate a unit cube mesh scaled by the requested factor.
    (void)buffer;
    float s = scale <= 0.0f ? 1.0f : scale;
    float vertices[] = {
        -0.5f * s, -0.5f * s, -0.5f * s,
         0.5f * s, -0.5f * s, -0.5f * s,
         0.5f * s,  0.5f * s, -0.5f * s,
        -0.5f * s,  0.5f * s, -0.5f * s,
        -0.5f * s, -0.5f * s,  0.5f * s,
         0.5f * s, -0.5f * s,  0.5f * s,
         0.5f * s,  0.5f * s,  0.5f * s,
        -0.5f * s,  0.5f * s,  0.5f * s,
    };
    unsigned int idx[] = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4,
        2, 3, 7, 2, 7, 6,
        1, 2, 6, 1, 6, 5,
        0, 3, 7, 0, 7, 4,
    };

    out->position_count = sizeof(vertices) / sizeof(float);
    out->positions = (float *)malloc(sizeof(vertices));
    if (!out->positions) {
        free(buffer);
        return 0;
    }
    memcpy(out->positions, vertices, sizeof(vertices));

    out->index_count = sizeof(idx) / sizeof(unsigned int);
    out->indices = (unsigned int *)malloc(sizeof(idx));
    if (!out->indices) {
        free(out->positions);
        free(buffer);
        return 0;
    }
    memcpy(out->indices, idx, sizeof(idx));

    for (int i = 0; i < 3; ++i) {
        out->aabb_min[i] = 1e9f;
        out->aabb_max[i] = -1e9f;
    }
    for (size_t i = 0; i < out->position_count / 3; ++i) {
        float x = out->positions[i * 3 + 0];
        float y = out->positions[i * 3 + 1];
        float z = out->positions[i * 3 + 2];
        out->aabb_min[0] = out->aabb_min[0] < x ? out->aabb_min[0] : x;
        out->aabb_min[1] = out->aabb_min[1] < y ? out->aabb_min[1] : y;
        out->aabb_min[2] = out->aabb_min[2] < z ? out->aabb_min[2] : z;
        out->aabb_max[0] = out->aabb_max[0] > x ? out->aabb_max[0] : x;
        out->aabb_max[1] = out->aabb_max[1] > y ? out->aabb_max[1] : y;
        out->aabb_max[2] = out->aabb_max[2] > z ? out->aabb_max[2] : z;
    }

    free(buffer);
    return 1;
}

void mesh_dispose(Mesh *mesh)
{
    if (!mesh) return;
    free(mesh->positions);
    free(mesh->indices);
    memset(mesh, 0, sizeof(Mesh));
}

