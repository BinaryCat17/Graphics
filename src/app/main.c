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
    UiElement* ui_root;
    UiInputContext input_ctx;
    
    // Dynamic UI Generation
    MemoryArena dynamic_ui_arena;
    
    // Selection
    MathNode* selected_node;
    bool selection_dirty; // Flags that inspector needs rebuild
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

static void ui_element_add_child(UiElement* parent, UiElement* child) {
    if (!parent || !child) return;
    
    // Simple realloc for prototype
    UiElement** new_children = (UiElement**)realloc(parent->children, (parent->child_count + 1) * sizeof(UiElement*));
    if (new_children) {
        parent->children = new_children;
        parent->children[parent->child_count] = child;
        parent->child_count++;
        child->parent = parent;
    }
}

static void* ui_alloc_dynamic(AppState* app, size_t size) {
    // If arena is not initialized, init it.
    if (!app->dynamic_ui_arena.base) {
        arena_init(&app->dynamic_ui_arena, 1024 * 1024); // 1MB
    }
    return arena_alloc_zero(&app->dynamic_ui_arena, size);
}

static UiNodeSpec* ui_create_spec(AppState* app, UiKind kind) {
    UiNodeSpec* spec = (UiNodeSpec*)ui_alloc_dynamic(app, sizeof(UiNodeSpec));
    spec->kind = kind;
    spec->color = (Vec4){1,1,1,1}; // Default white
    return spec;
}

static void ui_rebuild_graph_view(AppState* app) {
    if (!app->ui_root) return;
    UiElement* canvas = ui_find_element_by_id(app->ui_root, "canvas_area");
    if (!canvas) return;

    // Clear existing children (Note: We assume canvas children are transient or we just nuking them)
    // IMPORTANT: If we use realloc, we must be careful not to double-free if we use arena specs but malloced elements.
    // Standard ui_element_free frees children.
    for(size_t i=0; i<canvas->child_count; ++i) {
        ui_element_free(canvas->children[i]);
    }
    free(canvas->children);
    canvas->children = NULL;
    canvas->child_count = 0;

    const MetaStruct* node_meta = meta_get_struct("MathNode");

    // Re-populate from Graph
    for (int i = 0; i < app->graph.node_count; ++i) {
        MathNode* node = app->graph.nodes[i];
        if (!node) continue;

        // Container Spec
        UiNodeSpec* container_spec = ui_create_spec(app, UI_KIND_CONTAINER);
        container_spec->layout = UI_LAYOUT_FLEX_COLUMN;
        container_spec->width = 150;
        container_spec->height = 100; // Fixed size for now
        container_spec->flags = UI_FLAG_DRAGGABLE | UI_FLAG_CLICKABLE;
        container_spec->x_source = "x";
        container_spec->y_source = "y";
        // Styling
        container_spec->color = (Vec4){0.25f, 0.25f, 0.28f, 1.0f};
        container_spec->border_l = container_spec->border_t = container_spec->border_r = container_spec->border_b = 4;
        container_spec->texture_path = "ui_rect";
        container_spec->tex_w = 32; container_spec->tex_h = 32;

        // Container Element
        UiElement* el = ui_element_create(container_spec, node, node_meta);
        
        // Label Spec (Title)
        UiNodeSpec* label_spec = ui_create_spec(app, UI_KIND_TEXT);
        label_spec->text_source = "name";
        label_spec->height = 20;
        label_spec->padding = 5;
        label_spec->color = (Vec4){1,1,1,1};
        
        UiElement* label = ui_element_create(label_spec, node, node_meta);
        ui_element_add_child(el, label);

        // Value Label (if Observable)
        // Check type
        if (node->type == MATH_NODE_VALUE) {
            UiNodeSpec* val_spec = ui_create_spec(app, UI_KIND_TEXT);
            val_spec->text_source = "value"; 
            val_spec->height = 20;
            val_spec->padding = 5;
            val_spec->color = (Vec4){0.8f, 0.8f, 0.8f, 1.0f};
            
            UiElement* val_el = ui_element_create(val_spec, node, node_meta);
            ui_element_add_child(el, val_el);
        }

        ui_element_add_child(canvas, el);
    }
}

