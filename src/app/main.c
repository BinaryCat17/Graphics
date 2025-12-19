#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "features/math_engine/transpiler.h"
#include "foundation/platform/platform.h"
#include "foundation/meta/reflection.h"
#include "engine/graphics/backend/renderer_backend.h"
#include "features/math_engine/math_graph.h"
#include "engine/ui/ui_parser.h"
#include "engine/ui/ui_core.h"
#include "engine/ui/ui_renderer.h"
#include "engine/ui/ui_layout.h"
#include "engine/ui/ui_input.h"
#include "engine/ui/ui_command_system.h"
#include "engine/graphics/text/font.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define KEY_C 67

// --- Application State ---

static float text_measure_wrapper(const char* text, void* user_data) {
    (void)user_data;
    return font_measure_text(text);
}

typedef struct AppState {
    MathGraph graph;
    MemoryArena graph_arena;
    
    // UI State
    UiAsset* ui_asset;
    UiInstance ui_instance; // Manages UI Element memory
    UiInputContext input_ctx;
    
    // Selection
    MathNodeId selected_node_id;
    bool selection_dirty; 
    bool graph_dirty;
    uint32_t current_pipeline;
} AppState;

// --- Runtime Compilation ---

#include "engine/graphics/backend/vulkan/vk_utils.h"

static void app_recompile_graph(AppState* app, RenderSystem* rs) {
    if (!app || !rs || !rs->backend) return;

    LOG_INFO("App: Recompiling Math Graph...");

    // 1. Transpile to GLSL
    char* glsl = math_graph_transpile(&app->graph, TRANSPILE_MODE_IMAGE_2D, SHADER_TARGET_GLSL_VULKAN);
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
    if (app->current_pipeline > 0) {
        rs->backend->compute_pipeline_destroy(rs->backend, app->current_pipeline);
    }
    app->current_pipeline = new_pipe;
    render_system_set_compute_pipeline(rs, new_pipe);
    
    LOG_INFO("App: Graph Recompiled Successfully (ID: %u)", new_pipe);
}

// --- Dynamic UI Helpers ---

static UiElement* ui_find_element_by_id(UiElement* root, const char* id) {
    if (!root || !root->spec) return NULL;
    if (root->spec->id && strcmp(root->spec->id, id) == 0) return root;
    for (size_t i = 0; i < root->child_count; ++i) {
        UiElement* found = ui_find_element_by_id(root->children[i], id);
        if (found) return found;
    }
    return NULL;
}

static void ui_rebuild_graph_view(AppState* app) {
    if (!app->ui_instance.root) return;
    UiElement* canvas = ui_find_element_by_id(app->ui_instance.root, "canvas_area");
    if (!canvas) return;

    // Reset Input State
    ui_input_reset(&app->input_ctx);

    const MetaStruct* node_meta = meta_get_struct("MathNode");
    
    // Count valid nodes
    int count = 0;
    for (uint32_t i = 0; i < app->graph.node_count; ++i) {
        const MathNode* n = math_graph_get_node(&app->graph, i);
        if (n && n->type != MATH_NODE_NONE) count++;
    }

    // Reallocate children array from Arena
    canvas->children = (UiElement**)arena_alloc_zero(&app->ui_instance.arena, count * sizeof(UiElement*));
    canvas->child_count = count;

    int idx = 0;
    for (uint32_t i = 0; i < app->graph.node_count; ++i) {
        MathNode* node = math_graph_get_node(&app->graph, i);
        if (!node || node->type == MATH_NODE_NONE) continue;
        
        // --- Node Window (FROM TEMPLATE) ---
        UiNodeSpec* node_spec = ui_asset_get_template(app->ui_asset, "GraphNode");
        if (node_spec) {
            // Instantiate from template
            UiElement* container = ui_element_create(&app->ui_instance, node_spec, node, node_meta);
            canvas->children[idx++] = container;
            
            // Value Row (Append if needed)
            if (node->type == MATH_NODE_VALUE) {
                UiNodeSpec* val_spec = ui_asset_get_template(app->ui_asset, "ValueRow");
                if (val_spec) {
                    // Resize children to add one more
                    size_t old_cnt = container->child_count;
                    UiElement** old_children = container->children;
                    
                    container->child_count++;
                    container->children = arena_alloc_zero(&app->ui_instance.arena, container->child_count * sizeof(UiElement*));
                    
                    for(size_t k=0; k<old_cnt; ++k) container->children[k] = old_children[k];
                    
                    UiElement* row = ui_element_create(&app->ui_instance, val_spec, node, node_meta);
                    container->children[old_cnt] = row;
                    row->parent = container;
                }
            }
        }
    }
}

