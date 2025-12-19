#include "math_editor.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/meta/reflection.h"
#include "features/math_engine/transpiler.h"
#include "engine/graphics/backend/renderer_backend.h"
#include "engine/ui/ui_parser.h"
#include "engine/ui/ui_layout.h"
#include "engine/ui/ui_command_system.h"
#include "engine/graphics/text/font.h"
#include "engine/graphics/backend/vulkan/vk_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- Helper: Text Measurement for UI Layout ---
static float text_measure_wrapper(const char* text, void* user_data) {
    (void)user_data;
    return font_measure_text(text);
}

// --- Helper: Find Element ---
static UiElement* ui_find_element_by_id(UiElement* root, const char* id) {
    if (!root || !root->spec) return NULL;
    if (root->spec->id && strcmp(root->spec->id, id) == 0) return root;
    for (size_t i = 0; i < root->child_count; ++i) {
        UiElement* found = ui_find_element_by_id(root->children[i], id);
        if (found) return found;
    }
    return NULL;
}

// --- Recompilation Logic ---

static void math_editor_recompile_graph(MathEditorState* state, RenderSystem* rs) {
    if (!state || !rs || !rs->backend) return;

    LOG_INFO("Editor: Recompiling Math Graph...");

    // 1. Transpile to GLSL
    char* glsl = math_graph_transpile(&state->graph, TRANSPILE_MODE_IMAGE_2D, SHADER_TARGET_GLSL_VULKAN);
    if (!glsl) {
        LOG_ERROR("Transpilation failed.");
        return;
    }

    // 2. Save to temp file
    const char* tmp_glsl = "logs/tmp_graph.comp";
    FILE* f = fopen(tmp_glsl, "w");
    if (!f) {
        LOG_ERROR("Failed to create temp GLSL file");
        free(glsl);
        return;
    }
    fprintf(f, "%s", glsl);
    fclose(f);
    free(glsl);

    // 3. Call glslc
    const char* tmp_spv = "logs/tmp_graph.spv";
    char cmd[512];
    sprintf(cmd, "glslc %s -o %s", tmp_glsl, tmp_spv);
    
    LOG_DEBUG("Running: %s", cmd);
    int res = system(cmd);
    if (res != 0) {
        LOG_ERROR("glslc failed with code %d", res);
        return;
    }

    // 4. Load SPIR-V
    size_t spv_size = 0;
    uint32_t* spv_code = read_file_bin_u32(tmp_spv, &spv_size);
    if (!spv_code) {
        LOG_ERROR("Failed to read generated SPIR-V");
        return;
    }

    // 5. Create Pipeline
    uint32_t new_pipe = rs->backend->compute_pipeline_create(rs->backend, spv_code, spv_size, 0);
    free(spv_code);

    if (new_pipe == 0) {
        LOG_ERROR("Failed to create compute pipeline");
        return;
    }

    // 6. Swap
    if (state->current_pipeline > 0) {
        rs->backend->compute_pipeline_destroy(rs->backend, state->current_pipeline);
    }
    state->current_pipeline = new_pipe;
    render_system_set_compute_pipeline(rs, new_pipe);
    
    LOG_INFO("Editor: Graph Recompiled Successfully (ID: %u)", new_pipe);
}

// --- UI Regeneration Logic (Imperative -> Declarative Bridge) ---

static void math_editor_refresh_graph_view(MathEditorState* state) {
    if (!state->ui_instance.root) return;
    UiElement* canvas = ui_find_element_by_id(state->ui_instance.root, "canvas_area");
    if (!canvas) return;

    // Reset Input State to avoid stale IDs
    ui_input_reset(&state->input_ctx);

    const MetaStruct* node_meta = meta_get_struct("MathNode");
    
    // Count valid nodes
    int count = 0;
    for (uint32_t i = 0; i < state->graph.node_count; ++i) {
        const MathNode* n = math_graph_get_node(&state->graph, i);
        if (n && n->type != MATH_NODE_NONE) count++;
    }

    // Reallocate children array from Arena
    // Note: We are relying on the linear arena. Old arrays are "leaked" inside the arena until frame reset?
    // Actually, `ui_instance.arena` is a persistent arena for the UI tree. 
    // Ideally, we should reset the arena or use a pool if we rebuild often.
    // For now, we just allocate forward. This WILL eventually OOM if we don't reset.
    // TODO: Implement Arena Reset for dynamic UI parts.
    canvas->children = (UiElement**)arena_alloc_zero(&state->ui_instance.arena, count * sizeof(UiElement*));
    canvas->child_count = count;

    int idx = 0;
    for (uint32_t i = 0; i < state->graph.node_count; ++i) {
        MathNode* node = math_graph_get_node(&state->graph, i);
        if (!node || node->type == MATH_NODE_NONE) continue;
        
        // --- Node Window (FROM TEMPLATE) ---
        UiNodeSpec* node_spec = ui_asset_get_template(state->ui_asset, "GraphNode");
        if (node_spec) {
            // Instantiate from template
            UiElement* container = ui_element_create(&state->ui_instance, node_spec, node, node_meta);
            canvas->children[idx++] = container;
            container->parent = canvas;
            
            // Value Row (Append if needed - dynamic hack)
            if (node->type == MATH_NODE_VALUE) {
                UiNodeSpec* val_spec = ui_asset_get_template(state->ui_asset, "ValueRow");
                if (val_spec) {
                    size_t old_cnt = container->child_count;
                    UiElement** old_children = container->children;
                    
                    container->child_count++;
                    container->children = arena_alloc_zero(&state->ui_instance.arena, container->child_count * sizeof(UiElement*));
                    
                    for(size_t k=0; k<old_cnt; ++k) container->children[k] = old_children[k];
                    
                    UiElement* row = ui_element_create(&state->ui_instance, val_spec, node, node_meta);
                    container->children[old_cnt] = row;
                    row->parent = container;
                }
            }
        }
    }
}