static void ui_rebuild_inspector(AppState* app) {
    if (!app->ui_root) return;
    UiElement* panel = ui_find_element_by_id(app->ui_root, "prop_group");
    if (!panel) return;

    // Clear existing
    for(size_t i=0; i<panel->child_count; ++i) {
        ui_element_free(panel->children[i]);
    }
    free(panel->children);
    panel->children = NULL;
    panel->child_count = 0;

    if (!app->selected_node) return;

    const MetaStruct* meta = meta_get_struct("MathNode");
    if (!meta) return;

    // Iterate Fields
    for (size_t i = 0; i < meta->field_count; ++i) {
        const MetaField* field = &meta->fields[i];
        
        // Skip hidden internal fields if needed (e.g., id, dirty, inputs)
        // For now, expose everything except arrays/pointers
        if (field->type == META_TYPE_POINTER || field->type == META_TYPE_ARRAY) continue;
        if (strcmp(field->name, "dirty") == 0) continue; 
        if (strcmp(field->name, "id") == 0) continue;

        // Label
        UiNodeSpec* label_spec = ui_create_spec(app, UI_KIND_TEXT);
        label_spec->static_text = arena_push_string(&app->dynamic_ui_arena, field->name);
        label_spec->height = 20;
        label_spec->color = (Vec4){0.7f, 0.7f, 0.7f, 1.0f};
        ui_element_add_child(panel, ui_element_create(label_spec, NULL, NULL));

        // Input
        UiNodeSpec* input_spec = ui_create_spec(app, UI_KIND_TEXT_INPUT);
        input_spec->flags = UI_FLAG_EDITABLE | UI_FLAG_FOCUSABLE | UI_FLAG_CLICKABLE;
        input_spec->text_source = arena_push_string(&app->dynamic_ui_arena, field->name); // Bind name
        input_spec->height = 25;
        input_spec->color = (Vec4){0.1f, 0.1f, 0.1f, 1.0f};
        
        ui_element_add_child(panel, ui_element_create(input_spec, app->selected_node, meta));
    }
}

// --- Application Logic ---

static void app_setup_graph(AppState* app) {
    LOG_INFO("App: Setting up default Math Graph...");
    
    app->graph.graph_name = strdup("My Awesome Graph");
    
    // Create Test Nodes (Visualizer Graph)
    MathNode* uv = math_graph_add_node(&app->graph, MATH_NODE_UV);
    uv->name = strdup("UV.x");
    uv->x = 50; uv->y = 100;

    MathNode* freq = math_graph_add_node(&app->graph, MATH_NODE_VALUE);
    freq->name = strdup("Frequency");
    freq->value = 20.0f;
    freq->x = 50; freq->y = 250;
    
    MathNode* mul = math_graph_add_node(&app->graph, MATH_NODE_MUL);
    mul->name = strdup("Multiply");
    mul->x = 250; mul->y = 175;
    
    MathNode* s = math_graph_add_node(&app->graph, MATH_NODE_SIN);
    s->name = strdup("Sin");
    s->x = 450; s->y = 175;
    
    math_graph_connect(mul, 0, uv);
    math_graph_connect(mul, 1, freq);
    math_graph_connect(s, 0, mul);
}

static void app_on_init(Engine* engine) {
    // 1. Allocate State
    AppState* app = (AppState*)calloc(1, sizeof(AppState));
    engine->user_data = app;

    // 2. Initialize Graph
    math_graph_init(&app->graph);
    app_setup_graph(app);

    // 3. Initialize UI Input
    ui_input_init(&app->input_ctx);

    // 4. Initialize UI Asset
    const char* ui_path = "assets/ui/editor.yaml"; 
    
    if (ui_path) {
        app->ui_asset = ui_parser_load_from_file(ui_path);
        if (!app->ui_asset) {
            LOG_ERROR("App: Failed to load UI definition from '%s'", ui_path);
            return;
        }

        const MetaStruct* graph_meta = meta_get_struct("MathGraph");
        if (!graph_meta) {
            LOG_FATAL("App: Reflection metadata for 'MathGraph' not found.");
            return;
        }
        
        app->ui_root = ui_element_create(app->ui_asset->root, &app->graph, graph_meta);
        if (!app->ui_root) {
            LOG_ERROR("App: Failed to create UI View.");
            return;
        }

        // 5. Bind UI to Renderer
        render_system_bind_ui(&engine->render_system, app->ui_root);
        
        // 6. Populate Dynamic UI
        ui_rebuild_graph_view(app);
        
        // Default Selection
        if (app->graph.node_count > 0) {
            app->selected_node = app->graph.nodes[0];
            app->selection_dirty = true;
            ui_rebuild_inspector(app); // Immediate build
        }
    }
}

