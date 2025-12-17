#include "ui_scene_bridge.h"
#include "engine/render/backend/common/renderer_backend.h"
#include <stddef.h>
#include <string.h>

static void traverse_ui(const UiView* view, Scene* scene, const Assets* assets) {
    if (!view) return;

    // Create Scene Object for this view (Background)
    if (view->rect.w > 0 && view->rect.h > 0) {
        SceneObject obj = {0};
        obj.layer = LAYER_UI_BACKGROUND; // Background layer
        obj.position = (Vec3){view->rect.x, view->rect.y, 0.0f};
        obj.scale = (Vec3){view->rect.w, view->rect.h, 1.0f};
        obj.rotation = (Vec3){0, 0, 0};
        obj.mesh = &assets->unit_quad;
        obj.params.x = 0.0f; // No texture
        
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

    // Text Rendering
    if (view->cached_text && strlen(view->cached_text) > 0) {
        RendererBackend* backend = renderer_backend_default();
        if (backend && backend->get_glyph) {
            float cursor_x = view->rect.x + 5.0f; // Padding
            float cursor_y = view->rect.y + 5.0f; // Padding (Top alignment)
            
            // Center text for buttons
            if (view->def->type == UI_NODE_BUTTON) {
                // Approximate centering (would need text measurement first)
                cursor_x += 10.0f; 
                cursor_y += (view->rect.h - 15.0f) * 0.5f; 
            }

            const char* ptr = view->cached_text;
            while (*ptr) {
                uint32_t codepoint = (uint32_t)*ptr; // Simple ASCII for now
                RenderGlyph g;
                if (backend->get_glyph(backend, codepoint, &g)) {
                    SceneObject char_obj = {0};
                    char_obj.layer = LAYER_UI_CONTENT;
                    
                    // Position:
                    // Quad origin is Bottom-Left? Or Top-Left?
                    // Standard: Unit Quad is 0,0 to 1,1.
                    // Text needs to be drawn at (x + xoff, y + yoff).
                    // Vulkan Y is down? If Ortho 0..H is bottom-up, then Y increases up.
                    // If Ortho 0..H is top-down (standard UI), Y increases down.
                    // Check vk_backend: mat4_orthographic(0, w, 0, h, ...) -> 0 is Bottom? 
                    // Let's assume standard math ortho: 0,0 is Bottom-Left.
                    // But UI usually assumes 0,0 is Top-Left.
                    // If my UI coordinates are Top-Left based, but Projection is Bottom-Left...
                    // I might need to flip Y.
                    // Let's assume consistent coordinate system for now (Y Up).
                    
                    float x_pos = cursor_x + g.xoff;
                    float y_pos = cursor_y + g.yoff; 
                    
                    char_obj.position = (Vec3){x_pos, y_pos, 0.1f}; // Z slightly above background
                    char_obj.scale = (Vec3){g.w, g.h, 1.0f};
                    char_obj.rotation = (Vec3){0, 0, 0};
                    char_obj.mesh = &assets->unit_quad;
                    char_obj.color = (Vec4){1.0f, 1.0f, 1.0f, 1.0f}; // White text
                    
                    // Texture Params
                    char_obj.uv_rect.x = g.u0;
                    char_obj.uv_rect.y = g.v0;
                    char_obj.uv_rect.z = g.u1 - g.u0;
                    char_obj.uv_rect.w = g.v1 - g.v0;
                    
                    char_obj.params.x = 1.0f; // Enable Texture
                    
                    scene_add_object(scene, char_obj);
                    
                    cursor_x += g.advance;
                }
                ptr++;
            }
        }
    }

    for (size_t i = 0; i < view->child_count; ++i) {
        traverse_ui(view->children[i], scene, assets);
    }
}

void ui_build_scene(const UiView* root, Scene* scene, const Assets* assets) {
    if (!root || !scene || !assets) return;
    traverse_ui(root, scene, assets);
}
