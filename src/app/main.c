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
    
    // Dynamic UI Generation (Specs only)
    MemoryArena dynamic_ui_arena;
    
    // Selection
    MathNodeId selected_node_id;
    bool selection_dirty; 
} AppState;

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

static void* ui_alloc_spec(AppState* app, size_t size) {
    if (!app->dynamic_ui_arena.base) {
        arena_init(&app->dynamic_ui_arena, 256 * 1024); // 256KB for specs
    }
    return arena_alloc_zero(&app->dynamic_ui_arena, size);
}

static UiNodeSpec* ui_create_spec(AppState* app, UiKind kind) {
    UiNodeSpec* spec = (UiNodeSpec*)ui_alloc_spec(app, sizeof(UiNodeSpec));
    spec->kind = kind;
    spec->color = (Vec4){1,1,1,1}; 
    return spec;
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
        
        // --- Node Window ---
        UiNodeSpec* container_spec = ui_create_spec(app, UI_KIND_CONTAINER);
        container_spec->layout = UI_LAYOUT_FLEX_COLUMN;
        container_spec->width = 150;
        container_spec->height = 100;
        container_spec->flags = UI_FLAG_DRAGGABLE | UI_FLAG_CLICKABLE;
        container_spec->x_source = "ui_x"; // Use new ui_x field
        container_spec->y_source = "ui_y";
        container_spec->color = (Vec4){0.2f, 0.2f, 0.2f, 1.0f}; // Was bg_color
        // container_spec->border_color = ... // Removed
        // container_spec->border_width = ... // Removed
        container_spec->padding = 5.0f; // Changed from Vec4 to float

        // Bind directly to the node in the array
        UiElement* container = ui_element_create(&app->ui_instance, container_spec, node, node_meta);
        canvas->children[idx++] = container;
        
        // Determine children count
        int child_cnt = (node->type == MATH_NODE_VALUE) ? 2 : 1;
        container->children = (UiElement**)arena_alloc_zero(&app->ui_instance.arena, child_cnt * sizeof(UiElement*));
        container->child_count = child_cnt;
        
        // Header (Name)
        UiNodeSpec* label_spec = ui_create_spec(app, UI_KIND_TEXT);
        label_spec->text_source = "name";
        // label_spec->font_size = 16; // Removed
        label_spec->color = (Vec4){1,1,1,1};
        label_spec->height = 20;
        
        UiElement* label = ui_element_create(&app->ui_instance, label_spec, node, node_meta);
        container->children[0] = label;
        label->parent = container;
        
        // Value (if Value node)
        if (node->type == MATH_NODE_VALUE) {
            UiNodeSpec* val_spec = ui_create_spec(app, UI_KIND_TEXT);
            val_spec->text_source = "value"; // Bind to float value
            // val_spec->font_size = 14; // Removed
            val_spec->color = (Vec4){0.8f,0.8f,0.8f,1};
            
            UiElement* val = ui_element_create(&app->ui_instance, val_spec, node, node_meta);
            container->children[1] = val;
            val->parent = container;
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
    if (node->type == MATH_NODE_VALUE) ins_count += 2; // Label + Value
    
    inspector->children = (UiElement**)arena_alloc_zero(&app->ui_instance.arena, ins_count * sizeof(UiElement*));
    inspector->child_count = 0;
    
    // Title
    UiNodeSpec* title_spec = ui_create_spec(app, UI_KIND_TEXT);
    title_spec->text_source = "name";
    // title_spec->font_size = 20; // Removed
    
    UiElement* title = ui_element_create(&app->ui_instance, title_spec, node, node_meta);
    inspector->children[inspector->child_count++] = title;
    title->parent = inspector;
    
    // Value Editor (Only for Value nodes)
    if (node->type == MATH_NODE_VALUE) {
        // Label
        UiNodeSpec* lbl = ui_create_spec(app, UI_KIND_TEXT);
        lbl->static_text = "Value:"; // Was text_content
        UiElement* l = ui_element_create(&app->ui_instance, lbl, NULL, NULL);
        inspector->children[inspector->child_count++] = l;
        l->parent = inspector;
        
        // Input (Simplified as text for now, should be slider/input)
        UiNodeSpec* val = ui_create_spec(app, UI_KIND_TEXT);
        val->text_source = "value";
        val->color = (Vec4){0,1,0,1};
        UiElement* v = ui_element_create(&app->ui_instance, val, node, node_meta);
        inspector->children[inspector->child_count++] = v;
        v->parent = inspector;
    }
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

    ui_input_init(&app->input_ctx);
    ui_instance_init(&app->ui_instance, 1024 * 1024); // 1MB UI Arena

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
            char* glsl = math_graph_transpile_glsl(&app->graph, TRANSPILE_MODE_IMAGE_2D);
            if (glsl) {
                LOG_INFO("Generated GLSL:\n%s", glsl);
                free(glsl);
            }
        }
    }
    key_c_prev = key_c_curr;

    if (app->ui_instance.root) {
        ui_element_update(app->ui_instance.root);
        ui_input_update(&app->input_ctx, app->ui_instance.root, &engine->input);
        
        if (engine->input.mouse_clicked && app->input_ctx.hovered) {
             UiElement* hit = app->input_ctx.hovered;
             while (hit) {
                 if (hit->data_ptr && hit->meta && strcmp(hit->meta->name, "MathNode") == 0) {
                     // Safe extraction of ID
                     MathNode* n = (MathNode*)hit->data_ptr;
                     app->selected_node_id = n->id;
                     app->selection_dirty = true;
                     LOG_INFO("Selected Node: %d", n->id);
                     break;
                 }
                 hit = hit->parent;
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
             if (app->dynamic_ui_arena.base) arena_destroy(&app->dynamic_ui_arena);
             // math_graph_clear(&app->graph); // No need as arena handles it
             free(app);
        }
        engine_shutdown(&engine);
    }
    logger_shutdown();
    return 0;
}