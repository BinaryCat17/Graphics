#include "math_editor_view.h"
#include "math_editor_internal.h"
#include "math_graph_internal.h"
#include "engine/scene/render_packet.h"
#include "foundation/logger/logger.h"
#include <string.h>

// --- View Model Management ---

MathNodeView* math_editor_add_view(MathEditor* editor, MathNodeId id, float x, float y) {
    if (editor->view->node_views_count >= editor->view->node_view_cap) {
        uint32_t new_cap = editor->view->node_view_cap ? editor->view->node_view_cap * 2 : 16;
        MathNodeView* new_arr = arena_alloc_zero(&editor->graph_arena, new_cap * sizeof(MathNodeView));
        if (editor->view->node_views) {
            memcpy(new_arr, editor->view->node_views, editor->view->node_views_count * sizeof(MathNodeView));
        }
        editor->view->node_views = new_arr;
        editor->view->node_view_cap = new_cap;
    }
    MathNodeView* view = &editor->view->node_views[editor->view->node_views_count++];
    view->node_id = id;
    view->x = x;
    view->y = y;
    return view;
}

MathNodeView* math_editor_find_view(MathEditor* editor, MathNodeId id) {
    for(uint32_t i=0; i<editor->view->node_views_count; ++i) {
        if (editor->view->node_views[i].node_id == id) {
            return &editor->view->node_views[i];
        }
    }
    return NULL;
}

static int get_node_input_count(MathNodeType type) {
    switch (type) {
        case MATH_NODE_ADD: 
        case MATH_NODE_SUB: 
        case MATH_NODE_MUL: 
        case MATH_NODE_DIV: return 2;
        case MATH_NODE_SIN: 
        case MATH_NODE_COS: 
        case MATH_NODE_OUTPUT: return 1;
        case MATH_NODE_UV:  
        case MATH_NODE_VALUE: 
        case MATH_NODE_TIME: 
        default: return 0;
    }
}

void math_editor_sync_view_data(MathEditor* editor) {
    for(uint32_t i=0; i<editor->view->node_views_count; ++i) {
        MathNodeView* view = &editor->view->node_views[i];
        // Use internal accessor (visible via math_graph_internal.h)
        MathNode* node = math_graph_get_node(editor->graph, view->node_id);
        if(node) {
            // One-way binding: Logic -> View
            strncpy(view->name, node->name, 31);
            view->value = node->value;

            // Sync Inputs
            int input_count = get_node_input_count(node->type);
            view->input_ports_count = input_count;
            for(int k=0; k<input_count; ++k) {
                view->input_ports[k].index = k;
            }

            // Sync Outputs (Most nodes have 1 output, except OUTPUT node)
            if (node->type != MATH_NODE_OUTPUT) {
                view->output_ports_count = 1;
                view->output_ports[0].index = -1; 
            } else {
                view->output_ports_count = 0;
            }
        }
    }
}

// --- UI Sync ---

void math_editor_refresh_graph_view(MathEditor* editor) {
    SceneNode* root = scene_tree_get_root(editor->view->ui_instance);
    if (!root) return;
    
    // Sync data before rebuild
    math_editor_sync_view_data(editor);

    SceneNode* canvas = scene_node_find_by_id(root, "canvas_area");
    if (canvas) {
        // Declarative Refresh
        ui_node_rebuild_children(canvas, editor->view->ui_instance);
    }
}

void math_editor_update_selection(MathEditor* editor) {
    if (!editor) return;

    // 1. Update ViewModel (Selection Array)
    editor->view->selected_nodes_count = 0;
    if (editor->view->selected_node_id != MATH_NODE_INVALID_ID) {
        MathNode* node = math_graph_get_node(editor->graph, editor->view->selected_node_id);
        if (node) {
            editor->view->selected_nodes[0] = node;
            editor->view->selected_nodes_count = 1;
        }
    }
    
    editor->view->has_selection = (editor->view->selected_nodes_count > 0);
    editor->view->no_selection = !editor->view->has_selection;

    // 2. Trigger UI Rebuild for Inspector
    SceneNode* root = scene_tree_get_root(editor->view->ui_instance);
    if (root) {
        SceneNode* inspector = scene_node_find_by_id(root, "inspector_area");
        if (inspector) {
             ui_node_rebuild_children(inspector, editor->view->ui_instance);
        }
    }
}

// --- Rendering Logic ---

// Z-Layer Offsets
#define LAYER_OFFSET_WIRE    0.005f
#define LAYER_OFFSET_PORT    0.020f

void math_editor_sync_wires(MathEditor* editor) {
    if (!editor || !editor->view->wires) return;

    editor->view->wires_count = 0;

    for (uint32_t i = 0; i < editor->graph->node_count; ++i) {
        MathNode* target_node = math_graph_get_node(editor->graph, i);
        if (!target_node || target_node->type == MATH_NODE_NONE) continue;
        
        MathNodeView* target_view = math_editor_find_view(editor, target_node->id);
        if (!target_view) continue;

        for (int k = 0; k < MATH_NODE_MAX_INPUTS; ++k) {
            MathNodeId source_id = target_node->inputs[k];
            if (source_id == MATH_NODE_INVALID_ID) continue;
            
            MathNodeView* source_view = math_editor_find_view(editor, source_id);
            if (!source_view) continue;

            if (editor->view->wires_count >= editor->view->wires_cap) break;

            MathWireView* wire = &editor->view->wires[editor->view->wires_count++];
            
            // Output Port is usually on the right side of the node
            float start_x = source_view->x + NODE_WIDTH + NODE_PORT_SIZE * 0.5f;
            float start_y = source_view->y + NODE_HEADER_HEIGHT + NODE_PORT_SIZE * 0.5f;

            // Input Port is on the left side, offset by index K
            float end_x = target_view->x + NODE_PORT_SIZE * 0.5f;
            float end_y = target_view->y + NODE_HEADER_HEIGHT + (k * NODE_PORT_SPACING) + NODE_PORT_SIZE * 0.5f;
            
            wire->start = (Vec2){start_x, start_y};
            wire->end = (Vec2){end_x, end_y};
            wire->thickness = 3.0f;
            wire->color = (Vec4){0.8f, 0.8f, 0.8f, 1.0f};
        }
    }
}