static void ui_rebuild_inspector(AppState* app) {
    if (!app->ui_instance.root) return;
    UiElement* inspector = ui_find_element_by_id(app->ui_instance.root, "inspector_area");
    if (!inspector) return;
    
    if (app->selected_node_id == MATH_NODE_INVALID_ID) {
        inspector->child_count = 0;
        return;
    }
    
    MathNode* node = math_graph_get_node(&app->graph, app->selected_node_id);
    if (!node) {
        inspector->child_count = 0;
        return;
    }
    
    const MetaStruct* node_meta = meta_get_struct("MathNode");
    
    // Determine inspector children count
    int ins_count = 1; // Title
    if (node->type == MATH_NODE_VALUE) ins_count += 1; // Field (Row)
    
    inspector->children = (UiElement**)arena_alloc_zero(&app->ui_instance.arena, ins_count * sizeof(UiElement*));
    inspector->child_count = 0;
    
    // Title
    UiNodeSpec* title_spec = ui_asset_get_template(app->ui_asset, "InspectorTitle");
    if (title_spec) {
        UiElement* title = ui_element_create(&app->ui_instance, title_spec, node, node_meta);
        inspector->children[inspector->child_count++] = title;
        title->parent = inspector;
    }
    
    // Value Editor (Only for Value nodes)
    if (node->type == MATH_NODE_VALUE) {
        UiNodeSpec* field_spec = ui_asset_get_template(app->ui_asset, "InspectorField");
        if (field_spec) {
            UiElement* field = ui_element_create(&app->ui_instance, field_spec, node, node_meta);
            inspector->children[inspector->child_count++] = field;
            field->parent = inspector;
        }
    }
}

// --- Commands ---

static void cmd_add_node(void* user_data, UiElement* target) {
    (void)target;
    AppState* app = (AppState*)user_data;
    LOG_INFO("Command: Graph.AddNode");
    math_graph_add_node(&app->graph, MATH_NODE_VALUE);
    ui_rebuild_graph_view(app);
}

static void cmd_clear_graph(void* user_data, UiElement* target) {
    (void)target;
    AppState* app = (AppState*)user_data;
    LOG_INFO("Command: Graph.Clear");
    // We can't easily clear without re-initing arena if we don't have a clear function
    // For now just log
}

static void cmd_recompile(void* user_data, UiElement* target) {
    (void)target;
    AppState* app = (AppState*)user_data;
    app->graph_dirty = true;
}

// --- Application Logic ---

static void app_setup_graph(AppState* app) {
    LOG_INFO("App: Setting up default Math Graph...");
    
    MathNodeId uv_id = math_graph_add_node(&app->graph, MATH_NODE_UV);
    MathNode* uv = math_graph_get_node(&app->graph, uv_id);
    if(uv) { math_graph_set_name(&app->graph, uv_id, "UV.x"); uv->ui_x = 50; uv->ui_y = 100; }
    
    MathNodeId freq_id = math_graph_add_node(&app->graph, MATH_NODE_VALUE);
    MathNode* freq = math_graph_get_node(&app->graph, freq_id);
    if(freq) { math_graph_set_name(&app->graph, freq_id, "Frequency"); freq->value = 20.0f; freq->ui_x = 50; freq->ui_y = 250; }
    
    MathNodeId mul_id = math_graph_add_node(&app->graph, MATH_NODE_MUL);
    MathNode* mul = math_graph_get_node(&app->graph, mul_id);
    if(mul) { math_graph_set_name(&app->graph, mul_id, "Multiply"); mul->ui_x = 250; mul->ui_y = 175; }
    
    MathNodeId sin_id = math_graph_add_node(&app->graph, MATH_NODE_SIN);
    MathNode* s = math_graph_get_node(&app->graph, sin_id);
    if(s) { math_graph_set_name(&app->graph, sin_id, "Sin"); s->ui_x = 450; s->ui_y = 175; }
    
    math_graph_connect(&app->graph, mul_id, 0, uv_id); 
    math_graph_connect(&app->graph, mul_id, 1, freq_id); 
    math_graph_connect(&app->graph, sin_id, 0, mul_id);
}

static void app_on_init(Engine* engine) {
    AppState* app = (AppState*)calloc(1, sizeof(AppState));
    engine->user_data = app;

    // Init Graph Arena
    arena_init(&app->graph_arena, 1024 * 1024); // 1MB
    math_graph_init(&app->graph, &app->graph_arena);
    
    app->selected_node_id = MATH_NODE_INVALID_ID;
    
    app_setup_graph(app);

    ui_command_init();
    ui_command_register("Graph.AddNode", cmd_add_node, app);
    ui_command_register("Graph.Clear", cmd_clear_graph, app);
    ui_command_register("Graph.Recompile", cmd_recompile, app);

    ui_input_init(&app->input_ctx);
    ui_instance_init(&app->ui_instance, 1024 * 1024); // 1MB UI Arena

    // Force visualizer on for Phase 3 testing
    engine->show_compute_visualizer = true;
    engine->render_system.show_compute_result = true;

    const char* ui_path = "assets/ui/editor.yaml"; 
    if (ui_path) {
        app->ui_asset = ui_parser_load_from_file(ui_path);
        if (app->ui_asset) {
            const MetaStruct* graph_meta = meta_get_struct("MathGraph");
            
            // Build Static UI from Asset into Instance
            app->ui_instance.root = ui_element_create(&app->ui_instance, app->ui_asset->root, &app->graph, graph_meta);

            render_system_bind_ui(&engine->render_system, app->ui_instance.root);
            
            ui_rebuild_graph_view(app);
            
            // Select first node
            for(uint32_t i=0; i<app->graph.node_count; ++i) {
                const MathNode* n = math_graph_get_node(&app->graph, i);
                if(n && n->type != MATH_NODE_NONE) {
                    app->selected_node_id = n->id;
                    break;
                }
            }
        }
    }

    // Initial Compile
    app_recompile_graph(app, &engine->render_system);
}

