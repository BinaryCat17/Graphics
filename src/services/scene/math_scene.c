#include "math_scene.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// --- Helper Functions ---

static void free_node(MathNode* node) {
    if (!node) return;
    
    if (node->label) free(node->label);
    if (node->inputs) free(node->inputs);
    
    // Free specific data if needed
    if (node->type == MATH_NODE_VARIABLE) {
        if (node->data.variable.name) free(node->data.variable.name);
    } else if (node->type == MATH_NODE_FUNCTION) {
        if (node->data.func.func_name) free(node->data.func.func_name);
    } else if (node->type == MATH_NODE_VISUALIZER) {
        if (node->data.visual.visual_type) free(node->data.visual.visual_type);
    }

    free(node);
}

// --- Scene Lifecycle ---

MathScene* math_scene_create(void) {
    MathScene* scene = (MathScene*)malloc(sizeof(MathScene));
    if (!scene) return NULL;

    scene->node_capacity = 16;
    scene->node_count = 0;
    scene->nodes = (MathNode**)malloc(sizeof(MathNode*) * scene->node_capacity);
    
    scene->time = 0.0f;
    scene->time_step = 0.016f; // 60 FPS approx
    scene->is_playing = false;

    return scene;
}

void math_scene_dispose(MathScene* scene) {
    if (!scene) return;

    for (size_t i = 0; i < scene->node_count; ++i) {
        free_node(scene->nodes[i]);
    }
    
    free(scene->nodes);
    free(scene);
}

// --- Node Management ---

MathNode* math_scene_add_node(MathScene* scene, MathNodeType type, const char* label) {
    if (scene->node_count >= scene->node_capacity) {
        size_t new_cap = scene->node_capacity * 2;
        MathNode** new_nodes = (MathNode**)realloc(scene->nodes, sizeof(MathNode*) * new_cap);
        if (!new_nodes) return NULL;
        scene->nodes = new_nodes;
        scene->node_capacity = new_cap;
    }

    MathNode* node = (MathNode*)malloc(sizeof(MathNode));
    if (!node) return NULL;

    memset(node, 0, sizeof(MathNode));
    node->id = (unsigned int)(scene->node_count + 1); // Simple ID generation
    node->type = type;
    if (label) node->label = strdup(label);

    // Default connection capacity
    node->input_capacity = 2; // Most ops are binary
    node->inputs = (MathConnection*)malloc(sizeof(MathConnection) * node->input_capacity);
    node->input_count = 0;

    scene->nodes[scene->node_count++] = node;
    return node;
}

MathNode* math_scene_find_node(MathScene* scene, const char* label) {
    if (!label) return NULL;
    for (size_t i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i]->label && strcmp(scene->nodes[i]->label, label) == 0) {
            return scene->nodes[i];
        }
    }
    return NULL;
}

bool math_scene_remove_node(MathScene* scene, unsigned int id) {
    // Basic implementation: find, free, and shift array
    // Note: This leaves dangling connections in other nodes! 
    // A robust implementation would scan all nodes and remove connections to this ID.
    for (size_t i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i]->id == id) {
            free_node(scene->nodes[i]);
            
            // Shift remaining
            for (size_t j = i; j < scene->node_count - 1; ++j) {
                scene->nodes[j] = scene->nodes[j+1];
            }
            scene->node_count--;
            return true;
        }
    }
    return false;
}

// --- Connection Management ---

bool math_scene_connect(MathNode* source, MathNode* destination, int dest_input_index) {
    if (!source || !destination) return false;

    // Expand inputs if necessary
    if (dest_input_index >= (int)destination->input_capacity) {
        // ... realloc logic omitted for brevity, assuming small fixed inputs for now ...
        // In a real system, we'd resize the array.
        return false; 
    }

    // Ensure connection count encompasses this index
    if (dest_input_index >= (int)destination->input_count) {
        destination->input_count = dest_input_index + 1;
    }

    destination->inputs[dest_input_index].target_node = source;
    destination->inputs[dest_input_index].target_output_index = 0; // Default to 0
    destination->inputs[dest_input_index].input_index = dest_input_index;

    return true;
}

// --- Evaluation ---

float math_node_eval(MathNode* node) {
    if (!node) return 0.0f;

    switch (node->type) {
        case MATH_NODE_CONSTANT:
            return node->data.constant.value;

        case MATH_NODE_VARIABLE:
            return node->data.variable.current_value;

        case MATH_NODE_OPERATOR: {
            float left = 0.0f, right = 0.0f;
            if (node->input_count > 0 && node->inputs[0].target_node) 
                left = math_node_eval(node->inputs[0].target_node);
            if (node->input_count > 1 && node->inputs[1].target_node) 
                right = math_node_eval(node->inputs[1].target_node);

            char op = node->data.op.op_symbol;
            if (op == '+') return left + right;
            if (op == '-') return left - right;
            if (op == '*') return left * right;
            if (op == '/') return (right != 0.0f) ? left / right : 0.0f;
            return 0.0f;
        }

        case MATH_NODE_FUNCTION: {
            float arg = 0.0f;
            if (node->input_count > 0 && node->inputs[0].target_node)
                arg = math_node_eval(node->inputs[0].target_node);
            
            if (strcmp(node->data.func.func_name, "sin") == 0) return sinf(arg);
            if (strcmp(node->data.func.func_name, "cos") == 0) return cosf(arg);
            if (strcmp(node->data.func.func_name, "tan") == 0) return tanf(arg);
            if (strcmp(node->data.func.func_name, "abs") == 0) return fabsf(arg);
            
            return 0.0f;
        }

        default:
            return 0.0f;
    }
}

// --- Simulation ---

void math_scene_update(MathScene* scene, float delta_time) {
    if (!scene) return;

    if (scene->is_playing) {
        scene->time += delta_time;
    }

    // Update 'time' variable nodes
    for (size_t i = 0; i < scene->node_count; ++i) {
        MathNode* n = scene->nodes[i];
        if (n->type == MATH_NODE_VARIABLE) {
            if (n->data.variable.name && strcmp(n->data.variable.name, "t") == 0) {
                n->data.variable.current_value = scene->time;
            }
        }
    }
}
