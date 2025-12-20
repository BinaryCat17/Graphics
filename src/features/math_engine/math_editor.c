#include "math_editor.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/meta/reflection.h"
#include "features/math_engine/internal/transpiler.h"
#include "engine/graphics/internal/renderer_backend.h"
#include "engine/text/font.h"
#include "engine/graphics/render_system.h"
#include "engine/input/input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- Helper: Text Measurement for UI Layout ---
static float text_measure_wrapper(const char* text, void* user_data) {
    (void)user_data;
    return font_measure_text(text);
}

// --- View Model Management ---

// static MathNodeView* math_editor_find_view(MathEditorState* state, MathNodeId id) {
//     for(uint32_t i=0; i<state->node_view_count; ++i) {
//         if(state->node_views[i].node_id == id) return &state->node_views[i];
//     }
//     return NULL;
// }

static MathNodeView* math_editor_add_view(MathEditorState* state, MathNodeId id, float x, float y) {
    if (state->node_view_count >= state->node_view_cap) {
        uint32_t new_cap = state->node_view_cap ? state->node_view_cap * 2 : 16;
        MathNodeView* new_arr = arena_alloc_zero(&state->graph_arena, new_cap * sizeof(MathNodeView));
        if (state->node_views) {
            memcpy(new_arr, state->node_views, state->node_view_count * sizeof(MathNodeView));
        }
        state->node_views = new_arr;
        state->node_view_cap = new_cap;
    }
    MathNodeView* view = &state->node_views[state->node_view_count++];
    view->node_id = id;
    view->x = x;
    view->y = y;
    return view;
}

static void math_editor_sync_view_data(MathEditorState* state) {
    for(uint32_t i=0; i<state->node_view_count; ++i) {
        MathNodeView* view = &state->node_views[i];
        MathNode* node = math_graph_get_node(&state->graph, view->node_id);
        if(node) {
            // One-way binding: Logic -> View
            strncpy(view->name, node->name, 31);
            view->value = node->value;
        }
    }
}

// --- Recompilation Logic ---

static void math_editor_recompile_graph(MathEditorState* state, RenderSystem* rs) {
    if (!state || !rs) return;

    LOG_INFO("Editor: Recompiling Math Graph...");

    // 1. Transpile to GLSL
    char* glsl = math_graph_transpile(&state->graph, TRANSPILE_MODE_IMAGE_2D, SHADER_TARGET_GLSL_VULKAN);
    if (!glsl) {
        LOG_ERROR("Transpilation failed.");
        return;
    }

    // 2. Create Pipeline (Compiles internally)
    uint32_t new_pipe = render_system_create_compute_pipeline_from_source(rs, glsl);
    free(glsl);

    if (new_pipe == 0) {
        LOG_ERROR("Failed to create compute pipeline");
        return;
    }

    // 3. Swap
    if (state->current_pipeline > 0) {
        render_system_destroy_compute_pipeline(rs, state->current_pipeline);
    }
    state->current_pipeline = new_pipe;
    render_system_set_compute_pipeline(rs, new_pipe);
    
    LOG_INFO("Editor: Graph Recompiled Successfully (ID: %u)", new_pipe);
}

// --- UI Regeneration Logic (Imperative -> Declarative Bridge) ---

static void math_editor_refresh_graph_view(MathEditorState* state) {
    UiElement* root = ui_instance_get_root(state->ui_instance);
    if (!root) return;
    
    // Sync data before rebuild
    math_editor_sync_view_data(state);

    UiElement* canvas = ui_element_find_by_id(root, "canvas_area");
    if (canvas) {
        // Declarative Refresh
        ui_element_rebuild_children(canvas, state->ui_instance);
    }
}

static void math_editor_refresh_inspector(MathEditorState* state) {
    UiElement* root = ui_instance_get_root(state->ui_instance);
    if (!root) return;
    UiElement* inspector = ui_element_find_by_id(root, "inspector_area");
    if (!inspector) return;
    
    // Clear previous
    ui_element_clear_children(inspector, state->ui_instance);
    
    if (state->selected_node_id == MATH_NODE_INVALID_ID) {
        return;
    }
    
    MathNode* node = math_graph_get_node(&state->graph, state->selected_node_id);
    if (!node) {
        return;
    }
    
    const MetaStruct* node_meta = meta_get_struct("MathNode");
    
    // Title
    UiNodeSpec* title_spec = ui_asset_get_template(state->ui_asset, "InspectorTitle");
    if (title_spec) {
        UiElement* title = ui_element_create(state->ui_instance, title_spec, node, node_meta);
        ui_element_add_child(inspector, title);
    }
    
    // Value Editor
    if (node->type == MATH_NODE_VALUE) {
        UiNodeSpec* field_spec = ui_asset_get_template(state->ui_asset, "InspectorField");
        if (field_spec) {
            UiElement* field = ui_element_create(state->ui_instance, field_spec, node, node_meta);
            ui_element_add_child(inspector, field);
        }
    }
}

