#include "domains/cad_model/cad_scene_yaml.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(SceneError *err, int line, int column, const char *msg)
{
    if (!err) return;
    err->line = line;
    err->column = column;
    platform_strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = 0;
}

static float parse_float(const char *s)
{
    if (!s) return 0.0f;
    return (float)strtod(s, NULL);
}

static int parse_float_array(const SimpleYamlNode *node, float *out, size_t expected)
{
    if (!node) return 0;
    if (node->type == SIMPLE_YAML_SCALAR) {
        const char *s = node->scalar;
        size_t idx = 0;
        const char *p = s;
        while (*p && idx < expected) {
            while (*p && (*p == '[' || *p == ']' || *p == ',' || (*p > 0 && *p <= 32))) ++p;
            if (!*p) break;
            out[idx++] = (float)strtod(p, (char **)&p);
        }
        return idx == expected;
    }
    if (node->type == SIMPLE_YAML_SEQUENCE && node->item_count >= expected) {
        for (size_t i = 0; i < expected; ++i) {
            if (!node->items[i] || node->items[i]->type != SIMPLE_YAML_SCALAR) return 0;
            out[i] = parse_float(node->items[i]->scalar);
        }
        return 1;
    }
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

static GeometryNode *parse_geometry_node(const SimpleYamlNode *node)
{
    if (!node || node->type != SIMPLE_YAML_MAP) return NULL;
    const SimpleYamlNode *primitive = simple_yaml_map_get(node, "primitive");
    const SimpleYamlNode *boolean = simple_yaml_map_get(node, "boolean");
    const SimpleYamlNode *sketch = simple_yaml_map_get(node, "sketch");
    const SimpleYamlNode *step = simple_yaml_map_get(node, "step");

    if (primitive && primitive->type == SIMPLE_YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_PRIMITIVE;
        const SimpleYamlNode *type_node = simple_yaml_map_get(primitive, "type");
        const char *type = type_node && type_node->scalar ? type_node->scalar : "";
        g->data.primitive.type = parse_primitive_type(type);
        const SimpleYamlNode *size_node = simple_yaml_map_get(primitive, "size");
        if (size_node) parse_float_array(size_node, g->data.primitive.size, 3);
        const SimpleYamlNode *radius = simple_yaml_map_get(primitive, "radius");
        if (radius && radius->scalar) g->data.primitive.radius = parse_float(radius->scalar);
        const SimpleYamlNode *height = simple_yaml_map_get(primitive, "height");
        if (height && height->scalar) g->data.primitive.height = parse_float(height->scalar);
        const SimpleYamlNode *fillet = simple_yaml_map_get(primitive, "fillet");
        if (fillet && fillet->scalar) g->data.primitive.fillet = parse_float(fillet->scalar);
        return g;
    }

    if (boolean && boolean->type == SIMPLE_YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_BOOLEAN;
        const SimpleYamlNode *op = simple_yaml_map_get(boolean, "op");
        const char *op_s = op && op->scalar ? op->scalar : "union";
        g->data.boolean.op = parse_boolean_type(op_s);
        g->data.boolean.left = parse_geometry_node(simple_yaml_map_get(boolean, "left"));
        g->data.boolean.right = parse_geometry_node(simple_yaml_map_get(boolean, "right"));
        return g;
    }

    if (sketch && sketch->type == SIMPLE_YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_SKETCH;
        const SimpleYamlNode *path = simple_yaml_map_get(sketch, "path");
        if (path && path->scalar) g->data.sketch.path = platform_strdup(path->scalar);
        return g;
    }

    if (step && step->type == SIMPLE_YAML_MAP) {
        GeometryNode *g = (GeometryNode *)calloc(1, sizeof(GeometryNode));
        if (!g) return NULL;
        g->kind = GEO_STEP;
        const SimpleYamlNode *path = simple_yaml_map_get(step, "path");
        const SimpleYamlNode *scale = simple_yaml_map_get(step, "scale");
        if (path && path->scalar) g->data.step.path = platform_strdup(path->scalar);
        g->data.step.scale = scale && scale->scalar ? parse_float(scale->scalar) : 1.0f;
        return g;
    }

    return NULL;
}

static Material *find_material(Scene *scene, const char *id)
{
    for (size_t i = 0; i < scene->material_count; ++i) {
        if (scene->materials[i].id && strcmp(scene->materials[i].id, id) == 0) return &scene->materials[i];
    }
    return NULL;
}

static Part *find_part(Scene *scene, const char *id)
{
    for (size_t i = 0; i < scene->part_count; ++i) {
        if (scene->parts[i].id && strcmp(scene->parts[i].id, id) == 0) return &scene->parts[i];
    }
    return NULL;
}

static Joint *find_joint(Scene *scene, const char *id)
{
    for (size_t i = 0; i < scene->joint_count; ++i) {
        if (scene->joints[i].id && strcmp(scene->joints[i].id, id) == 0) return &scene->joints[i];
    }
    return NULL;
}

static float unit_scale(const char *unit, const char *mm, float mm_scale, const char *cm, float cm_scale, const char *m, float m_scale)
{
    if (!unit) return 1.0f;
    if (strcmp(unit, mm) == 0) return mm_scale;
    if (strcmp(unit, cm) == 0) return cm_scale;
    if (strcmp(unit, m) == 0) return m_scale;
    return 1.0f;
}

static void init_identity(float *m)
{
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static int parse_assembly_children(Scene *scene, const SimpleYamlNode *children_node, AssemblyNode *parent, SceneError *err)
{
    if (!children_node) return 1;
    if (children_node->type != SIMPLE_YAML_SEQUENCE) {
        set_error(err, children_node->line, 1, "Assembly children must be a sequence");
        return 0;
    }
    parent->child_count = children_node->item_count;
    parent->children = (AssemblyNode *)calloc(parent->child_count, sizeof(AssemblyNode));
    if (!parent->children && parent->child_count > 0) return 0;

    for (size_t i = 0; i < parent->child_count; ++i) {
        const SimpleYamlNode *child = children_node->items[i];
        if (!child || child->type != SIMPLE_YAML_MAP) continue;
        const SimpleYamlNode *joint_id = simple_yaml_map_get(child, "joint");
        const SimpleYamlNode *child_id = simple_yaml_map_get(child, "child");
        if (!joint_id || !joint_id->scalar || !child_id || !child_id->scalar) {
            set_error(err, child ? child->line : 0, 1, "Assembly child missing joint or child");
            return 0;
        }
        Joint *joint = find_joint(scene, joint_id->scalar);
        Part *part = find_part(scene, child_id->scalar);
        if (!joint || !part) {
            set_error(err, child->line, 1, "Assembly references unknown joint or part");
            return 0;
        }
        parent->children[i].via_joint = joint;
        parent->children[i].part = part;
        if (!parse_assembly_children(scene, simple_yaml_map_get(child, "children"), &parent->children[i], err)) return 0;
    }
    return 1;
}

int parse_scene_yaml(const char *path, Scene *out, SceneError *err)
{
    if (!out || !path) return 0;
    memset(out, 0, sizeof(Scene));

    FILE *f = platform_fopen(path, "rb");
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

    SimpleYamlNode *root = NULL;
    SimpleYamlError yaml_err = {0};
    if (!simple_yaml_parse(text, &root, &yaml_err)) {
        free(text);
        set_error(err, yaml_err.line, yaml_err.column, yaml_err.message);
        return 0;
    }
    free(text);

    const SimpleYamlNode *version = simple_yaml_map_get(root, "version");
    out->version = version && version->scalar ? (int)strtol(version->scalar, NULL, 10) : 1;

    const SimpleYamlNode *metadata = simple_yaml_map_get(root, "metadata");
    if (metadata && metadata->type == SIMPLE_YAML_MAP) {
        const SimpleYamlNode *name = simple_yaml_map_get(metadata, "name");
        const SimpleYamlNode *author = simple_yaml_map_get(metadata, "author");
        if (name && name->scalar) out->metadata.name = platform_strdup(name->scalar);
        if (author && author->scalar) out->metadata.author = platform_strdup(author->scalar);
    }

    const SimpleYamlNode *units = simple_yaml_map_get(root, "units");
    if (units && units->type == SIMPLE_YAML_MAP) {
        const SimpleYamlNode *length = simple_yaml_map_get(units, "length");
        const SimpleYamlNode *angle = simple_yaml_map_get(units, "angle");
        const char *length_s = length && length->scalar ? length->scalar : "mm";
        const char *angle_s = angle && angle->scalar ? angle->scalar : "deg";
        out->units.length_scale = unit_scale(length_s, "mm", 0.001f, "cm", 0.01f, "m", 1.0f);
        out->units.angle_scale = unit_scale(angle_s, "deg", (float)(3.14159265358979323846 / 180.0), "rad", 1.0f, "grad", 0.015707963f);
    } else {
        out->units.length_scale = 0.001f;
        out->units.angle_scale = (float)(3.14159265358979323846 / 180.0);
    }

    /* Materials */
    const SimpleYamlNode *materials = simple_yaml_map_get(root, "materials");
    if (materials && materials->type == SIMPLE_YAML_SEQUENCE) {
        out->material_count = materials->item_count;
        out->materials = (Material *)calloc(out->material_count, sizeof(Material));
        if (!out->materials && out->material_count > 0) {
            simple_yaml_free(root);
            return 0;
        }
        for (size_t i = 0; i < out->material_count; ++i) {
            const SimpleYamlNode *m = materials->items[i];
            if (!m || m->type != SIMPLE_YAML_MAP) continue;
            const SimpleYamlNode *id = simple_yaml_map_get(m, "id");
            if (id && id->scalar) {
                for (size_t j = 0; j < i; ++j) {
                    if (out->materials[j].id && strcmp(out->materials[j].id, id->scalar) == 0) {
                        set_error(err, m->line, 1, "Duplicate material id");
                        simple_yaml_free(root);
                        return 0;
                    }
                }
                out->materials[i].id = platform_strdup(id->scalar);
            }
            const SimpleYamlNode *density = simple_yaml_map_get(m, "density");
            const SimpleYamlNode *young = simple_yaml_map_get(m, "young_modulus");
            const SimpleYamlNode *poisson = simple_yaml_map_get(m, "poisson_ratio");
            if (density && density->scalar) out->materials[i].density = parse_float(density->scalar);
            if (young && young->scalar) out->materials[i].young_modulus = parse_float(young->scalar);
            if (poisson && poisson->scalar) out->materials[i].poisson_ratio = parse_float(poisson->scalar);
        }
    }

    /* Parts */
    char **part_material_ids = NULL;
    const SimpleYamlNode *parts = simple_yaml_map_get(root, "parts");
    if (parts && parts->type == SIMPLE_YAML_SEQUENCE) {
        out->part_count = parts->item_count;
        out->parts = (Part *)calloc(out->part_count, sizeof(Part));
        part_material_ids = (char **)calloc(out->part_count, sizeof(char *));
        if ((!out->parts && out->part_count > 0) || (!part_material_ids && out->part_count > 0)) {
            simple_yaml_free(root);
            free(part_material_ids);
            scene_dispose(out);
            return 0;
        }
        for (size_t i = 0; i < out->part_count; ++i) {
            const SimpleYamlNode *p = parts->items[i];
            if (!p || p->type != SIMPLE_YAML_MAP) continue;
            const SimpleYamlNode *id = simple_yaml_map_get(p, "id");
            if (id && id->scalar) out->parts[i].id = platform_strdup(id->scalar);
            for (size_t j = 0; j < i; ++j) {
                if (out->parts[i].id && out->parts[j].id && strcmp(out->parts[i].id, out->parts[j].id) == 0) {
                    set_error(err, p->line, 1, "Duplicate part id");
                    simple_yaml_free(root);
                    free(part_material_ids);
                    scene_dispose(out);
                    return 0;
                }
            }
            init_identity(out->parts[i].transform);
            const SimpleYamlNode *mat = simple_yaml_map_get(p, "material");
            if (mat && mat->scalar) part_material_ids[i] = platform_strdup(mat->scalar);
            const SimpleYamlNode *geo = simple_yaml_map_get(p, "geometry");
            if (geo) out->parts[i].geometry = parse_geometry_node(geo);
        }
    }

    /* Joints */
    char **joint_parent_ids = NULL;
    char **joint_child_ids = NULL;
    const SimpleYamlNode *joints = simple_yaml_map_get(root, "joints");
    if (joints && joints->type == SIMPLE_YAML_SEQUENCE) {
        out->joint_count = joints->item_count;
        out->joints = (Joint *)calloc(out->joint_count, sizeof(Joint));
        joint_parent_ids = (char **)calloc(out->joint_count, sizeof(char *));
        joint_child_ids = (char **)calloc(out->joint_count, sizeof(char *));
        if ((!out->joints && out->joint_count > 0) || !joint_parent_ids || !joint_child_ids) {
            simple_yaml_free(root);
            free(part_material_ids);
            free(joint_parent_ids);
            free(joint_child_ids);
            scene_dispose(out);
            return 0;
        }
        for (size_t i = 0; i < out->joint_count; ++i) {
            const SimpleYamlNode *j = joints->items[i];
            if (!j || j->type != SIMPLE_YAML_MAP) continue;
            const SimpleYamlNode *id = simple_yaml_map_get(j, "id");
            if (id && id->scalar) out->joints[i].id = platform_strdup(id->scalar);
            for (size_t k = 0; k < i; ++k) {
                if (out->joints[i].id && out->joints[k].id && strcmp(out->joints[i].id, out->joints[k].id) == 0) {
                    set_error(err, j->line, 1, "Duplicate joint id");
                    simple_yaml_free(root);
                    free(part_material_ids);
                    free(joint_parent_ids);
                    free(joint_child_ids);
                    scene_dispose(out);
                    return 0;
                }
            }
            const SimpleYamlNode *parent = simple_yaml_map_get(j, "parent");
            const SimpleYamlNode *child = simple_yaml_map_get(j, "child");
            if (parent && parent->scalar) joint_parent_ids[i] = platform_strdup(parent->scalar);
            if (child && child->scalar) joint_child_ids[i] = platform_strdup(child->scalar);
            const SimpleYamlNode *type = simple_yaml_map_get(j, "type");
            out->joints[i].type = parse_joint_type(type && type->scalar ? type->scalar : "revolute");
            parse_float_array(simple_yaml_map_get(j, "origin"), out->joints[i].origin, 3);
            parse_float_array(simple_yaml_map_get(j, "axis"), out->joints[i].axis, 3);
        }
    }

    /* Assemblies */
    const SimpleYamlNode *assemblies = simple_yaml_map_get(root, "assemblies");
    if (assemblies && assemblies->type == SIMPLE_YAML_SEQUENCE) {
        out->assembly_count = assemblies->item_count;
        out->assemblies = (Assembly *)calloc(out->assembly_count, sizeof(Assembly));
        if (!out->assemblies && out->assembly_count > 0) {
            simple_yaml_free(root);
            free(part_material_ids);
            free(joint_parent_ids);
            free(joint_child_ids);
            scene_dispose(out);
            return 0;
        }
        for (size_t i = 0; i < out->assembly_count; ++i) {
            const SimpleYamlNode *a = assemblies->items[i];
            if (!a || a->type != SIMPLE_YAML_MAP) continue;
            const SimpleYamlNode *id = simple_yaml_map_get(a, "id");
            if (id && id->scalar) out->assemblies[i].id = platform_strdup(id->scalar);
            for (size_t k = 0; k < i; ++k) {
                if (out->assemblies[i].id && out->assemblies[k].id && strcmp(out->assemblies[i].id, out->assemblies[k].id) == 0) {
                    set_error(err, a->line, 1, "Duplicate assembly id");
                    simple_yaml_free(root);
                    free(part_material_ids);
                    free(joint_parent_ids);
                    free(joint_child_ids);
                    scene_dispose(out);
                    return 0;
                }
            }
            const SimpleYamlNode *root_part = simple_yaml_map_get(a, "root");
            if (!root_part || !root_part->scalar) {
                set_error(err, a->line, 1, "Assembly missing root part");
                simple_yaml_free(root);
                free(part_material_ids);
                free(joint_parent_ids);
                free(joint_child_ids);
                scene_dispose(out);
                return 0;
            }
            Part *root_p = find_part(out, root_part->scalar);
            if (!root_p) {
                set_error(err, a->line, 1, "Assembly root references unknown part");
                simple_yaml_free(root);
                free(part_material_ids);
                free(joint_parent_ids);
                free(joint_child_ids);
                scene_dispose(out);
                return 0;
            }
            out->assemblies[i].root.part = root_p;
            if (!parse_assembly_children(out, simple_yaml_map_get(a, "children"), &out->assemblies[i].root, err)) {
                simple_yaml_free(root);
                free(part_material_ids);
                free(joint_parent_ids);
                free(joint_child_ids);
                scene_dispose(out);
                return 0;
            }
        }
    }

    /* Analysis */
    char **load_targets = NULL;
    const SimpleYamlNode *analysis = simple_yaml_map_get(root, "analysis");
    if (analysis && analysis->type == SIMPLE_YAML_SEQUENCE) {
        out->analysis_count = analysis->item_count;
        out->analysis = (LoadCase *)calloc(out->analysis_count, sizeof(LoadCase));
        if (!out->analysis && out->analysis_count > 0) {
            simple_yaml_free(root);
            free(part_material_ids);
            free(joint_parent_ids);
            free(joint_child_ids);
            scene_dispose(out);
            return 0;
        }
        for (size_t i = 0; i < out->analysis_count; ++i) {
            load_targets = NULL;
            const SimpleYamlNode *a = analysis->items[i];
            if (!a || a->type != SIMPLE_YAML_MAP) continue;
            const SimpleYamlNode *id = simple_yaml_map_get(a, "id");
            if (id && id->scalar) out->analysis[i].id = platform_strdup(id->scalar);
            const SimpleYamlNode *loads = simple_yaml_map_get(a, "loads");
            if (loads && loads->type == SIMPLE_YAML_SEQUENCE) {
                out->analysis[i].load_count = loads->item_count;
                out->analysis[i].loads = (LoadVector *)calloc(out->analysis[i].load_count, sizeof(LoadVector));
                out->analysis[i].targets = (Part **)calloc(out->analysis[i].load_count, sizeof(Part *));
                load_targets = (char **)calloc(out->analysis[i].load_count, sizeof(char *));
                for (size_t j = 0; j < loads->item_count; ++j) {
                    const SimpleYamlNode *l = loads->items[j];
                    if (!l || l->type != SIMPLE_YAML_MAP) continue;
                    const SimpleYamlNode *target = simple_yaml_map_get(l, "target");
                    if (target && target->scalar) load_targets[j] = platform_strdup(target->scalar);
                    if (parse_float_array(simple_yaml_map_get(l, "force"), out->analysis[i].loads[j].force, 3)) out->analysis[i].loads[j].has_force = 1;
                    if (parse_float_array(simple_yaml_map_get(l, "moment"), out->analysis[i].loads[j].moment, 3)) out->analysis[i].loads[j].has_moment = 1;
                    if (parse_float_array(simple_yaml_map_get(l, "point"), out->analysis[i].loads[j].point, 3)) out->analysis[i].loads[j].has_point = 1;
                    const SimpleYamlNode *fixed = simple_yaml_map_get(l, "fixed");
                    if (fixed && fixed->scalar) out->analysis[i].loads[j].fixed = strcmp(fixed->scalar, "true") == 0 || strcmp(fixed->scalar, "1") == 0;
                }
                for (size_t j = 0; j < out->analysis[i].load_count; ++j) {
                    if (load_targets && load_targets[j]) {
                        Part *p = find_part(out, load_targets[j]);
                        if (!p) {
                            set_error(err, loads->items[j] ? loads->items[j]->line : 0, 1, "Load references unknown part");
                            simple_yaml_free(root);
                            free(part_material_ids);
                            free(joint_parent_ids);
                            free(joint_child_ids);
                            for (size_t t = 0; t < out->analysis[i].load_count; ++t) free(load_targets[t]);
                            free(load_targets);
                            scene_dispose(out);
                            return 0;
                        }
                        out->analysis[i].targets[j] = p;
                    }
                }
                if (load_targets) {
                    for (size_t j = 0; j < out->analysis[i].load_count; ++j) free(load_targets[j]);
                    free(load_targets);
                    load_targets = NULL;
                }
            }
        }
    }

    /* Motion */
    char **motion_joint_ids = NULL;
    const SimpleYamlNode *motion = simple_yaml_map_get(root, "motion");
    if (motion && motion->type == SIMPLE_YAML_SEQUENCE) {
        out->motion_count = motion->item_count;
        out->motion_profiles = (MotionProfile *)calloc(out->motion_count, sizeof(MotionProfile));
        motion_joint_ids = (char **)calloc(out->motion_count, sizeof(char *));
        if ((!out->motion_profiles && out->motion_count > 0) || !motion_joint_ids) {
            simple_yaml_free(root);
            free(part_material_ids);
            free(joint_parent_ids);
            free(joint_child_ids);
            scene_dispose(out);
            free(motion_joint_ids);
            return 0;
        }
        for (size_t i = 0; i < out->motion_count; ++i) {
            const SimpleYamlNode *m = motion->items[i];
            if (!m || m->type != SIMPLE_YAML_MAP) continue;
            const SimpleYamlNode *id = simple_yaml_map_get(m, "id");
            const SimpleYamlNode *joint = simple_yaml_map_get(m, "joint");
            const SimpleYamlNode *profile = simple_yaml_map_get(m, "profile");
            if (id && id->scalar) out->motion_profiles[i].id = platform_strdup(id->scalar);
            if (joint && joint->scalar) motion_joint_ids[i] = platform_strdup(joint->scalar);
            if (profile && profile->type == SIMPLE_YAML_MAP) {
                const SimpleYamlNode *type = simple_yaml_map_get(profile, "type");
                if (type && type->scalar) out->motion_profiles[i].type = platform_strdup(type->scalar);
                const SimpleYamlNode *start = simple_yaml_map_get(profile, "start");
                const SimpleYamlNode *end = simple_yaml_map_get(profile, "end");
                const SimpleYamlNode *v_max = simple_yaml_map_get(profile, "v_max");
                const SimpleYamlNode *amp = simple_yaml_map_get(profile, "amplitude");
                const SimpleYamlNode *freq = simple_yaml_map_get(profile, "frequency");
                if (start && start->scalar) out->motion_profiles[i].start = parse_float(start->scalar);
                if (end && end->scalar) out->motion_profiles[i].end = parse_float(end->scalar);
                if (v_max && v_max->scalar) out->motion_profiles[i].v_max = parse_float(v_max->scalar);
                if (amp && amp->scalar) out->motion_profiles[i].amplitude = parse_float(amp->scalar);
                if (freq && freq->scalar) out->motion_profiles[i].frequency = parse_float(freq->scalar);
            }
        }
    }

    /* Resolve references */
    for (size_t i = 0; i < out->part_count; ++i) {
        if (part_material_ids && part_material_ids[i]) {
            out->parts[i].material = find_material(out, part_material_ids[i]);
            if (!out->parts[i].material) {
                set_error(err, 0, 0, "Part references unknown material");
                simple_yaml_free(root);
                free(part_material_ids[i]);
                free(part_material_ids);
                free(joint_parent_ids);
                free(joint_child_ids);
                scene_dispose(out);
                return 0;
            }
            free(part_material_ids[i]);
        }
    }
    free(part_material_ids);

    for (size_t i = 0; i < out->joint_count; ++i) {
        if (joint_parent_ids && joint_parent_ids[i]) {
            out->joints[i].parent = find_part(out, joint_parent_ids[i]);
            free(joint_parent_ids[i]);
        }
        if (joint_child_ids && joint_child_ids[i]) {
            out->joints[i].child = find_part(out, joint_child_ids[i]);
            free(joint_child_ids[i]);
        }
        if (!out->joints[i].parent || !out->joints[i].child) {
            set_error(err, 0, 0, "Joint references unknown part");
            simple_yaml_free(root);
            free(joint_parent_ids);
            free(joint_child_ids);
            scene_dispose(out);
            return 0;
        }
    }
    free(joint_parent_ids);
    free(joint_child_ids);

    for (size_t i = 0; i < out->motion_count; ++i) {
        if (motion_joint_ids && motion_joint_ids[i]) {
            out->motion_profiles[i].joint = find_joint(out, motion_joint_ids[i]);
            free(motion_joint_ids[i]);
            if (!out->motion_profiles[i].joint) {
                set_error(err, 0, 0, "Motion references unknown joint");
                simple_yaml_free(root);
                free(motion_joint_ids);
                scene_dispose(out);
                return 0;
            }
        }
    }
    free(motion_joint_ids);

    simple_yaml_free(root);
    return 1;
}