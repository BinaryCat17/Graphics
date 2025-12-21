#include "math_editor.h"
#include "engine/core/engine.h"
#include "internal/math_editor_internal.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/meta/reflection.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/config/config_system.h"
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

static void math_editor_sync_view_data(MathEditor* editor) {
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
    
    editor->has_selection = (editor->selected_nodes_count > 0);
    editor->no_selection = !editor->has_selection;

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
    
    // 1. Clear Logic
    math_graph_clear(ctx->graph);
    
    // 2. Clear View
    // We don't free node_views memory, just reset count. 
    // The memory will be reused.
    ctx->node_views_count = 0;
    
    // 3. Clear Selection
    ctx->selected_node_id = MATH_NODE_INVALID_ID;
    ctx->selection_dirty = true; // Trigger inspector clear
    
    // 4. Trigger UI Refresh
    math_editor_refresh_graph_view(ctx);
    
    // 5. Trigger Compute Recompile (empty graph)
    ctx->graph_dirty = true;
}

UI_COMMAND(cmd_recompile, MathEditor) {
    ctx->graph_dirty = true;
}

// --- Lifecycle ---

static void math_editor_load_graph(MathEditor* editor, const char* path) {
    LOG_INFO("Editor: Loading graph from %s", path);
    char* content = fs_read_text(NULL, path);
    if (!content) {
        LOG_WARN("Failed to load graph: %s. Using fallback.", path);
        // Fallback: simple UV node
        MathNodeId uv_id = math_graph_add_node(editor->graph, MATH_NODE_UV);
        math_editor_add_view(editor, uv_id, 100, 100);
        return;
    }

    ConfigNode* root = NULL;
    ConfigError err = {0};
    
    // Parse into the graph arena
    if (!simple_yaml_parse(&editor->graph_arena, content, &root, &err)) {
        LOG_ERROR("YAML Error in %s: %s", path, err.message);
        free(content);
        return;
    }
    free(content);

    const ConfigNode* nodes_list = config_node_map_get(root, "nodes");
    if (!nodes_list || nodes_list->type != CONFIG_NODE_SEQUENCE) return;

    size_t count = nodes_list->item_count;
    if (count == 0) return;

    MathNodeBlueprint* bps = arena_alloc_zero(&editor->graph_arena, count * sizeof(MathNodeBlueprint));
    const MetaStruct* meta_bp = meta_get_struct("MathNodeBlueprint");
    if (!meta_bp) {
        LOG_ERROR("MathNodeBlueprint meta not found!");
        return;
    }

    // 1. Load Blueprints
    for(size_t i=0; i<count; ++i) {
        // Init Defaults
        bps[i].logic.input_0 = -1;
        bps[i].logic.input_1 = -1;
        bps[i].logic.input_2 = -1;
        bps[i].logic.input_3 = -1;
        
        config_load_struct(nodes_list->items[i], meta_bp, &bps[i], &editor->graph_arena);
    }

    // 2. Create Nodes
    MathNodeId* id_map = arena_alloc(&editor->graph_arena, count * sizeof(MathNodeId)); 
    
    for(size_t i=0; i<count; ++i) {
        MathNodeBlueprint* bp = &bps[i];
        
        // Ensure type is valid (defaults to 0 which might be MATH_NODE_NONE)
        if (bp->logic.type == 0) bp->logic.type = MATH_NODE_VALUE; 

        MathNodeId id = math_graph_add_node(editor->graph, bp->logic.type);
        id_map[i] = id;
        
        // Logic Properties
        math_graph_set_value(editor->graph, id, bp->logic.value);
        math_graph_set_name(editor->graph, id, bp->layout.name); 
        
        // View Properties
        math_editor_add_view(editor, id, bp->layout.x, bp->layout.y);
    }

    // 3. Connect Nodes
    for(size_t i=0; i<count; ++i) {
        MathNodeBlueprint* bp = &bps[i];
        MathNodeId target_id = id_map[i];

        int inputs[4] = {bp->logic.input_0, bp->logic.input_1, bp->logic.input_2, bp->logic.input_3};
        
        for(int k=0; k<4; ++k) {
            int source_idx = inputs[k];
            if (source_idx >= 0 && source_idx < (int)count) {
                math_graph_connect(editor->graph, target_id, k, id_map[source_idx]);
            }
        }
    }
    
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
    if (items_node) {
        const MetaStruct* meta = meta_get_struct("MathNodePaletteItem");
        if (meta) {
            config_load_struct_array(items_node, meta, (void***)&editor->palette_items, &editor->palette_items_count, &editor->graph_arena);
            LOG_INFO("Editor: Loaded %zu palette items from %s", editor->palette_items_count, path);
        } else {
            LOG_ERROR("MathNodePaletteItem meta not found! Check codegen.");
        }
    }
}