// --- Commands ---

static void cmd_add_node(void* user_data, UiElement* target) {
    (void)target;
    MathEditorState* state = (MathEditorState*)user_data;
    LOG_INFO("Command: Graph.AddNode");
    
    MathNodeId id = math_graph_add_node(&state->graph, MATH_NODE_VALUE);
    math_editor_add_view(state, id, 100, 100);
    
    math_editor_refresh_graph_view(state);
}

static void cmd_clear_graph(void* user_data, UiElement* target) {
    (void)target;
    (void)user_data;
    LOG_INFO("Command: Graph.Clear");
    // TODO: Implement proper clear
}

static void cmd_recompile(void* user_data, UiElement* target) {
    (void)target;
    MathEditorState* state = (MathEditorState*)user_data;
    state->graph_dirty = true;
}

// --- Lifecycle ---

static void math_editor_setup_default_graph(MathEditorState* state) {
    LOG_INFO("Editor: Setting up default Math Graph...");
    
    MathNodeId uv_id = math_graph_add_node(&state->graph, MATH_NODE_UV);
    MathNode* uv = math_graph_get_node(&state->graph, uv_id);
    if(uv) { math_graph_set_name(&state->graph, uv_id, "UV.x"); }
    math_editor_add_view(state, uv_id, 50, 100);
    
    MathNodeId freq_id = math_graph_add_node(&state->graph, MATH_NODE_VALUE);
    MathNode* freq = math_graph_get_node(&state->graph, freq_id);
    if(freq) { math_graph_set_name(&state->graph, freq_id, "Frequency"); freq->value = 20.0f; }
    math_editor_add_view(state, freq_id, 50, 250);
    
    MathNodeId mul_id = math_graph_add_node(&state->graph, MATH_NODE_MUL);
    MathNode* mul = math_graph_get_node(&state->graph, mul_id);
    if(mul) { math_graph_set_name(&state->graph, mul_id, "Multiply"); }
    math_editor_add_view(state, mul_id, 250, 175);
    
    MathNodeId sin_id = math_graph_add_node(&state->graph, MATH_NODE_SIN);
    MathNode* s = math_graph_get_node(&state->graph, sin_id);
    if(s) { math_graph_set_name(&state->graph, sin_id, "Sin"); }
    math_editor_add_view(state, sin_id, 450, 175);
    
    math_graph_connect(&state->graph, mul_id, 0, uv_id); 
    math_graph_connect(&state->graph, mul_id, 1, freq_id); 
    math_graph_connect(&state->graph, sin_id, 0, mul_id);
    
    math_editor_sync_view_data(state);
}

void math_editor_init(MathEditorState* state, Engine* engine) {
    // 1. Init Memory
    arena_init(&state->graph_arena, 1024 * 1024); // 1MB for Graph Data
    math_graph_init(&state->graph, &state->graph_arena);
    state->node_views = NULL;
    state->node_view_count = 0;
    state->node_view_cap = 0;
    
    state->selected_node_id = MATH_NODE_INVALID_ID;
    
    // 2. Setup Default Data
    math_editor_setup_default_graph(state);

    // 3. Init UI System
    ui_command_init();
    ui_command_register("Graph.AddNode", cmd_add_node, state);
    ui_command_register("Graph.Clear", cmd_clear_graph, state);
    ui_command_register("Graph.Recompile", cmd_recompile, state);

    state->input_ctx = ui_input_create();
    state->ui_instance = ui_instance_create(1024 * 1024); // 1MB for UI Elements

    // 4. Load UI Asset
    const char* ui_path = engine_get_config(engine)->ui_path; // Use config path
    if (ui_path) {
        state->ui_asset = ui_parser_load_from_file(ui_path);
        if (state->ui_asset) {
            // NOTE: We now bind MathEditorState, not MathGraph!
            const MetaStruct* editor_meta = meta_get_struct("MathEditorState");
            if (!editor_meta) {
                 LOG_ERROR("MathEditorState meta not found! Did you run codegen?");
            }
            
            // Build Static UI from Asset into Instance
            UiElement* root = ui_element_create(state->ui_instance, ui_asset_get_root(state->ui_asset), state, editor_meta);
            ui_instance_set_root(state->ui_instance, root);
            
            // Initial Select
            if (state->node_view_count > 0) {
                state->selected_node_id = state->node_views[0].node_id;
            }
        } else {
            LOG_ERROR("Failed to load UI asset: %s", ui_path);
        }
    }

    // 5. Initial Compute Compile
    engine_set_show_compute(engine, true);
    render_system_set_show_compute(engine_get_render_system(engine), true);
    math_editor_recompile_graph(state, engine_get_render_system(engine));

    // 6. Input Mappings
    InputSystem* input = engine_get_input_system(engine);
    if (input) {
        input_map_action(input, "ToggleCompute", INPUT_KEY_C, INPUT_MOD_NONE);
    }
}

