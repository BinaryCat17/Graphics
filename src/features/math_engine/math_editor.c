#include "math_editor.h"
#include "engine/core/engine.h"
#include "internal/math_editor_internal.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/meta/reflection.h"
#include "foundation/config/simple_yaml.h"
#include "features/math_engine/internal/transpiler.h"
#include "features/math_engine/internal/math_graph_internal.h" // Access to internal Graph/Node structs
#include "engine/graphics/internal/renderer_backend.h"
#include "engine/text/font.h"
#include "engine/assets/assets.h"
#include "engine/graphics/render_system.h"
#include "engine/input/input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- Helper: Text Measurement for UI Layout ---
static float text_measure_wrapper(const char* text, void* user_data) {
    const Font* font = (const Font*)user_data;
    return font_measure_text(font, text);
}

// --- View Model Management ---

static MathNodeView* math_editor_add_view(MathEditor* editor, MathNodeId id, float x, float y) {
    if (editor->node_view_count >= editor->node_view_cap) {
        uint32_t new_cap = editor->node_view_cap ? editor->node_view_cap * 2 : 16;
        MathNodeView* new_arr = arena_alloc_zero(&editor->graph_arena, new_cap * sizeof(MathNodeView));
        if (editor->node_views) {
            memcpy(new_arr, editor->node_views, editor->node_view_count * sizeof(MathNodeView));
        }
        editor->node_views = new_arr;
        editor->node_view_cap = new_cap;
    }
    MathNodeView* view = &editor->node_views[editor->node_view_count++];
    view->node_id = id;
    view->x = x;
    view->y = y;
    return view;
}

