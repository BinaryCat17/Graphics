#ifndef MATH_SCENE_H
#define MATH_SCENE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Enums & Types ---

typedef enum MathNodeType {
    MATH_NODE_CONSTANT,      // e.g., 3.14, 42
    MATH_NODE_VARIABLE,      // e.g., 't', 'x', 'user_param'
    MATH_NODE_OPERATOR,      // e.g., +, -, *, /
    MATH_NODE_FUNCTION,      // e.g., sin, cos, exp, custom_func
    MATH_NODE_VISUALIZER,    // e.g., Plot3D, VectorField
    MATH_NODE_COMPUTE        // Intermediate compute buffer
} MathNodeType;

typedef struct MathNode MathNode;

typedef struct MathConnection {
    MathNode* target_node;   // The node providing the input
    int target_output_index; // Which output of the target node (usually 0)
    int input_index;         // Which input slot of the current node this connects to
} MathConnection;

// --- Node Data Variants ---

typedef struct NodeDataConstant {
    float value;
} NodeDataConstant;

typedef struct NodeDataVariable {
    char* name;
    float current_value;     // For simulation/animation
} NodeDataVariable;

typedef struct NodeDataOperator {
    char op_symbol;          // '+', '-', '*', '/'
} NodeDataOperator;

typedef struct NodeDataFunction {
    char* func_name;         // "sin", "abs", etc.
} NodeDataFunction;

typedef struct NodeDataVisualizer {
    char* visual_type;       // "surface", "line", "points"
    bool visible;
    float color[4];          // RGBA
} NodeDataVisualizer;

// --- Main Node Structure ---

struct MathNode {
    unsigned int id;
    char* label;             // User-friendly label or internal ID
    MathNodeType type;
    
    // Connections (Inputs)
    MathConnection* inputs;
    size_t input_count;
    size_t input_capacity;

    // Specific Data
    union {
        NodeDataConstant constant;
        NodeDataVariable variable;
        NodeDataOperator op;
        NodeDataFunction func;
        NodeDataVisualizer visual;
    } data;
    
    // Future: Compiled/Bytecode representation for fast evaluation
    void* compiled_data;
};

// --- Scene Container ---

typedef struct MathScene {
    MathNode** nodes;
    size_t node_count;
    size_t node_capacity;

    // Global simulation state
    float time;
    float time_step;
    bool is_playing;
} MathScene;

// --- API ---

MathScene* math_scene_create(void);
void math_scene_dispose(MathScene* scene);

// Node management
MathNode* math_scene_add_node(MathScene* scene, MathNodeType type, const char* label);
MathNode* math_scene_find_node(MathScene* scene, const char* label);
bool math_scene_remove_node(MathScene* scene, unsigned int id);

// Connection management
bool math_scene_connect(MathNode* source, MathNode* destination, int dest_input_index);

// Evaluation (CPU-side reference implementation)
float math_node_eval(MathNode* node);

// Simulation
void math_scene_update(MathScene* scene, float delta_time);

#ifdef __cplusplus
}
#endif

#endif // MATH_SCENE_H