MathEditor* math_editor_create(Engine* engine) {
    MathEditor* editor = (MathEditor*)calloc(1, sizeof(MathEditor));
    if (!editor) return NULL;

    // Allocate selection array (Capacity 1 for now)
    editor->selected_nodes = (MathNode**)calloc(1, sizeof(MathNode*));

    // 1. Init Memory
    arena_init(&editor->graph_arena, 1024 * 1024); // 1MB for Graph Data
    // Use factory function (Heap alloc via Arena)
    editor->graph = math_graph_create(&editor->graph_arena);
    
    editor->node_views = NULL;
    editor->node_views_count = 0;
    editor->node_view_cap = 0;
    
    editor->selected_node_id = MATH_NODE_INVALID_ID;
    editor->has_selection = false;
    editor->no_selection = true;
    
    // 2. Setup Default Data
    math_editor_load_graph(editor, "assets/ui/default_graph.yaml");
    math_editor_load_palette(editor, "assets/ui/palette_config.yaml");

    // 3. Init UI System
    // ui_command_init(); // Managed by Engine Core
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
            if (editor->node_views_count > 0) {
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

#include "engine/scene/scene.h"

// --- Helper: Find View ---
static MathNodeView* math_editor_find_view(MathEditor* editor, MathNodeId id) {
    for(uint32_t i=0; i<editor->node_views_count; ++i) {
        if (editor->node_views[i].node_id == id) {
            return &editor->node_views[i];
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

static void math_editor_render_ports(MathEditor* editor, Scene* scene, Vec4 clip_rect) {
    if (!editor || !editor->graph || !scene) return;

    for (uint32_t i = 0; i < editor->node_views_count; ++i) {
        MathNodeView* view = &editor->node_views[i];
        MathNode* node = math_graph_get_node(editor->graph, view->node_id);
        if (!node) continue;

        int input_count = get_node_input_count(node->type);
        
        // Render Inputs
        for (int k = 0; k < input_count; ++k) {
            float x = view->x + clip_rect.x;
            float y = view->y + 45.0f + (k * 25.0f) + clip_rect.y;
            
            SceneObject port = {0};
            port.id = (node->id << 8) | (k + 1); // Pseudo ID
            port.layer = LAYER_UI_CONTENT; // Draw on top of background but below text? Or just content.
            port.prim_type = SCENE_PRIM_QUAD;
            port.position = (Vec3){x, y, 0.0f};
            port.scale = (Vec3){10.0f, 10.0f, 1.0f};
            port.color = (Vec4){0.5f, 0.5f, 0.5f, 1.0f}; // Grey
            port.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f};
            port.ui.clip_rect = clip_rect;
            
            // SDF Circle
            port.ui.style_params.x = 4.0f; // SCENE_MODE_SDF_BOX
            port.ui.style_params.y = 5.0f; // Radius (Half size)
            port.ui.style_params.z = 1.0f; // Border thickness
            
            scene_add_object(scene, port);
        }

        // Render Output (All nodes have 1 output except OUTPUT node? Actually OUTPUT node consumes input.
        // Value nodes have output. Math ops have output.
        // Let's assume all nodes except OUTPUT have an output port on the right.
        if (node->type != MATH_NODE_OUTPUT) {
            float x = view->x + 150.0f + clip_rect.x;
            float y = view->y + 45.0f + clip_rect.y;
            
            SceneObject port = {0};
            port.id = (node->id << 8) | 0xFF; // Pseudo ID
            port.layer = LAYER_UI_CONTENT;
            port.prim_type = SCENE_PRIM_QUAD;
            port.position = (Vec3){x, y, 0.0f};
            port.scale = (Vec3){10.0f, 10.0f, 1.0f};
            port.color = (Vec4){0.5f, 0.5f, 0.5f, 1.0f};
            port.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f};
            port.ui.clip_rect = clip_rect;
            
            port.ui.style_params.x = 4.0f; // SCENE_MODE_SDF_BOX
            port.ui.style_params.y = 5.0f; // Radius
            port.ui.style_params.z = 1.0f; // Border
            
            scene_add_object(scene, port);
        }
    }
}

static void math_editor_render_connections(MathEditor* editor, Scene* scene, Vec4 clip_rect) {
    if (!editor || !editor->graph || !scene) return;

    for (uint32_t i = 0; i < editor->graph->node_count; ++i) {
        MathNode* target_node = math_graph_get_node(editor->graph, i);
        if (!target_node) continue;
        
        MathNodeView* target_view = math_editor_find_view(editor, target_node->id);
        if (!target_view) continue;

        // Draw connections for each input
        for (int k = 0; k < MATH_NODE_MAX_INPUTS; ++k) {
            MathNodeId source_id = target_node->inputs[k];
            if (source_id == MATH_NODE_INVALID_ID) continue;
            
            MathNodeView* source_view = math_editor_find_view(editor, source_id);
            if (!source_view) continue;

            // Calculate endpoints (Must match render_ports logic)
            float start_x = source_view->x + 150.0f + clip_rect.x; 
            float start_y = source_view->y + 45.0f + clip_rect.y; 
            
            float end_x = target_view->x + clip_rect.x;
            float end_y = target_view->y + 45.0f + (k * 25.0f) + clip_rect.y;
            
            // Bounding Box
            float min_x = start_x < end_x ? start_x : end_x;
            float max_x = start_x > end_x ? start_x : end_x;
            float min_y = start_y < end_y ? start_y : end_y;
            float max_y = start_y > end_y ? start_y : end_y;
            
            float padding = 50.0f;
            min_x -= padding; min_y -= padding;
            max_x += padding; max_y += padding;
            
            float width = max_x - min_x;
            float height = max_y - min_y;
            
            if (width < 1.0f) width = 1.0f;
            if (height < 1.0f) height = 1.0f;

            // Normalize Points to 0..1 relative to Quad
            float u1 = (start_x - min_x) / width;
            float v1 = (start_y - min_y) / height;
            float u2 = (end_x - min_x) / width;
            float v2 = (end_y - min_y) / height;

            SceneObject wire = {0};
            wire.id = (target_node->id << 16) | (source_id & 0xFFFF); 
            wire.layer = LAYER_UI_BACKGROUND;
            wire.prim_type = SCENE_PRIM_CURVE; 
            wire.position = (Vec3){min_x + width*0.5f, min_y + height*0.5f, 0.0f};
            wire.scale = (Vec3){width, height, 1.0f};
            wire.color = (Vec4){0.8f, 0.8f, 0.8f, 1.0f}; 
            
            // Fix: Set UV Rect so shader receives correct UVs
            wire.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f};
            wire.ui.clip_rect = clip_rect;

            wire.ui.style_params.y = 1.0f; // Curve Type
            wire.ui.extra_params = (Vec4){u1, v1, u2, v2};
            wire.ui.style_params.z = 3.0f / height; 
            wire.ui.style_params.w = width / height; // AR
            
            scene_add_object(scene, wire);
        }
    }
}

void math_editor_render(MathEditor* editor, Scene* scene, const struct Assets* assets, MemoryArena* arena) {
    UiElement* root = ui_instance_get_root(editor->ui_instance);
    if (!editor || !scene || !root) return;

    // Resolve Clipping Rect from Canvas Area
    Vec4 canvas_clip = {-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    UiElement* canvas_area = ui_element_find_by_id(root, "canvas_area");
    if (canvas_area) {
        Rect r = ui_element_get_screen_rect(canvas_area);
        canvas_clip = (Vec4){r.x, r.y, r.w, r.h};
    }
    
    // 1. Render Connections (Background)
    math_editor_render_connections(editor, scene, canvas_clip);
    
    // 2. Render UI Tree to Scene
    ui_instance_render(editor->ui_instance, scene, assets, arena);

    // 3. Render Ports (Overlay/Content) - Drawn last to be on top
    math_editor_render_ports(editor, scene, canvas_clip);
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

    if (editor->selected_nodes) free((void*)editor->selected_nodes);
    if (editor->palette_items) {
        // Items are in arena, but array pointer might be arena or heap? 
        // config_load_struct_array uses arena. So no free needed for palette_items if arena is destroyed.
        // But selected_nodes was malloc'd.
    }

    free(editor);
}
