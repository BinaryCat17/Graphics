#include "ui_core.h"
#include "ui_renderer.h"
#include "ui_input.h"
#include "internal/ui_internal.h"
#include "internal/ui_command_system.h"
#include "internal/ui_layout.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"
#include "foundation/meta/reflection.h"
#include "engine/scene/scene.h"
#include "engine/scene/internal/scene_tree_internal.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static bool s_ui_initialized = false;

void ui_system_init(void) {
    if (s_ui_initialized) return;
    s_ui_initialized = true;
    LOG_INFO("UI System Initialized");
}

void ui_system_shutdown(void) {
    ui_command_shutdown();
}

static void ui_node_init_recursive(SceneNode* el, SceneTree* tree, const MetaStruct* meta) {
    if (!el || !el->spec) return;

    // UI-specific init
    el->render_color = el->spec->style.color;
    
    el->rect.x = el->spec->layout.x;
    el->rect.y = el->spec->layout.y;
    el->desired_x = el->spec->layout.x;
    el->desired_y = el->spec->layout.y;
    el->on_click_cmd_id = el->spec->on_click;
    el->on_change_cmd_id = el->spec->on_change;

    // Cache Bindings
    if (meta && el->spec->binding_count > 0) {
        el->ui_bindings = arena_alloc_zero(&tree->arena, el->spec->binding_count * sizeof(UiBinding));
        el->ui_binding_count = el->spec->binding_count;
        UiBinding* bindings = (UiBinding*)el->ui_bindings;
        
        for (size_t i = 0; i < el->spec->binding_count; ++i) {
             SceneBindingSpec* b_spec = &el->spec->bindings[i];
             size_t total_offset = 0;
             const MetaField* f = meta_find_field_by_path(meta, b_spec->source, &total_offset);
             
             if (f) {
                 bindings[i].source_field = f;
                 bindings[i].source_offset = total_offset;
                 bindings[i].target = ui_resolve_target_enum(b_spec->target);
             }
        }
    }

    // Recurse for static children
    for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
        ui_node_init_recursive(child, tree, meta);
    }

    // Populate Dynamic Children (UI-way handles collections)
    // Note: ui_node_rebuild_children calls ui_node_create internally for new nodes, 
    // so we don't need to manually recurse into them here (they are added to the end).
    ui_node_rebuild_children(el, tree);
}

SceneNode* ui_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta) {
    if (!tree || !spec) return NULL;

    // 1. Basic Scene Node (Creates entire static subtree)
    SceneNode* el = scene_node_create(tree, spec, data, meta);
    if (!el) return NULL;

    // 2. Recursive UI Init
    ui_node_init_recursive(el, tree, meta);
    
    return el;
}

void ui_node_update(SceneNode* element, float dt) {
    if (!element || !element->spec) return;
    
    // 1. Data Binding Sync
    UiBinding* bindings = (UiBinding*)element->ui_bindings;
    if (element->data_ptr && bindings) {
        for (size_t i = 0; i < element->ui_binding_count; ++i) {
             ui_apply_binding_value(element, &bindings[i]);
        }
    }

    // 2. Animation Interpolation
    const SceneNodeSpec* spec = element->spec;
    float target_t = element->is_hovered ? 1.0f : 0.0f;
    float speed = spec->style.animation_speed > 0 ? spec->style.animation_speed : 10.0f;
    
    if (element->hover_t != target_t) {
        float diff = target_t - element->hover_t;
        float step = speed * dt;
        if (fabsf(diff) < step) element->hover_t = target_t;
        else element->hover_t += (diff > 0 ? 1.0f : -1.0f) * step;
        
        // Update Animated Color
        if (spec->style.hover_color.w > 0) {
            element->render_color.x = spec->style.color.x + (spec->style.hover_color.x - spec->style.color.x) * element->hover_t;
            element->render_color.y = spec->style.color.y + (spec->style.hover_color.y - spec->style.color.y) * element->hover_t;
            element->render_color.z = spec->style.color.z + (spec->style.hover_color.z - spec->style.color.z) * element->hover_t;
            element->render_color.w = spec->style.color.w + (spec->style.hover_color.w - spec->style.color.w) * element->hover_t;
        }
    }

    // 3. Recurse
    for (SceneNode* child = element->first_child; child; child = child->next_sibling) {
        ui_node_update(child, dt);
    }
}

void ui_system_layout(SceneTree* tree, float window_w, float window_h, uint64_t frame_number, UiTextMeasureFunc measure_func, void* measure_data) {
    if (!tree || !tree->root) return;
    ui_layout_root(tree->root, window_w, window_h, frame_number, false, measure_func, measure_data);
}

void ui_system_render(SceneTree* tree, struct Scene* scene, const struct Assets* assets, struct MemoryArena* arena) {
    if (!tree || !tree->root) return;
    scene_tree_render(tree, scene, assets, arena);
}

Rect ui_node_get_screen_rect(const SceneNode* node) {
    return node ? node->screen_rect : (Rect){0};
}