static void math_editor_refresh_inspector(MathEditorState* state) {
    if (!state->ui_instance.root) return;
    UiElement* inspector = ui_find_element_by_id(state->ui_instance.root, "inspector_area");
    if (!inspector) return;
    
    if (state->selected_node_id == MATH_NODE_INVALID_ID) {
        inspector->child_count = 0;
        return;
    }
    
    MathNode* node = math_graph_get_node(&state->graph, state->selected_node_id);
    if (!node) {
        inspector->child_count = 0;
        return;
    }
    
    const MetaStruct* node_meta = meta_get_struct("MathNode");
    
    int ins_count = 1; // Title
    if (node->type == MATH_NODE_VALUE) ins_count += 1; // Field
    
    inspector->children = (UiElement**)arena_alloc_zero(&state->ui_instance.arena, ins_count * sizeof(UiElement*));
    inspector->child_count = 0;
    
    // Title
    UiNodeSpec* title_spec = ui_asset_get_template(state->ui_asset, "InspectorTitle");
    if (title_spec) {
        UiElement* title = ui_element_create(&state->ui_instance, title_spec, node, node_meta);
        inspector->children[inspector->child_count++] = title;
        title->parent = inspector;
    }
    
    // Value Editor
    if (node->type == MATH_NODE_VALUE) {
        UiNodeSpec* field_spec = ui_asset_get_template(state->ui_asset, "InspectorField");
        if (field_spec) {
            UiElement* field = ui_element_create(&state->ui_instance, field_spec, node, node_meta);
            inspector->children[inspector->child_count++] = field;
            field->parent = inspector;
        }
    }
}

// --- Commands ---

static void cmd_add_node(void* user_data, UiElement* target) {
    (void)target;
    MathEditorState* state = (MathEditorState*)user_data;
    LOG_INFO("Command: Graph.AddNode");
    math_graph_add_node(&state->graph, MATH_NODE_VALUE);
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
    if(uv) { math_graph_set_name(&state->graph, uv_id, "UV.x"); uv->ui_x = 50; uv->ui_y = 100; }
    
    MathNodeId freq_id = math_graph_add_node(&state->graph, MATH_NODE_VALUE);
    MathNode* freq = math_graph_get_node(&state->graph, freq_id);
    if(freq) { math_graph_set_name(&state->graph, freq_id, "Frequency"); freq->value = 20.0f; freq->ui_x = 50; freq->ui_y = 250; }
    
    MathNodeId mul_id = math_graph_add_node(&state->graph, MATH_NODE_MUL);
    MathNode* mul = math_graph_get_node(&state->graph, mul_id);
    if(mul) { math_graph_set_name(&state->graph, mul_id, "Multiply"); mul->ui_x = 250; mul->ui_y = 175; }
    
    MathNodeId sin_id = math_graph_add_node(&state->graph, MATH_NODE_SIN);
    MathNode* s = math_graph_get_node(&state->graph, sin_id);
    if(s) { math_graph_set_name(&state->graph, sin_id, "Sin"); s->ui_x = 450; s->ui_y = 175; }
    
    math_graph_connect(&state->graph, mul_id, 0, uv_id); 
    math_graph_connect(&state->graph, mul_id, 1, freq_id); 
    math_graph_connect(&state->graph, sin_id, 0, mul_id);
}

