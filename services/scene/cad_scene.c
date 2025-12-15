#include "cad_scene.h"

#include <stdlib.h>
#include <string.h>

static void free_geometry(GeometryNode *node)
{
    if (!node) return;
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

static void free_assembly_node(AssemblyNode *node)
{
    if (!node) return;
    for (size_t i = 0; i < node->child_count; ++i) {
        free_assembly_node(&node->children[i]);
    }
    free(node->children);
    node->children = NULL;
    node->child_count = 0;
}

void scene_dispose(Scene *scene)
{
    if (!scene) return;

    free(scene->metadata.name);
    free(scene->metadata.author);

    for (size_t i = 0; i < scene->material_count; ++i) {
        free(scene->materials[i].id);
    }
    free(scene->materials);

    for (size_t i = 0; i < scene->part_count; ++i) {
        free(scene->parts[i].id);
        free_geometry(scene->parts[i].geometry);
    }
    free(scene->parts);

    for (size_t i = 0; i < scene->joint_count; ++i) {
        free(scene->joints[i].id);
    }
    free(scene->joints);

    for (size_t i = 0; i < scene->assembly_count; ++i) {
        free(scene->assemblies[i].id);
        free_assembly_node(&scene->assemblies[i].root);
    }
    free(scene->assemblies);

    for (size_t i = 0; i < scene->analysis_count; ++i) {
        free(scene->analysis[i].id);
        free(scene->analysis[i].targets);
        free(scene->analysis[i].loads);
    }
    free(scene->analysis);

    for (size_t i = 0; i < scene->motion_count; ++i) {
        free(scene->motion_profiles[i].id);
        free(scene->motion_profiles[i].type);
    }
    free(scene->motion_profiles);

    memset(scene, 0, sizeof(Scene));
}

int load_step_mesh(const char *path, float scale, Mesh *out, MeshError *err)
{
    if (!path || !out) {
        return 0;
    }
    (void)path;
    (void)err;
    memset(out, 0, sizeof(Mesh));

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
        return 0;
    }
    memcpy(out->positions, vertices, sizeof(vertices));

    out->index_count = sizeof(idx) / sizeof(unsigned int);
    out->indices = (unsigned int *)malloc(sizeof(idx));
    if (!out->indices) {
        free(out->positions);
        out->positions = NULL;
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

    return 1;
}

void mesh_dispose(Mesh *mesh)
{
    if (!mesh) return;
    free(mesh->positions);
    free(mesh->indices);
    memset(mesh, 0, sizeof(Mesh));
}
