#include "ui_scene_bridge.h"
#include <stddef.h>

static void traverse_ui(const UiView* view, Scene* scene, const Assets* assets) {
    if (!view) return;

    // Create Scene Object for this view
    if (view->rect.w > 0 && view->rect.h > 0) {
        SceneObject obj = {0};
        obj.layer = LAYER_UI_CONTENT;
        obj.position = (Vec3){view->rect.x, view->rect.y, 0.0f};
        obj.scale = (Vec3){view->rect.w, view->rect.h, 1.0f};
        obj.rotation = (Vec3){0, 0, 0};
        obj.mesh = &assets->unit_quad;
        
        // Color based on type
        if (view->def->type == UI_NODE_PANEL) {
            obj.color = (Vec4){0.2f, 0.2f, 0.2f, 1.0f};
        } else if (view->def->type == UI_NODE_BUTTON) {
            obj.color = (Vec4){0.3f, 0.5f, 0.8f, 1.0f};
        } else if (view->def->type == UI_NODE_SLIDER) {
            obj.color = (Vec4){0.4f, 0.4f, 0.4f, 1.0f};
        } else if (view->def->type == UI_NODE_LABEL) {
             obj.color = (Vec4){0, 0, 0, 0}; // Transparent
        } else {
            obj.color = (Vec4){1.0f, 0.0f, 1.0f, 1.0f}; // Debug magenta
        }
        
        // Add to scene
        scene_add_object(scene, obj);
    }

    for (size_t i = 0; i < view->child_count; ++i) {
        traverse_ui(view->children[i], scene, assets);
    }
}

void ui_build_scene(const UiView* root, Scene* scene, const Assets* assets) {
    if (!root || !scene || !assets) return;
    traverse_ui(root, scene, assets);
}