void math_editor_init(MathEditorState* state, Engine* engine) {
    // 1. Init Memory
    arena_init(&state->graph_arena, 1024 * 1024); // 1MB for Graph Data
    math_graph_init(&state->graph, &state->graph_arena);
    
    state->selected_node_id = MATH_NODE_INVALID_ID;
    
    // 2. Setup Default Data
    math_editor_setup_default_graph(state);

    // 3. Init UI System
    ui_command_init();
    ui_command_register("Graph.AddNode", cmd_add_node, state);
    ui_command_register("Graph.Clear", cmd_clear_graph, state);
    ui_command_register("Graph.Recompile", cmd_recompile, state);

    ui_input_init(&state->input_ctx);
    ui_instance_init(&state->ui_instance, 1024 * 1024); // 1MB for UI Elements

    // 4. Load UI Asset
    const char* ui_path = engine->config.ui_path; // Use config path
    if (ui_path) {
        state->ui_asset = ui_parser_load_from_file(ui_path);
        if (state->ui_asset) {
            const MetaStruct* graph_meta = meta_get_struct("MathGraph");
            
            // Build Static UI from Asset into Instance
            state->ui_instance.root = ui_element_create(&state->ui_instance, state->ui_asset->root, &state->graph, graph_meta);

            // Bind to Renderer
            render_system_bind_ui(&engine->render_system, state->ui_instance.root);
            
            // Generate Dynamic Parts
            math_editor_refresh_graph_view(state);
            
            // Select first node (optional convenience)
            for(uint32_t i=0; i<state->graph.node_count; ++i) {
                const MathNode* n = math_graph_get_node(&state->graph, i);
                if(n && n->type != MATH_NODE_NONE) {
                    state->selected_node_id = n->id;
                    break;
                }
            }
        } else {
            LOG_ERROR("Failed to load UI asset: %s", ui_path);
        }
    }

    // 5. Initial Compute Compile
    // Force visualizer on for now
    engine->show_compute_visualizer = true;
    engine->render_system.show_compute_result = true;
    math_editor_recompile_graph(state, &engine->render_system);
}

void math_editor_update(MathEditorState* state, Engine* engine) {
    if (!state) return;

    // Toggle Visualizer (Hotkey C)
    static bool key_c_prev = false;
    bool key_c_curr = platform_get_key(engine->window, 67); // KEY_C
    
    if (key_c_curr && !key_c_prev) {
        engine->show_compute_visualizer = !engine->show_compute_visualizer;
        engine->render_system.show_compute_result = engine->show_compute_visualizer;
        if (engine->show_compute_visualizer) {
            state->graph_dirty = true; 
        }
    }
    key_c_prev = key_c_curr;

    // UI Update Loop
    if (state->ui_instance.root) {
        // Animation / Logic Update
        ui_element_update(state->ui_instance.root, engine->dt);
        
        // Input Handling
        ui_input_update(&state->input_ctx, state->ui_instance.root, &engine->input);
        
        // Process Events
        UiEvent evt;
        while (ui_input_pop_event(&state->input_ctx, &evt)) {
            switch (evt.type) {
                case UI_EVENT_VALUE_CHANGE:
                case UI_EVENT_DRAG_END:
                    state->graph_dirty = true;
                    break;
                case UI_EVENT_CLICK: {
                    // Selection Logic
                    UiElement* hit = evt.target;
                    while (hit) {
                        if (hit->data_ptr && hit->meta && strcmp(hit->meta->name, "MathNode") == 0) {
                            MathNode* n = (MathNode*)hit->data_ptr;
                            state->selected_node_id = n->id;
                            state->selection_dirty = true;
                            LOG_INFO("Selected Node: %d", n->id);
                            break;
                        }
                        hit = hit->parent;
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
        PlatformWindowSize size = platform_get_framebuffer_size(engine->window);
        ui_layout_root(state->ui_instance.root, (float)size.width, (float)size.height, engine->render_system.frame_count, false, text_measure_wrapper, NULL);
    }

    // Graph Evaluation (Naive interpretation on CPU for debugging/node values)
    for (uint32_t i = 0; i < state->graph.node_count; ++i) {
        const MathNode* n = math_graph_get_node(&state->graph, i);
        if (n && n->type != MATH_NODE_NONE) {
            math_graph_evaluate(&state->graph, i);
        }
    }

    // Recompile Compute Shader if dirty
    if (state->graph_dirty && engine->show_compute_visualizer) {
        math_editor_recompile_graph(state, &engine->render_system);
        state->graph_dirty = false;
    }
}

void math_editor_shutdown(MathEditorState* state, Engine* engine) {
    (void)engine;
    if (!state) return;
    
    ui_instance_destroy(&state->ui_instance);
    arena_destroy(&state->graph_arena);
    
    if (state->ui_asset) ui_asset_free(state->ui_asset);
    
    // Note: RenderSystem pipeline cleanup should theoretically happen here or in Engine
    // but RenderSystem cleans up its own resources on shutdown.
}