static void app_on_update(Engine* engine) {
    AppState* app = (AppState*)engine->user_data;
    if (!app) return;

    static bool key_c_prev = false;
    bool key_c_curr = platform_get_key(engine->window, KEY_C);
    
    if (key_c_curr && !key_c_prev) {
        engine->show_compute_visualizer = !engine->show_compute_visualizer;
        engine->render_system.show_compute_result = engine->show_compute_visualizer;
        if (engine->show_compute_visualizer) {
            app->graph_dirty = true; // Trigger recompile
        }
    }
    key_c_prev = key_c_curr;

    if (app->ui_instance.root) {
        ui_element_update(app->ui_instance.root, engine->dt);
        ui_input_update(&app->input_ctx, app->ui_instance.root, &engine->input);
        
        // Handle Events
        UiEvent evt;
        while (ui_input_pop_event(&app->input_ctx, &evt)) {
            switch (evt.type) {
                case UI_EVENT_VALUE_CHANGE:
                case UI_EVENT_DRAG_END:
                    app->graph_dirty = true;
                    break;
                case UI_EVENT_CLICK: {
                    // Selection Logic
                    UiElement* hit = evt.target;
                    while (hit) {
                        if (hit->data_ptr && hit->meta && strcmp(hit->meta->name, "MathNode") == 0) {
                            MathNode* n = (MathNode*)hit->data_ptr;
                            app->selected_node_id = n->id;
                            app->selection_dirty = true;
                            LOG_INFO("Selected Node: %d", n->id);
                            break;
                        }
                        hit = hit->parent;
                    }
                } break;
                default: break;
            }
        }
        
        if (app->selection_dirty) {
            ui_rebuild_inspector(app);
            app->selection_dirty = false;
        }
        
        PlatformWindowSize size = platform_get_framebuffer_size(engine->window);
        ui_layout_root(app->ui_instance.root, (float)size.width, (float)size.height, engine->render_system.frame_count, false, text_measure_wrapper, NULL);
    }

    // Evaluate all nodes (Naive)
    for (uint32_t i = 0; i < app->graph.node_count; ++i) {
        const MathNode* n = math_graph_get_node(&app->graph, i);
        if (n && n->type != MATH_NODE_NONE) {
            math_graph_evaluate(&app->graph, i);
        }
    }

    // Recompile if needed
    if (app->graph_dirty && engine->show_compute_visualizer) {
        app_recompile_graph(app, &engine->render_system);
        app->graph_dirty = false;
    }
}

int main(int argc, char** argv) {
    EngineConfig config = {
        .width = 1280, .height = 720, .title = "Graphics Engine",
        .assets_path = "assets", .ui_path = "assets/ui/editor.yaml",
        .log_level = LOG_LEVEL_INFO,
        .on_init = app_on_init, .on_update = app_on_update
    };
    logger_init("logs/graphics.log");
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0) config.assets_path = argv[++i];
        else if (strcmp(argv[i], "--ui") == 0) config.ui_path = argv[++i];
        else if (strcmp(argv[i], "--log-level") == 0) {
            const char* l = argv[++i];
            if (strcmp(l, "debug") == 0) config.log_level = LOG_LEVEL_DEBUG;
        } else if (strcmp(argv[i], "--log-interval") == 0) {
             float interval = atof(argv[++i]);
             logger_set_trace_interval(interval);
             config.screenshot_interval = (double)interval;
        }
    }

    Engine engine;
    if (engine_init(&engine, &config)) {
        engine_run(&engine);
        if (engine.user_data) {
             AppState* app = (AppState*)engine.user_data;
             ui_instance_destroy(&app->ui_instance);
             arena_destroy(&app->graph_arena); // Free graph memory
             if (app->ui_asset) ui_asset_free(app->ui_asset);
             // math_graph_clear(&app->graph); // No need as arena handles it
             free(app);
        }
        engine_shutdown(&engine);
    }
    logger_shutdown();
    return 0;
}