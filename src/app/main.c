#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "features/graph_editor/transpiler.h"
#include "foundation/platform/platform.h"
#include "foundation/meta/reflection.h"
#include "engine/graphics/backend/renderer_backend.h"
#include "features/graph_editor/math_graph.h"
#include "engine/ui/ui_parser.h"
#include "engine/ui/ui_core.h"
#include "engine/ui/ui_renderer.h"
#include "engine/ui/ui_layout.h"
#include "engine/ui/ui_input.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define KEY_C 67

// --- Application State ---

typedef struct AppState {
    MathGraph graph;
    
    // UI State
    UiAsset* ui_asset;
    UiInstance ui_instance; // Manages UI Element memory
    UiInputContext input_ctx;
    
    // Dynamic UI Generation (Specs only)
    MemoryArena dynamic_ui_arena;
    
    // Selection
    MathNode* selected_node;
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
    
    // Count Nodes
    int count = 0;
    for(int i=0; i<app->graph.node_count; ++i) if(app->graph.nodes[i]) count++;

    // Reallocate children array from Arena
    // Old array is leaked in arena but that's fine for prototype/rare updates.
    canvas->children = (UiElement**)arena_alloc_zero(&app->ui_instance.arena, count * sizeof(UiElement*));
    canvas->child_count = count;

    int idx = 0;
    for (int i = 0; i < app->graph.node_count; ++i) {
        MathNode* node = app->graph.nodes[i];
        if (!node) continue;

        // Container Spec
        UiNodeSpec* container_spec = ui_create_spec(app, UI_KIND_CONTAINER);
        container_spec->layout = UI_LAYOUT_FLEX_COLUMN;
        container_spec->width = 150;
        container_spec->height = 100;
        container_spec->flags = UI_FLAG_DRAGGABLE | UI_FLAG_CLICKABLE;
        container_spec->x_source = "x";
        container_spec->y_source = "y";
        container_spec->color = (Vec4){0.25f, 0.25f, 0.28f, 1.0f};
        container_spec->border_l = container_spec->border_t = container_spec->border_r = container_spec->border_b = 4;
        container_spec->texture_path = "ui_rect";
        container_spec->tex_w = 32; container_spec->tex_h = 32;

        UiElement* el = ui_element_create(&app->ui_instance, container_spec, node, node_meta);
        
        // Set up children manually
        int child_cnt = (node->type == MATH_NODE_VALUE) ? 2 : 1;
        el->child_count = child_cnt;
        el->children = (UiElement**)arena_alloc_zero(&app->ui_instance.arena, child_cnt * sizeof(UiElement*));
        
        // Label
        UiNodeSpec* label_spec = ui_create_spec(app, UI_KIND_TEXT);
        label_spec->text_source = "name";
        label_spec->height = 20;
        label_spec->padding = 5;
        label_spec->color = (Vec4){1,1,1,1};
        
        el->children[0] = ui_element_create(&app->ui_instance, label_spec, node, node_meta);
        el->children[0]->parent = el;

        // Value
        if (node->type == MATH_NODE_VALUE) {
            UiNodeSpec* val_spec = ui_create_spec(app, UI_KIND_TEXT);
            val_spec->text_source = "value"; 
            val_spec->height = 20;
            val_spec->padding = 5;
            val_spec->color = (Vec4){0.8f, 0.8f, 0.8f, 1.0f};
            
            el->children[1] = ui_element_create(&app->ui_instance, val_spec, node, node_meta);
            el->children[1]->parent = el;
        }

        canvas->children[idx] = el;
        el->parent = canvas;
        idx++;
    }
}