void math_editor_render(MathEditorState* state, Scene* scene, const Assets* assets, MemoryArena* arena) {
    UiElement* root = ui_instance_get_root(state->ui_instance);
    if (!state || !scene || !root) return;
    
    // Render UI Tree to Scene
    ui_instance_render(state->ui_instance, scene, assets, arena);
}

void math_editor_update(MathEditorState* state, Engine* engine) {
    if (!state) return;
    
    // Sync Logic -> View (one way binding for visual updates)
    math_editor_sync_view_data(state);

    // Toggle Visualizer (Hotkey C) - Action Based
    if (input_is_action_just_pressed(engine_get_input_system(engine), "ToggleCompute")) {
         bool show = !engine_get_show_compute(engine);
         engine_set_show_compute(engine, show);
         render_system_set_show_compute(engine_get_render_system(engine), show);
         if (show) {
             state->graph_dirty = true; 
         }
    }

    UiElement* root = ui_instance_get_root(state->ui_instance);

    // UI Update Loop
    if (root) {
        // Animation / Logic Update
        ui_element_update(root, engine_get_dt(engine));
        
        // Input Handling
        ui_input_update(state->input_ctx, root, engine_get_input_system(engine));
        
        // Process Events
        UiEvent evt;
        while (ui_input_pop_event(state->input_ctx, &evt)) {
            switch (evt.type) {
                case UI_EVENT_VALUE_CHANGE:
                case UI_EVENT_DRAG_END:
                    state->graph_dirty = true;
                    // If drag end was on a view node, update its x/y is automatic via UI binding,
                    // but we don't need to sync back to logic because logic doesn't have x/y anymore!
                    // This is the beauty of separation.
                    break;
                case UI_EVENT_CLICK: {
                    // Selection Logic
                    UiElement* hit = evt.target;
                    while (hit) {
                        // Check for MathNodeView
                        void* data = ui_element_get_data(hit);
                        const MetaStruct* meta = ui_element_get_meta(hit);

                        if (data && meta && strcmp(meta->name, "MathNodeView") == 0) {
                            MathNodeView* v = (MathNodeView*)data;
                            state->selected_node_id = v->node_id;
                            state->selection_dirty = true;
                            LOG_INFO("Selected Node: %d", v->node_id);
                            break;
                        }
                        hit = ui_element_get_parent(hit);
                    }
                } break;
                default: break;
            }
        }
        
        // Lazy Inspector Rebuild
        if (state->selection_dirty) {
            math_editor_refresh_inspector(state);
            state->selection_dirty = false;
        }
        
        // Layout
        PlatformWindowSize size = platform_get_framebuffer_size(engine_get_window(engine));
        ui_instance_layout(state->ui_instance, (float)size.width, (float)size.height, render_system_get_frame_count(engine_get_render_system(engine)), text_measure_wrapper, NULL);
    }

    // Graph Evaluation (Naive interpretation on CPU for debugging/node values)
    for (uint32_t i = 0; i < state->graph.node_count; ++i) {
        const MathNode* n = math_graph_get_node(&state->graph, i);
        if (n && n->type != MATH_NODE_NONE) {
            math_graph_evaluate(&state->graph, i);
        }
    }

    // Recompile Compute Shader if dirty
    if (state->graph_dirty && engine_get_show_compute(engine)) {
        math_editor_recompile_graph(state, engine_get_render_system(engine));
        state->graph_dirty = false;
    }
}

void math_editor_shutdown(MathEditorState* state, Engine* engine) {
    (void)engine;
    if (!state) return;
    
    ui_input_destroy(state->input_ctx);
    ui_instance_free(state->ui_instance);
    arena_destroy(&state->graph_arena);
    
    if (state->ui_asset) ui_asset_free(state->ui_asset);
}