static void app_on_update(Engine* engine) {
    AppState* app = (AppState*)engine->user_data;
    if (!app) return;

    static bool key_c_prev = false;
    bool key_c_curr = platform_get_key(engine->window, KEY_C);
    
    // Toggle Compute Visualization
    if (key_c_curr && !key_c_prev) {
        engine->show_compute_visualizer = !engine->show_compute_visualizer;
        engine->render_system.show_compute_result = engine->show_compute_visualizer;
        
        if (engine->show_compute_visualizer) {
            LOG_INFO("App: Transpiling & Running Compute Graph...");
            
            // 1. Transpile Graph to GLSL
            char* glsl = math_graph_transpile_glsl(&app->graph, TRANSPILE_MODE_IMAGE_2D);
            
            // 2. Run on GPU
            if (glsl) {
                RenderSystem* rs = &engine->render_system;
                if (rs->backend && rs->backend->run_compute_image) {
                    rs->backend->run_compute_image(rs->backend, glsl, 512, 512);
                }
                free(glsl);
            }
        }
    }
    key_c_prev = key_c_curr;

    // UI Update Pipeline
    if (app->ui_root) {
        // 1. Update Data Bindings (Read latest X/Y/Text from Graph)
        ui_element_update(app->ui_root);
        
        // 2. Process Input (Write-back new positions if dragging)
        ui_input_update(&app->input_ctx, app->ui_root, &engine->input);
        
        // Selection Logic
        if (engine->input.mouse_clicked && app->input_ctx.hovered) {
             UiElement* hit = app->input_ctx.hovered;
             // Traverse up to find a node container if we hit a child label
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
        
        // 3. Layout (Calculate Rects based on updated data)
        PlatformWindowSize size = platform_get_framebuffer_size(engine->window);
        ui_layout_root(app->ui_root, (float)size.width, (float)size.height, engine->render_system.frame_count, false);
    }

    // Graph Update
    math_graph_update(&app->graph);
    math_graph_update_visuals(&app->graph, false);
}

// --- Main Entry Point ---

int main(int argc, char** argv) {
    // 1. Config
    EngineConfig config = {
        .width = 1280,
        .height = 720,
        .title = "Graphics Engine",
        .assets_path = "assets",
        .ui_path = "assets/ui/editor.yaml",
        .log_level = LOG_LEVEL_INFO,
        // Bind Callbacks
        .on_init = app_on_init,
        .on_update = app_on_update
    };

    logger_init("logs/graphics.log");

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            config.assets_path = argv[++i];
        } else if (strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
            config.ui_path = argv[++i];
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            const char* level_str = argv[++i];
            if (strcmp(level_str, "trace") == 0) config.log_level = LOG_LEVEL_TRACE;
            else if (strcmp(level_str, "debug") == 0) config.log_level = LOG_LEVEL_DEBUG;
            else if (strcmp(level_str, "info") == 0) config.log_level = LOG_LEVEL_INFO;
            else if (strcmp(level_str, "warn") == 0) config.log_level = LOG_LEVEL_WARN;
            else if (strcmp(level_str, "error") == 0) config.log_level = LOG_LEVEL_ERROR;
            else if (strcmp(level_str, "fatal") == 0) config.log_level = LOG_LEVEL_FATAL;
        } else if (strcmp(argv[i], "--log-interval") == 0 && i + 1 < argc) {
             float interval = atof(argv[++i]);
             logger_set_trace_interval(interval);
             config.screenshot_interval = (double)interval;
        }
    }

    // 2. Engine Lifecycle
    Engine engine;
    if (engine_init(&engine, &config)) {
        engine_run(&engine);
        
        // Cleanup App State
        if (engine.user_data) {
             AppState* app = (AppState*)engine.user_data;
             if (app->ui_root) ui_element_free(app->ui_root);
             if (app->ui_asset) ui_asset_free(app->ui_asset);
             if (app->dynamic_ui_arena.base) arena_destroy(&app->dynamic_ui_arena);
             math_graph_dispose(&app->graph);
             free(app);
        }
        
        engine_shutdown(&engine);
    } else {
        LOG_FATAL("Engine failed to initialize.");
        return 1;
    }

    logger_shutdown();
    return 0;
}