static void ui_rebuild_inspector(AppState* app) {
    if (!app->ui_instance.root) return;
    UiElement* panel = ui_find_element_by_id(app->ui_instance.root, "prop_group");
    if (!panel) return;
    
    ui_input_reset(&app->input_ctx);

    if (!app->selected_node) {
        panel->child_count = 0;
        panel->children = NULL;
        return;
    }

    const MetaStruct* meta = meta_get_struct("MathNode");
    if (!meta) return;

    // Count visible fields
    int field_count = 0;
    for (size_t i = 0; i < meta->field_count; ++i) {
        const MetaField* field = &meta->fields[i];
        if (field->type == META_TYPE_POINTER || field->type == META_TYPE_ARRAY) continue;
        if (strcmp(field->name, "dirty") == 0) continue; 
        if (strcmp(field->name, "id") == 0) continue;
        field_count++;
    }

    // Each field needs Label + Input = 2 elements
    panel->child_count = field_count * 2;
    panel->children = (UiElement**)arena_alloc_zero(&app->ui_instance.arena, panel->child_count * sizeof(UiElement*));
    
    int idx = 0;
    for (size_t i = 0; i < meta->field_count; ++i) {
        const MetaField* field = &meta->fields[i];
        if (field->type == META_TYPE_POINTER || field->type == META_TYPE_ARRAY) continue;
        if (strcmp(field->name, "dirty") == 0) continue; 
        if (strcmp(field->name, "id") == 0) continue;

        // Label
        UiNodeSpec* label_spec = ui_create_spec(app, UI_KIND_TEXT);
        label_spec->static_text = arena_push_string(&app->dynamic_ui_arena, field->name);
        label_spec->height = 20;
        label_spec->color = (Vec4){0.7f, 0.7f, 0.7f, 1.0f};
        
        UiElement* label = ui_element_create(&app->ui_instance, label_spec, NULL, NULL);
        panel->children[idx] = label;
        label->parent = panel;
        idx++;

        // Input
        UiNodeSpec* input_spec = ui_create_spec(app, UI_KIND_TEXT_INPUT);
        input_spec->flags = UI_FLAG_EDITABLE | UI_FLAG_FOCUSABLE | UI_FLAG_CLICKABLE;
        input_spec->text_source = arena_push_string(&app->dynamic_ui_arena, field->name);
        input_spec->height = 25;
        input_spec->color = (Vec4){0.1f, 0.1f, 0.1f, 1.0f};
        
        UiElement* input = ui_element_create(&app->ui_instance, input_spec, app->selected_node, meta);
        panel->children[idx] = input;
        input->parent = panel;
        idx++;
    }
}

// --- Application Logic ---

static void app_setup_graph(AppState* app) {
    LOG_INFO("App: Setting up default Math Graph...");
    app->graph.graph_name = strdup("My Awesome Graph");
    MathNode* uv = math_graph_add_node(&app->graph, MATH_NODE_UV);
    uv->name = strdup("UV.x"); uv->x = 50; uv->y = 100;
    MathNode* freq = math_graph_add_node(&app->graph, MATH_NODE_VALUE);
    freq->name = strdup("Frequency"); freq->value = 20.0f; freq->x = 50; freq->y = 250;
    MathNode* mul = math_graph_add_node(&app->graph, MATH_NODE_MUL);
    mul->name = strdup("Multiply"); mul->x = 250; mul->y = 175;
    MathNode* s = math_graph_add_node(&app->graph, MATH_NODE_SIN);
    s->name = strdup("Sin"); s->x = 450; s->y = 175;
    math_graph_connect(mul, 0, uv); math_graph_connect(mul, 1, freq); math_graph_connect(s, 0, mul);
}

static void app_on_init(Engine* engine) {
    AppState* app = (AppState*)calloc(1, sizeof(AppState));
    engine->user_data = app;

    math_graph_init(&app->graph);
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
            if (app->graph.node_count > 0) {
                app->selected_node = app->graph.nodes[0];
                app->selection_dirty = true;
                ui_rebuild_inspector(app);
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
                if (engine->render_system.backend->run_compute_image) {
                    engine->render_system.backend->run_compute_image(engine->render_system.backend, glsl, 512, 512);
                }
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
                     app->selected_node = (MathNode*)hit->data_ptr;
                     app->selection_dirty = true;
                     LOG_INFO("Selected Node: %d", app->selected_node->id);
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
        ui_layout_root(app->ui_instance.root, (float)size.width, (float)size.height, engine->render_system.frame_count, false);
    }

    math_graph_update(&app->graph);
    math_graph_update_visuals(&app->graph, false);
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
             ui_instance_destroy(&app->ui_instance); // Destroy Arena
             if (app->ui_asset) ui_asset_free(app->ui_asset);
             if (app->dynamic_ui_arena.base) arena_destroy(&app->dynamic_ui_arena);
             math_graph_dispose(&app->graph);
             free(app);
        }
        engine_shutdown(&engine);
    }
    logger_shutdown();
    return 0;
}