static void math_editor_sync_view_data(MathEditor* editor) {
    for(uint32_t i=0; i<editor->node_view_count; ++i) {
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

// --- Recompilation Logic ---

static void math_editor_recompile_graph(MathEditor* editor, RenderSystem* rs) {
    if (!editor || !rs) return;

    LOG_INFO("Editor: Recompiling Math Graph...");

    // 1. Transpile to GLSL
    char* glsl = math_graph_transpile(editor->graph, TRANSPILE_MODE_IMAGE_2D, SHADER_TARGET_GLSL_VULKAN);
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
    if (editor->current_pipeline > 0) {
        render_system_destroy_compute_pipeline(rs, editor->current_pipeline);
    }
    editor->current_pipeline = new_pipe;
    render_system_set_compute_pipeline(rs, new_pipe);
    
    LOG_INFO("Editor: Graph Recompiled Successfully (ID: %u)", new_pipe);
}

// --- UI Regeneration Logic (Imperative -> Declarative Bridge) ---

static void math_editor_refresh_graph_view(MathEditor* editor) {
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

static void math_editor_update_selection(MathEditor* editor) {
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

    // 2. Trigger UI Rebuild for Inspector
    // The UI system will read 'selected_nodes_count' and 'selected_nodes' 
    // and instantiate the correct template based on 'type'.
    UiElement* root = ui_instance_get_root(editor->ui_instance);
    if (root) {
        UiElement* inspector = ui_element_find_by_id(root, "inspector_area");
        if (inspector) {
             ui_element_rebuild_children(inspector, editor->ui_instance);
        }
    }
}

// --- Commands ---

UI_COMMAND(cmd_add_node, MathEditor) {
    LOG_INFO("Command: Graph.AddNode");
    
    MathNodeType type = MATH_NODE_VALUE;

    // Try to extract type from PaletteItem data bound to the UI element
    if (target) {
        void* data = ui_element_get_data(target);
        const MetaStruct* meta = ui_element_get_meta(target);
        
        // If the button itself doesn't have data, check parent
        if (!data) {
             UiElement* p = ui_element_get_parent(target);
             if(p) {
                 data = ui_element_get_data(p);
                 meta = ui_element_get_meta(p);
             }
        }

        if (data && meta && strcmp(meta->name, "MathNodePaletteItem") == 0) {
            MathNodePaletteItem* item = (MathNodePaletteItem*)data;
            type = (MathNodeType)item->type;
        }
    }
    
    MathNodeId id = math_graph_add_node(ctx->graph, type);
    // Center roughly or use mouse pos if available (future)
    math_editor_add_view(ctx, id, 100, 100); 
    
    math_editor_refresh_graph_view(ctx);
}

UI_COMMAND(cmd_clear_graph, MathEditor) {
    LOG_INFO("Command: Graph.Clear");
    // TODO: Implement proper clear
}

UI_COMMAND(cmd_recompile, MathEditor) {
    ctx->graph_dirty = true;
}

// --- Lifecycle ---

static void math_editor_setup_default_graph(MathEditor* editor) {
    LOG_INFO("Editor: Setting up default Math Graph...");
    
    MathNodeId uv_id = math_graph_add_node(editor->graph, MATH_NODE_UV);
    MathNode* uv = math_graph_get_node(editor->graph, uv_id);
    if(uv) { math_graph_set_name(editor->graph, uv_id, "UV.x"); }
    math_editor_add_view(editor, uv_id, 50, 100);
    
    MathNodeId freq_id = math_graph_add_node(editor->graph, MATH_NODE_VALUE);
    MathNode* freq = math_graph_get_node(editor->graph, freq_id);
    if(freq) { math_graph_set_name(editor->graph, freq_id, "Frequency"); freq->value = 20.0f; }
    math_editor_add_view(editor, freq_id, 50, 250);
    
    MathNodeId mul_id = math_graph_add_node(editor->graph, MATH_NODE_MUL);
    MathNode* mul = math_graph_get_node(editor->graph, mul_id);
    if(mul) { math_graph_set_name(editor->graph, mul_id, "Multiply"); }
    math_editor_add_view(editor, mul_id, 250, 175);
    
    MathNodeId sin_id = math_graph_add_node(editor->graph, MATH_NODE_SIN);
    MathNode* s = math_graph_get_node(editor->graph, sin_id);
    if(s) { math_graph_set_name(editor->graph, sin_id, "Sin"); }
    math_editor_add_view(editor, sin_id, 450, 175);
    
    math_graph_connect(editor->graph, mul_id, 0, uv_id); 
    math_graph_connect(editor->graph, mul_id, 1, freq_id); 
    math_graph_connect(editor->graph, sin_id, 0, mul_id);
    
    math_editor_sync_view_data(editor);
}

static void math_editor_load_palette(MathEditor* editor, const char* path) {
    char* content = fs_read_text(NULL, path);
    if (!content) {
        LOG_WARN("Failed to load palette config: %s", path);
        return;
    }
    
    ConfigNode* root = NULL;
    ConfigError err = {0};
    
    // Parse into the graph arena (persists until editor destroy)
    if (!simple_yaml_parse(&editor->graph_arena, content, &root, &err)) {
        LOG_ERROR("YAML Parse Error in %s line %d: %s", path, err.line, err.message);
        free(content);
        return;
    }
    
    free(content); // Raw text no longer needed
    
    if (!root || root->type != CONFIG_NODE_MAP) {
        return;
    }
    
    const ConfigNode* items_node = config_node_map_get(root, "items");
    if (items_node && items_node->type == CONFIG_NODE_SEQUENCE) {
        // Allocate palette array
        editor->palette_items = arena_alloc_zero(&editor->graph_arena, 64 * sizeof(MathNodePaletteItem));
        editor->palette_count = 0;
        
        for (size_t i = 0; i < items_node->item_count && editor->palette_count < 64; ++i) {
             ConfigNode* item_node = items_node->items[i];
             if (item_node->type == CONFIG_NODE_MAP) {
                 MathNodePaletteItem* pal_item = &editor->palette_items[editor->palette_count++];
                 
                 const ConfigNode* label_node = config_node_map_get(item_node, "label");
                 if (label_node && label_node->scalar) {
                     strncpy(pal_item->label, label_node->scalar, 31);
                 }
                 
                 const ConfigNode* type_node = config_node_map_get(item_node, "type");
                 if (type_node && type_node->scalar) {
                      const MetaEnum* e = meta_get_enum("MathNodeType");
                      int val = 0;
                      if (e && meta_enum_get_value(e, type_node->scalar, &val)) {
                          pal_item->type = val;
                      } else {
                          LOG_WARN("Unknown node type in palette: %s", type_node->scalar);
                      }
                 }
             }
        }
    }
    
    LOG_INFO("Editor: Loaded %d palette items from %s", editor->palette_count, path);
}

MathEditor* math_editor_create(Engine* engine) {
    MathEditor* editor = (MathEditor*)calloc(1, sizeof(MathEditor));
    if (!editor) return NULL;

    // 1. Init Memory
    arena_init(&editor->graph_arena, 1024 * 1024); // 1MB for Graph Data
    // Use factory function (Heap alloc via Arena)
    editor->graph = math_graph_create(&editor->graph_arena);
    
    editor->node_views = NULL;
    editor->node_view_count = 0;
    editor->node_view_cap = 0;
    
    editor->selected_node_id = MATH_NODE_INVALID_ID;
    
    // 2. Setup Default Data
    math_editor_setup_default_graph(editor);
    math_editor_load_palette(editor, "assets/ui/palette_config.yaml");

    // 3. Init UI System
    ui_command_init();
    UI_REGISTER_COMMAND("Graph.AddNode", cmd_add_node, editor);
    UI_REGISTER_COMMAND("Graph.Clear", cmd_clear_graph, editor);
    UI_REGISTER_COMMAND("Graph.Recompile", cmd_recompile, editor);

    editor->input_ctx = ui_input_create();

    // 4. Load UI Asset
    const char* ui_path = engine_get_config(engine)->ui_path; // Use config path
    if (ui_path) {
        editor->ui_asset = ui_parser_load_from_file(ui_path);
        if (!editor->ui_asset) {
             LOG_ERROR("Failed to load UI asset: %s", ui_path);
        }
    } else {
        editor->ui_asset = NULL;
    }

    editor->ui_instance = ui_instance_create(editor->ui_asset, 1024 * 1024); // 1MB for UI Elements

    if (editor->ui_asset) {
            // NOTE: We now bind MathEditor, not MathGraph!
            const MetaStruct* editor_meta = meta_get_struct("MathEditor");
            if (!editor_meta) {
                 LOG_ERROR("MathEditor meta not found! Did you run codegen?");
            }
            
            // Build Static UI from Asset into Instance
            UiElement* root = ui_element_create(editor->ui_instance, ui_asset_get_root(editor->ui_asset), editor, editor_meta);
            ui_instance_set_root(editor->ui_instance, root);
            
            // Initial Select
            if (editor->node_view_count > 0) {
                editor->selected_node_id = editor->node_views[0].node_id;
                math_editor_update_selection(editor);
            }
    }

    // 5. Initial Compute Compile
    engine_set_show_compute(engine, true);
    render_system_set_show_compute(engine_get_render_system(engine), true);
    math_editor_recompile_graph(editor, engine_get_render_system(engine));

    // 6. Input Mappings
    InputSystem* input = engine_get_input_system(engine);
    if (input) {
        input_map_action(input, "ToggleCompute", INPUT_KEY_C, INPUT_MOD_NONE);
    }
    
    return editor;
}

void math_editor_render(MathEditor* editor, Scene* scene, const struct Assets* assets, MemoryArena* arena) {
    UiElement* root = ui_instance_get_root(editor->ui_instance);
    if (!editor || !scene || !root) return;
    
    // Render UI Tree to Scene
    ui_instance_render(editor->ui_instance, scene, assets, arena);
}

void math_editor_update(MathEditor* editor, Engine* engine) {
    if (!editor) return;
    
    // Sync Logic -> View (one way binding for visual updates)
    math_editor_sync_view_data(editor);

    // Toggle Visualizer (Hotkey C) - Action Based
    if (input_is_action_just_pressed(engine_get_input_system(engine), "ToggleCompute")) {
         bool show = !engine_get_show_compute(engine);
         engine_set_show_compute(engine, show);
         render_system_set_show_compute(engine_get_render_system(engine), show);
         if (show) {
             editor->graph_dirty = true; 
         }
    }

    UiElement* root = ui_instance_get_root(editor->ui_instance);

    // UI Update Loop
    if (root) {
        // Animation / Logic Update
        ui_element_update(root, engine_get_dt(engine));
        
        // Input Handling
        ui_input_update(editor->input_ctx, root, engine_get_input_system(engine));
        
        // Process Events
        UiEvent evt;
        while (ui_input_pop_event(editor->input_ctx, &evt)) {
            switch (evt.type) {
                case UI_EVENT_VALUE_CHANGE:
                case UI_EVENT_DRAG_END:
                    editor->graph_dirty = true;
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
                            editor->selected_node_id = v->node_id;
                            editor->selection_dirty = true;
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
        if (editor->selection_dirty) {
            math_editor_update_selection(editor);
            editor->selection_dirty = false;
        }
        
        // Layout
        PlatformWindowSize size = platform_get_framebuffer_size(engine_get_window(engine));
        ui_instance_layout(editor->ui_instance, (float)size.width, (float)size.height, render_system_get_frame_count(engine_get_render_system(engine)), text_measure_wrapper, (void*)assets_get_font(engine_get_assets(engine)));
    }

    // Graph Evaluation (Naive interpretation on CPU for debugging/node values)
    if (editor->graph) { // Check pointer
        for (uint32_t i = 0; i < editor->graph->node_count; ++i) { // Use ->
            const MathNode* n = math_graph_get_node(editor->graph, i);
            if (n && n->type != MATH_NODE_NONE) {
                math_graph_evaluate(editor->graph, i);
            }
        }
    }

    // Recompile Compute Shader if dirty
    if (editor->graph_dirty && engine_get_show_compute(engine)) {
        math_editor_recompile_graph(editor, engine_get_render_system(engine));
        editor->graph_dirty = false;
    }
}

void math_editor_destroy(MathEditor* editor) {
    if (!editor) return;
    
    ui_input_destroy(editor->input_ctx);
    ui_instance_free(editor->ui_instance);
    
    // Explicitly destroy graph resources (pool, ptrs)
    math_graph_destroy(editor->graph);
    
    arena_destroy(&editor->graph_arena);
    
    if (editor->ui_asset) ui_asset_free(editor->ui_asset);

    free(editor);
}
