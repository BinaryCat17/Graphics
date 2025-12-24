#include "math_editor.h"
#include "engine/core/engine.h"
#include "internal/math_editor_internal.h"
#include "internal/math_editor_view.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/meta/reflection.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/config/config_system.h"
#include "features/math_engine/internal/transpiler.h"
#include "features/math_engine/internal/math_graph_internal.h" // Access to internal Graph/Node structs
#include "engine/graphics/internal/renderer_backend.h"
#include "engine/graphics/layer_constants.h"
#include "engine/graphics/stream.h"
#include "engine/graphics/compute_graph.h"
#include "engine/graphics/gpu_input.h"
#include "engine/text/font.h"
#include "engine/assets/assets.h"
#include "engine/graphics/render_system.h"
#include "engine/input/input.h"
#include "engine/scene/render_packet.h"

#include "engine/ui/ui_core.h"
#include "engine/ui/ui_input.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- GPU Data Structures ---

typedef struct GpuNodeData {
    Vec2 pos;
    Vec2 size;
    uint32_t id;
    uint32_t padding;
} GpuNodeData;

// --- Helper: Text Measurement for UI Layout ---
static Vec2 text_measure_wrapper(const char* text, float scale, void* user_data) {
    const Font* font = (const Font*)user_data;
    float w = font_measure_text(font, text) * scale;
    return (Vec2){w, 20.0f * scale}; // Fixed height for now
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

// --- Commands ---

UI_COMMAND(cmd_add_node, MathEditor) {
    LOG_INFO("Command: Graph.AddNode");
    
    MathNodeType type = MATH_NODE_VALUE;

    // Try to extract type from PaletteItem data bound to the UI element
    if (target) {
        void* data = scene_node_get_data(target);
        const MetaStruct* meta = scene_node_get_meta(target);
        
        // If the button itself doesn't have data, check parent
        if (!data) {
             SceneNode* p = scene_node_get_parent(target);
             if(p) {
                 data = scene_node_get_data(p);
                 meta = scene_node_get_meta(p);
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
    ctx->view->node_views_count = 0;
    
    // 3. Clear Selection
    ctx->view->selected_node_id = MATH_NODE_INVALID_ID;
    ctx->view->selection_dirty = true; // Trigger inspector clear
    
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

    // 1. Init Memory
    arena_init(&editor->graph_arena, 1024 * 1024); // 1MB for Graph Data
    // Use factory function (Heap alloc via Arena)
    editor->graph = math_graph_create(&editor->graph_arena);

    // Init View
    editor->view = arena_alloc_zero(&editor->graph_arena, sizeof(MathGraphView));
    
    // Allocate selection array (Capacity 1 for now)
    editor->view->selected_nodes = (MathNode**)calloc(1, sizeof(MathNode*));
    
    // Wires Buffer
    editor->view->wires_cap = 1024;
    editor->view->wires = arena_alloc_zero(&editor->graph_arena, editor->view->wires_cap * sizeof(MathWireView));

    editor->view->node_views = NULL;
    editor->view->node_views_count = 0;
    editor->view->node_view_cap = 0;
    
    editor->view->selected_node_id = MATH_NODE_INVALID_ID;
    editor->view->has_selection = false;
    editor->view->no_selection = true;
    
    // 2. Setup Default Data
    math_editor_load_graph(editor, "assets/ui/default_graph.yaml");
    math_editor_load_palette(editor, "assets/ui/palette_config.yaml");

    // Sync View Models (Nodes & Wires) before UI creation
    math_editor_sync_view_data(editor);
    math_editor_sync_wires(editor);

    // 3. Init UI System
    // ui_command_init(); // Managed by Engine Core
    UI_REGISTER_COMMAND("Graph.AddNode", cmd_add_node, editor);
    UI_REGISTER_COMMAND("Graph.Clear", cmd_clear_graph, editor);
    UI_REGISTER_COMMAND("Graph.Recompile", cmd_recompile, editor);
    
    // ui_register_provider("GraphNetwork", math_graph_view_provider); // Removed: All UI is declarative now

    editor->view->input_ctx = ui_input_create();

    // 4. Load UI Asset
    const char* ui_path_raw = engine_get_config(engine)->ui_path; 
    const char* assets_root = assets_get_root_dir(engine_get_assets(engine));
    
    // Normalize path: if it starts with "assets/", strip it to get relative path for the Asset System
    const char* ui_rel_path = ui_path_raw;
    if (ui_path_raw && assets_root) {
        size_t root_len = strlen(assets_root);
        if (strncmp(ui_path_raw, assets_root, root_len) == 0) {
            if (ui_path_raw[root_len] == '/' || ui_path_raw[root_len] == '\\') {
                ui_rel_path = ui_path_raw + root_len + 1;
            }
        }
    }

    if (ui_rel_path) {
        editor->view->ui_asset = assets_load_scene(engine_get_assets(engine), ui_rel_path);
        if (!editor->view->ui_asset) {
             LOG_ERROR("Failed to load UI asset: %s (Raw: %s)", ui_rel_path, ui_path_raw);
        }
    } else {
        editor->view->ui_asset = NULL;
    }

    editor->view->ui_instance = scene_tree_create(editor->view->ui_asset, 1024 * 1024); // 1MB for UI Elements

    if (editor->view->ui_asset) {
            // NOTE: We now bind MathEditor, not MathGraph!
            const MetaStruct* editor_meta = meta_get_struct("MathEditor");
            if (!editor_meta) {
                 LOG_ERROR("MathEditor meta not found! Did you run codegen?");
            }
            
            // Build Static UI from Asset into Instance
            SceneNode* root = ui_node_create(editor->view->ui_instance, scene_asset_get_root(editor->view->ui_asset), editor, editor_meta);
            scene_tree_set_root(editor->view->ui_instance, root);
            
            // Initial Select
            if (editor->view->node_views_count > 0) {
                editor->view->selected_node_id = editor->view->node_views[0].node_id;
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

    // 7. GPU Picking Initialization
    editor->gpu_nodes = stream_create(engine_get_render_system(engine), STREAM_CUSTOM, 4096, sizeof(GpuNodeData));
    editor->gpu_picking_result = stream_create(engine_get_render_system(engine), STREAM_UINT, 1, 0);

    // Wires Streams
    editor->gpu_wires = stream_create(engine_get_render_system(engine), STREAM_CUSTOM, 1024, sizeof(MathWireView)); 
    editor->gpu_wire_verts = stream_create(engine_get_render_system(engine), STREAM_CUSTOM, 1024 * 64 * 6, 32); 
    
    // Create Pipeline
    AssetData spv = assets_load_file(engine_get_assets(engine), "shaders/compute/picking.comp.spv");
    if (spv.data) {
        editor->picking_pipeline_id = render_system_create_compute_pipeline(engine_get_render_system(engine), (uint32_t*)spv.data, spv.size);
        assets_free_file(&spv);
    } else {
        LOG_ERROR("Failed to load picking shader: shaders/compute/picking.comp.spv. Run tools/build_shaders.py");
    }

    // Wires Compute Pipeline
    AssetData wire_comp = assets_load_file(engine_get_assets(engine), "shaders/compute/wires.comp.spv");
    if (wire_comp.data) {
        editor->wire_compute_pipeline_id = render_system_create_compute_pipeline(engine_get_render_system(engine), (uint32_t*)wire_comp.data, wire_comp.size);
        assets_free_file(&wire_comp);
    } else {
        LOG_ERROR("Failed to load wires compute shader.");
    }

    // Wires Render Pipeline
    AssetData wire_vert = assets_load_file(engine_get_assets(engine), "shaders/render_wires.vert.spv");
    AssetData wire_frag = assets_load_file(engine_get_assets(engine), "shaders/render_wires.frag.spv");
    if (wire_vert.data && wire_frag.data) {
        editor->wire_render_pipeline_id = render_system_create_graphics_pipeline(
            engine_get_render_system(engine), 
            wire_vert.data, wire_vert.size, 
            wire_frag.data, wire_frag.size, 
            1 // Zero-Copy Layout
        );
        assets_free_file(&wire_vert);
        assets_free_file(&wire_frag);
    }

    // Create Compute Graph
    if (editor->picking_pipeline_id != 0) {
        editor->gpu_compute_graph = compute_graph_create();
        editor->picking_pass = compute_graph_add_pass(editor->gpu_compute_graph, editor->picking_pipeline_id, 1, 1, 1);
        compute_pass_bind_stream(editor->picking_pass, 0, editor->gpu_nodes);
        compute_pass_bind_stream(editor->picking_pass, 1, editor->gpu_picking_result);
    }

    if (editor->gpu_compute_graph && editor->wire_compute_pipeline_id != 0) {
        editor->wire_pass = compute_graph_add_pass(editor->gpu_compute_graph, editor->wire_compute_pipeline_id, 1, 1, 1);
        compute_pass_bind_stream(editor->wire_pass, 0, editor->gpu_wires);
        compute_pass_bind_stream(editor->wire_pass, 1, editor->gpu_wire_verts);
    }
    
    // 8. Graphics Pipeline for Nodes
    AssetData vert = assets_load_file(engine_get_assets(engine), "shaders/editor_nodes.vert.spv");
    AssetData frag = assets_load_file(engine_get_assets(engine), "shaders/editor_nodes.frag.spv");
    if (vert.data && frag.data) {
        editor->nodes_pipeline_id = render_system_create_graphics_pipeline(
            engine_get_render_system(engine), 
            vert.data, vert.size, 
            frag.data, frag.size, 
            1 // Zero-Copy Layout
        );
        assets_free_file(&vert);
        assets_free_file(&frag);
    } else {
        LOG_ERROR("Failed to load editor_nodes shaders.");
    }

    editor->draw_data_cache = arena_alloc_zero(&editor->graph_arena, sizeof(CustomDrawData));
    editor->wire_draw_data = arena_alloc_zero(&editor->graph_arena, sizeof(CustomDrawData));
    
    return editor;
}

#include "engine/scene/render_packet.h"

void math_editor_render(MathEditor* editor, Scene* scene, const struct Assets* assets, MemoryArena* arena) {
    if (!editor || !scene || !editor->view->ui_instance) return;

    // 1. Render Graph Nodes (GPU Instancing)
    if (editor->nodes_pipeline_id > 0 && editor->view->node_views_count > 0 && editor->draw_data_cache) {
        editor->draw_data_cache->pipeline_id = editor->nodes_pipeline_id;
        editor->draw_data_cache->vertex_count = 6; // Quad
        editor->draw_data_cache->instance_count = editor->view->node_views_count;
        editor->draw_data_cache->buffers[0] = stream_get_handle(editor->gpu_nodes); 
        
        SceneObject obj = {0};
        obj.prim_type = SCENE_PRIM_CUSTOM;
        obj.layer = LAYER_UI_BACKGROUND; // Draw behind standard UI
        obj.instance_buffer = editor->draw_data_cache;
        
        scene_add_object(scene, obj);
    }

    // 1.5 Render Wires (Custom Compute Generated)
    if (editor->wire_render_pipeline_id > 0 && editor->view->wires_count > 0 && editor->wire_draw_data) {
        editor->wire_draw_data->pipeline_id = editor->wire_render_pipeline_id;
        editor->wire_draw_data->vertex_count = editor->view->wires_count * 64 * 6; // 64 segments * 6 verts
        editor->wire_draw_data->instance_count = 1;
        editor->wire_draw_data->buffers[0] = stream_get_handle(editor->gpu_wire_verts);
        
        SceneObject obj = {0};
        obj.prim_type = SCENE_PRIM_CUSTOM;
        obj.layer = LAYER_UI_BACKGROUND; // Behind Nodes but maybe same layer?
        obj.instance_buffer = editor->wire_draw_data;
        
        scene_add_object(scene, obj);
    }

    // 2. Render UI Overlay (Inspector, Palette, Wireframe Links)
    // Links are currently drawn by UI system as lines?
    // math_editor_sync_wires -> editor->view->wires.
    // ui_system_render -> draws scene_tree.
    // We need to ensure wires are drawn. UI system handles that if they are part of the UI tree.
    // But wires in MathEditor are separate struct?
    // Let's check ui_renderer.c. It likely only renders UI Nodes.
    // If we want wires, we should add them to scene here too.
    
    ui_system_render(editor->view->ui_instance, scene, assets, arena);
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

    SceneNode* root = scene_tree_get_root(editor->view->ui_instance);

    // UI Update Loop
    if (root) {
        // Animation / Logic Update
        ui_node_update(root, engine_get_dt(engine));
        
        // Input Handling
        ui_input_update(editor->view->input_ctx, root, engine_get_input_system(engine));
        
        // Sync Wires AFTER input (so they attach to new node positions)
        math_editor_sync_wires(editor);
        
        // Process Events
        UiEvent evt;
        while (ui_input_pop_event(editor->view->input_ctx, &evt)) {
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
                    SceneNode* hit = evt.target;
                    while (hit) {
                        // Check for MathNodeView
                        void* data = scene_node_get_data(hit);
                        const MetaStruct* meta = scene_node_get_meta(hit);

                        if (data && meta && strcmp(meta->name, "MathNodeView") == 0) {
                            MathNodeView* v = (MathNodeView*)data;
                            editor->view->selected_node_id = v->node_id;
                            editor->view->selection_dirty = true;
                            LOG_INFO("Selected Node: %d", v->node_id);
                            break;
                        }
                        hit = scene_node_get_parent(hit);
                    }
                } break;
                default: break;
            }
        }
        
        // Lazy Inspector Rebuild
        if (editor->view->selection_dirty) {
            math_editor_update_selection(editor);
            editor->view->selection_dirty = false;
        }
        
        // Layout
        PlatformWindowSize size = platform_get_framebuffer_size(engine_get_window(engine));
        ui_system_layout(editor->view->ui_instance, (float)size.width, (float)size.height, render_system_get_frame_count(engine_get_render_system(engine)), text_measure_wrapper, (void*)assets_get_font(engine_get_assets(engine)));
    }

    // Graph Evaluation (Naive interpretation on CPU for debugging/node values)
    if (editor->graph && (editor->graph_dirty || editor->view->selection_dirty)) { 
        for (uint32_t i = 0; i < editor->graph->node_count; ++i) { 
            const MathNode* n = math_graph_get_node(editor->graph, i);
            if (n && n->type != MATH_NODE_NONE) {
                math_graph_evaluate(editor->graph, i);
            }
        }
    }

    // --- GPU Picking & Wires Update ---
    if (editor->gpu_compute_graph) {
        // 0. Update Wires
        if (editor->wire_pass && editor->view->wires_count > 0) {
            // Upload Wires
            stream_set_data(editor->gpu_wires, editor->view->wires, editor->view->wires_count);
            
            // Push Constants
            struct { uint32_t count; uint32_t segs; } push = { editor->view->wires_count, 64 };
            compute_pass_set_push_constants(editor->wire_pass, &push, sizeof(push));
            
            // Dispatch
            uint32_t groups = (push.count + 63) / 64;
            compute_pass_set_dispatch_size(editor->wire_pass, groups, 1, 1);
        } else if (editor->wire_pass) {
            compute_pass_set_dispatch_size(editor->wire_pass, 0, 0, 0);
        }

        if (editor->view->node_views_count > 0) {
            // 1. Upload Node Data
        // Use scratch memory or malloc
        uint32_t count = editor->view->node_views_count;
        if (count > 4096) count = 4096;
        
        GpuNodeData* gpu_data = (GpuNodeData*)malloc(count * sizeof(GpuNodeData));
        if (gpu_data) {
            for(uint32_t i=0; i < count; ++i) {
                MathNodeView* v = &editor->view->node_views[i];
                gpu_data[i].pos = (Vec2){v->x, v->y};
                gpu_data[i].size = (Vec2){NODE_WIDTH, NODE_HEADER_HEIGHT + v->input_ports_count * NODE_PORT_SPACING}; 
                gpu_data[i].id = v->node_id;
            }
            stream_set_data(editor->gpu_nodes, gpu_data, count);
            free(gpu_data);
        }

        // 2. Setup Push Constants
        struct { Vec2 mouse; uint32_t count; } push;
        InputSystem* input = engine_get_input_system(engine);
        push.mouse = (Vec2){ input_get_mouse_x(input), input_get_mouse_y(input) };
        push.count = count;
        
        compute_pass_set_push_constants(editor->picking_pass, &push, sizeof(push));
        
        // 3. Update Dispatch Size
        uint32_t groups = (push.count + 63) / 64;
        compute_pass_set_dispatch_size(editor->picking_pass, groups, 1, 1);
        
        // 4. Reset Previous Result
        uint32_t invalid_id = MATH_NODE_INVALID_ID;
        stream_set_data(editor->gpu_picking_result, &invalid_id, 1);

        // 5. Execute
        compute_graph_execute(editor->gpu_compute_graph, engine_get_render_system(engine));
        
        // 6. Read Back
        uint32_t picked_id = MATH_NODE_INVALID_ID;
        if (stream_read_back(editor->gpu_picking_result, &picked_id, 1)) {
             if (picked_id != MATH_NODE_INVALID_ID) {
                  // Only log on click to avoid spam
                  if (input_is_mouse_down(input)) {
                       LOG_DEBUG("GPU Picked ID: %u", picked_id);
                  }
             }
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
    
    if (editor->view) {
        ui_input_destroy(editor->view->input_ctx);
        scene_tree_destroy(editor->view->ui_instance);
        if (editor->view->ui_asset) scene_asset_destroy(editor->view->ui_asset);
        if (editor->view->selected_nodes) free((void*)editor->view->selected_nodes);
    }

    // Cleanup GPU Resources
    if (editor->gpu_compute_graph) compute_graph_destroy(editor->gpu_compute_graph);
    if (editor->gpu_nodes) stream_destroy(editor->gpu_nodes);
    if (editor->gpu_picking_result) stream_destroy(editor->gpu_picking_result);
    if (editor->gpu_wires) stream_destroy(editor->gpu_wires);
    if (editor->gpu_wire_verts) stream_destroy(editor->gpu_wire_verts);
    // Note: Pipelines are managed by RenderSystem, usually no explicit destroy needed unless dynamic?
    // Actually render_system_destroy_compute_pipeline exists, we should use it.
    if (editor->picking_pipeline_id != 0) {
        // We need access to render system... but destroy doesn't pass engine/rs.
        // Assuming render system cleanups all pipelines on shutdown.
        // If hot-reloading editor, we might leak. Ideally we pass engine to destroy.
    }
    // We should also destroy graphics pipeline, but again, need sys context.
    
    // Explicitly destroy graph resources (pool, ptrs)
    math_graph_destroy(editor->graph);
    
    arena_destroy(&editor->graph_arena);
    
    if (editor->palette_items) {
        // Items are in arena, but array pointer might be arena or heap? 
        // config_load_struct_array uses arena. So no free needed for palette_items if arena is destroyed.
        // But selected_nodes was malloc'd.
    }

    free(editor);
}
