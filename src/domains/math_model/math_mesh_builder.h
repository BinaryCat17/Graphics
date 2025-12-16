#ifndef MATH_MESH_BUILDER_H
#define MATH_MESH_BUILDER_H

#include "domains/math_model/math_scene.h"
#include "domains/cad_model/cad_scene.h" // For Mesh struct

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MathMeshConfig {
    int grid_resolution_x; // e.g., 100
    int grid_resolution_y; // e.g., 100
    float range_x_min;
    float range_x_max;
    float range_y_min;
    float range_y_max;
} MathMeshConfig;

/**
 * Generates a 3D mesh from a MathScene visualizer node.
 * Supports "surface" type nodes (z = f(x, y)).
 * 
 * @param scene The math scene containing the graph.
 * @param visual_node The node of type MATH_NODE_VISUALIZER to render.
 * @param config Grid configuration.
 * @param out_mesh Pointer to an existing Mesh struct to populate. 
 *                 Caller is responsible for disposing the mesh later using mesh_dispose (or manually freeing arrays).
 * @return true on success.
 */
bool math_mesh_build_surface(MathScene* scene, MathNode* visual_node, const MathMeshConfig* config, Mesh* out_mesh);

#ifdef __cplusplus
}
#endif

#endif // MATH_MESH_BUILDER_H
