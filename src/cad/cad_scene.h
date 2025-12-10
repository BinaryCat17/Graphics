#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Basic material properties parsed from the YAML scene. */
typedef struct Material {
    char *id;
    float density;
    float young_modulus;
    float poisson_ratio;
} Material;

typedef enum GeometryKind {
    GEO_PRIMITIVE,
    GEO_BOOLEAN,
    GEO_SKETCH,
    GEO_STEP,
    GEO_KIND_NONE,
} GeometryKind;

typedef enum GeometryPrimitiveType {
    GEO_PRIM_BOX,
    GEO_PRIM_CYLINDER,
    GEO_PRIM_SPHERE,
    GEO_PRIM_EXTRUDE,
} GeometryPrimitiveType;

typedef enum GeometryBooleanType {
    GEO_BOOL_UNION,
    GEO_BOOL_DIFFERENCE,
    GEO_BOOL_INTERSECTION,
} GeometryBooleanType;

typedef struct GeometryPrimitive {
    GeometryPrimitiveType type;
    float size[3];
    float radius;
    float height;
    float fillet;
} GeometryPrimitive;

typedef struct GeometryBooleanNode {
    GeometryBooleanType op;
    struct GeometryNode *left;
    struct GeometryNode *right;
} GeometryBooleanNode;

typedef struct GeometrySketch {
    char *path;
} GeometrySketch;

typedef struct GeometryStep {
    char *path;
    float scale;
} GeometryStep;

typedef struct GeometryNode {
    GeometryKind kind;
    union {
        GeometryPrimitive primitive;
        GeometryBooleanNode boolean;
        GeometrySketch sketch;
        GeometryStep step;
    } data;
} GeometryNode;

typedef struct Part {
    char *id;
    Material *material;
    GeometryNode *geometry;
    float transform[16];
} Part;

typedef enum JointType {
    JOINT_REVOLUTE,
    JOINT_PRISMATIC,
    JOINT_FIXED,
} JointType;

typedef struct Joint {
    char *id;
    Part *parent;
    Part *child;
    JointType type;
    float origin[3];
    float axis[3];
} Joint;

typedef struct AssemblyNode {
    Part *part;
    Joint *via_joint;
    struct AssemblyNode *children;
    size_t child_count;
} AssemblyNode;

typedef struct Assembly {
    char *id;
    AssemblyNode root;
} Assembly;

typedef struct LoadVector {
    float force[3];
    int has_force;
    float moment[3];
    int has_moment;
    float point[3];
    int has_point;
    int fixed;
} LoadVector;

typedef struct LoadCase {
    char *id;
    Part **targets;
    LoadVector *loads;
    size_t load_count;
} LoadCase;

typedef struct MotionProfile {
    char *id;
    Joint *joint;
    char *type;
    float start;
    float end;
    float v_max;
    float amplitude;
    float frequency;
} MotionProfile;

typedef struct SceneUnits {
    float length_scale;
    float angle_scale;
} SceneUnits;

typedef struct SceneMetadata {
    char *name;
    char *author;
} SceneMetadata;

typedef struct Scene {
    int version;
    SceneMetadata metadata;
    SceneUnits units;
    Material *materials;
    size_t material_count;
    Part *parts;
    size_t part_count;
    Joint *joints;
    size_t joint_count;
    Assembly *assemblies;
    size_t assembly_count;
    LoadCase *analysis;
    size_t analysis_count;
    MotionProfile *motion_profiles;
    size_t motion_count;
} Scene;

typedef struct SceneError {
    int line;
    int column;
    char message[128];
} SceneError;

/** Release all memory owned by the scene. */
void scene_dispose(Scene *scene);

/** Simple triangle mesh representation used by STEP loader. */
typedef struct Mesh {
    float *positions; // xyz triplets
    size_t position_count;
    unsigned int *indices;
    size_t index_count;
    float aabb_min[3];
    float aabb_max[3];
} Mesh;

typedef struct MeshError {
    int line;
    int column;
    char message[128];
} MeshError;

int load_step_mesh(const char *path, float scale, Mesh *out, MeshError *err);
void mesh_dispose(Mesh *mesh);

#ifdef __cplusplus
}
#endif

