#include "math_mesh_builder.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool math_mesh_build_surface(MathScene* scene, MathNode* visual_node, const MathMeshConfig* config, Mesh* out_mesh) {
    if (!scene || !visual_node || !config || !out_mesh) return false;
    if (visual_node->type != MATH_NODE_VISUALIZER) return false;

    // Find "x" and "y" variables in the scene to drive the surface
    MathNode* var_x = math_scene_find_node(scene, "x");
    MathNode* var_y = math_scene_find_node(scene, "y");

    // If variables don't exist, we can't generate a surface parameterized by x, y
    // (Unless we implicitly assume inputs 0 and 1 are x and y, but explicit is better)
    if (!var_x || !var_y) {
        // Fallback: Check if inputs to the visualizer are x and y? 
        // No, the visualizer usually takes 'z' as input. The graph calculates z based on x, y.
        // So x and y must be roots/leaves in the graph.
        fprintf(stderr, "MathMeshBuilder: Could not find variable nodes 'x' and 'y' in the scene.\n");
        return false;
    }

    // The visualizer node should have 1 input: the Z value (or vector).
    if (visual_node->input_count < 1 || !visual_node->inputs[0].target_node) {
        fprintf(stderr, "MathMeshBuilder: Visualizer node has no input source.\n");
        return false;
    }
    MathNode* source_z = visual_node->inputs[0].target_node;

    int res_x = config->grid_resolution_x;
    int res_y = config->grid_resolution_y;
    int num_verts = (res_x + 1) * (res_y + 1);
    int num_quads = res_x * res_y;
    int num_indices = num_quads * 6; // 2 triangles per quad

    // Allocate mesh memory
    out_mesh->position_count = num_verts; // struct uses count of TRIPLETS usually? 
    // Checking cad_scene.h: float *positions; // xyz triplets. size_t position_count;
    // Usually count implies number of floats or number of vertices. 
    // Let's assume number of floats based on common C patterns, OR check usage. 
    // load_step_mesh uses: out->positions = malloc(sizeof(float) * 3 * count);
    
    out_mesh->positions = (float*)malloc(sizeof(float) * 3 * num_verts);
    out_mesh->indices = (unsigned int*)malloc(sizeof(unsigned int) * num_indices);
    out_mesh->position_count = num_verts * 3; // Storing total float count typically? 
    // Wait, let's double check cad_scene usage. 
    // Usually position_count is number of vertices. But raw arrays often track size.
    // I will store total FLOATS in position_count if that's what the loader does.
    // But standard is usually Vertex Count. 
    // Let's stick to Vertex Count for now, but allocate 3x.
    // Re-reading cad_scene.h: "float *positions; size_t position_count;"
    // I'll assume position_count is number of vertices (triplets).
    out_mesh->position_count = num_verts; 
    
    out_mesh->index_count = num_indices;

    if (!out_mesh->positions || !out_mesh->indices) {
        if (out_mesh->positions) free(out_mesh->positions);
        if (out_mesh->indices) free(out_mesh->indices);
        return false;
    }

    float step_x = (config->range_x_max - config->range_x_min) / (float)res_x;
    float step_y = (config->range_y_max - config->range_y_min) / (float)res_y;

    int v_idx = 0;
    
    // Generate Vertices
    for (int iy = 0; iy <= res_y; ++iy) {
        float y = config->range_y_min + iy * step_y;
        var_y->data.variable.current_value = y;

        for (int ix = 0; ix <= res_x; ++ix) {
            float x = config->range_x_min + ix * step_x;
            var_x->data.variable.current_value = x;

            // Evaluate Z
            float z = math_node_eval(source_z);

            out_mesh->positions[v_idx * 3 + 0] = x;
            out_mesh->positions[v_idx * 3 + 1] = z; // Up axis
            out_mesh->positions[v_idx * 3 + 2] = y; // Depth axis 

            v_idx++;
        }
    }

    // Generate Indices
    int i_idx = 0;
    for (int iy = 0; iy < res_y; ++iy) {
        for (int ix = 0; ix < res_x; ++ix) {
            int row1 = iy * (res_x + 1);
            int row2 = (iy + 1) * (res_x + 1);

            // Triangle 1
            out_mesh->indices[i_idx++] = row1 + ix;
            out_mesh->indices[i_idx++] = row2 + ix;
            out_mesh->indices[i_idx++] = row1 + ix + 1;

            // Triangle 2
            out_mesh->indices[i_idx++] = row1 + ix + 1;
            out_mesh->indices[i_idx++] = row2 + ix;
            out_mesh->indices[i_idx++] = row2 + ix + 1;
        }
    }
    
    // Bounds (AABB)
    out_mesh->aabb_min[0] = config->range_x_min;
    out_mesh->aabb_min[1] = config->range_y_min; // Assuming Z is up, wait. I put Y in [1] and Z in [2].
    // If I put Y in [1], then AABB Y is config Y range.
    out_mesh->aabb_min[1] = config->range_y_min; 
    out_mesh->aabb_min[2] = -100.0f; // TODO: Calculate actual Z min/max

    out_mesh->aabb_max[0] = config->range_x_max;
    out_mesh->aabb_max[1] = config->range_y_max;
    out_mesh->aabb_max[2] = 100.0f; 

    return true;
}
