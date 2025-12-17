#include "engine/ui/ui_scene_bridge.h"
#include "foundation/logger/logger.h"
#include "engine/graphics/renderer_backend.h"
#include "engine/graphics/font.h"
#include "engine/graphics/scene.h"
#include "engine/graphics/text_renderer.h" 

#include <string.h>
#include <stdio.h>

static void process_node(const UiView* view, Scene* scene, const Assets* assets) {
    if (!view || !view->def) return;

    // Calculate Absolute Position (Simplified, assuming layout wrote absolute rects or we rely on them)
    float abs_x = view->rect.x;
    float abs_y = view->rect.y;

    // Render based on type
    if (view->def->type == UI_NODE_PANEL) {
        SceneObject obj = {0};
        obj.id = (unsigned int)view->id_hash;
        obj.position = (Vec3){abs_x, abs_y, 0.5f}; // Layer 0.5
        obj.scale = (Vec3){view->rect.w, view->rect.h, 1.0f};
        obj.color = (Vec4){0.2f, 0.2f, 0.2f, 1.0f}; // Default Gray
        
        // Style overrides
        if (view->def->style_name && strcmp(view->def->style_name, "node_header") == 0) {
            obj.color = (Vec4){0.3f, 0.3f, 0.4f, 1.0f};
        } else if (view->def->style_name && strcmp(view->def->style_name, "node_body") == 0) {
            obj.color = (Vec4){0.15f, 0.15f, 0.15f, 0.9f};
        } else if (view->is_hovered) {
             obj.color = (Vec4){0.25f, 0.25f, 0.25f, 1.0f};
        }

        obj.prim_type = SCENE_PRIM_QUAD;
        obj.uv_rect = (Vec4){0, 0, 1, 1};
        obj.params.x = 0.0f; // No texture

        scene_add_object(scene, obj);
    } 
    else if (view->def->type == UI_NODE_LABEL && view->cached_text) {
        scene_add_text(scene, view->cached_text, (Vec3){abs_x, abs_y, 0.0f}, 0.7f, (Vec4){1,1,1,1});
    }
    else if (view->def->type == UI_NODE_BUTTON) {
        SceneObject obj = {0};
        obj.id = (unsigned int)view->id_hash;
        obj.position = (Vec3){abs_x, abs_y, 0.6f};
        obj.scale = (Vec3){view->rect.w, view->rect.h, 1.0f};
        
        obj.color = view->is_pressed ? (Vec4){0.5f, 0.5f, 0.5f, 1.0f} : 
                    view->is_hovered ? (Vec4){0.4f, 0.4f, 0.4f, 1.0f} : 
                                       (Vec4){0.3f, 0.3f, 0.3f, 1.0f};
                                       
        obj.prim_type = SCENE_PRIM_QUAD;
        scene_add_object(scene, obj);
        
        // Button Text
        if (view->cached_text) {
            float text_w = font_measure_text(view->cached_text) * 0.7f; // Scale 0.7
            float off_x = (view->rect.w - text_w) * 0.5f;
            float off_y = (view->rect.h - 14.0f) * 0.5f;
            scene_add_text(scene, view->cached_text, (Vec3){abs_x + off_x, abs_y + off_y, 0.0f}, 0.7f, (Vec4){1,1,1,1});
        }
    }
    else if (view->def->type == UI_NODE_CURVE) {
        SceneObject obj = {0};
        obj.id = (unsigned int)view->id_hash;
        obj.position = (Vec3){abs_x, abs_y, 0.4f}; 
        obj.scale = (Vec3){view->rect.w, view->rect.h, 1.0f};
        obj.color = (Vec4){1.0f, 0.8f, 0.2f, 1.0f}; 
        obj.prim_type = SCENE_PRIM_CURVE;
        obj.uv_rect = (Vec4){0, 0, 1, 1}; 
        
        scene_add_object(scene, obj);
    }

    // Children
    for (size_t i = 0; i < view->child_count; ++i) {
        process_node(view->children[i], scene, assets);
    }
}

void ui_build_scene(const UiView* root, Scene* scene, const Assets* assets) {
    if (!root || !scene) return;
    process_node(root, scene, assets);
}
