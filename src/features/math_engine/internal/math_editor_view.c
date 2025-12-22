#include "math_editor_view.h"
#include "math_editor_internal.h"
#include "math_graph_internal.h"
#include "engine/scene/scene.h"
#include "foundation/logger/logger.h"
#include <string.h>

// --- View Model Management ---

MathNodeView* math_editor_add_view(MathEditor* editor, MathNodeId id, float x, float y) {
    if (editor->node_views_count >= editor->node_view_cap) {
        uint32_t new_cap = editor->node_view_cap ? editor->node_view_cap * 2 : 16;
        MathNodeView* new_arr = arena_alloc_zero(&editor->graph_arena, new_cap * sizeof(MathNodeView));
        if (editor->node_views) {
            memcpy(new_arr, editor->node_views, editor->node_views_count * sizeof(MathNodeView));
        }
        editor->node_views = new_arr;
        editor->node_view_cap = new_cap;
    }
    MathNodeView* view = &editor->node_views[editor->node_views_count++];
    view->node_id = id;
    view->x = x;
    view->y = y;
    return view;
}

MathNodeView* math_editor_find_view(MathEditor* editor, MathNodeId id) {
    for(uint32_t i=0; i<editor->node_views_count; ++i) {
        if (editor->node_views[i].node_id == id) {
            return &editor->node_views[i];
        }
    }
    return NULL;
}

void math_editor_sync_view_data(MathEditor* editor) {
    for(uint32_t i=0; i<editor->node_views_count; ++i) {
        MathNodeView* view = &editor->node_views[i];
        // Use internal accessor (visible via math_graph_internal.h)
        MathNode* node = math_graph_get_node(editor->graph, view->node_id);
        if(node) {
            // One-way binding: Logic -> View
            strncpy(view->name, node->name, 31);
            view->value = node->value;
        }
    }
}

// --- UI Sync ---

void math_editor_refresh_graph_view(MathEditor* editor) {
    UiElement* root = ui_instance_get_root(editor->ui_instance);
    if (!root) return;
    
    // Sync data before rebuild
    math_editor_sync_view_data(editor);

    UiElement* canvas = ui_element_find_by_id(root, "canvas_area");
    if (canvas) {
        // Declarative Refresh
        ui_element_rebuild_children(canvas, editor->ui_instance);
    }
}

void math_editor_update_selection(MathEditor* editor) {
    if (!editor) return;

    // 1. Update ViewModel (Selection Array)
    editor->selected_nodes_count = 0;
    if (editor->selected_node_id != MATH_NODE_INVALID_ID) {
        MathNode* node = math_graph_get_node(editor->graph, editor->selected_node_id);
        if (node) {
            editor->selected_nodes[0] = node;
            editor->selected_nodes_count = 1;
        }
    }
    
    editor->has_selection = (editor->selected_nodes_count > 0);
    editor->no_selection = !editor->has_selection;

    // 2. Trigger UI Rebuild for Inspector
    UiElement* root = ui_instance_get_root(editor->ui_instance);
    if (root) {
        UiElement* inspector = ui_element_find_by_id(root, "inspector_area");
        if (inspector) {
             ui_element_rebuild_children(inspector, editor->ui_instance);
        }
    }
}

// --- Rendering Logic ---

// Z-Layer Offsets
#define LAYER_OFFSET_WIRE    0.005f
#define LAYER_OFFSET_PORT    0.020f

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

void math_editor_sync_wires(MathEditor* editor) {
    if (!editor || !editor->wires) return;

    editor->wires_count = 0;

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

            if (editor->wires_count >= editor->wires_cap) break;

            MathWireView* wire = &editor->wires[editor->wires_count++];
            
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

static void math_editor_render_ports(MathEditor* editor, Scene* scene, Vec4 clip_rect, float base_z) {
    if (!editor || !editor->graph || !scene) return;

    for (uint32_t i = 0; i < editor->node_views_count; ++i) {
        MathNodeView* view = &editor->node_views[i];
        MathNode* node = math_graph_get_node(editor->graph, view->node_id);
        if (!node) continue;

        int input_count = get_node_input_count(node->type);
        
        // Render Inputs
        for (int k = 0; k < input_count; ++k) {
            float x = view->x + clip_rect.x;
            float y = view->y + NODE_HEADER_HEIGHT + (k * NODE_PORT_SPACING) + clip_rect.y;
            
            // Center of the port
            Vec3 center = {x + NODE_PORT_SIZE * 0.5f, y + NODE_PORT_SIZE * 0.5f, base_z + LAYER_OFFSET_PORT};
            
            scene_push_circle_sdf(scene, center, NODE_PORT_SIZE * 0.5f, (Vec4){0.5f, 0.5f, 0.5f, 1.0f}, clip_rect);
        }

        // Render Output
        if (node->type != MATH_NODE_OUTPUT) {
            float x = view->x + NODE_WIDTH + clip_rect.x;
            float y = view->y + NODE_HEADER_HEIGHT + clip_rect.y;
            
            Vec3 center = {x + NODE_PORT_SIZE * 0.5f, y + NODE_PORT_SIZE * 0.5f, base_z + LAYER_OFFSET_PORT};
            
            scene_push_circle_sdf(scene, center, NODE_PORT_SIZE * 0.5f, (Vec4){0.5f, 0.5f, 0.5f, 1.0f}, clip_rect);
        }
    }
}

void math_graph_view_provider(void* instance_data, Rect screen_rect, float z_depth, Scene* scene, MemoryArena* arena) {
    (void)arena; // Unused
    MathEditor* editor = (MathEditor*)instance_data;
    if (!editor) return;
    
    Vec4 clip_vec = {screen_rect.x, screen_rect.y, screen_rect.w, screen_rect.h};
    
    // Render Ports (Immediate mode on top of UI)
    math_editor_render_ports(editor, scene, clip_vec, z_depth);
}
