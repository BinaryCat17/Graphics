#include "ui_scene_bridge.h"
#include "ui_def.h"
#include "ui_layout.h"
#include "foundation/logger/logger.h"
#include "foundation/math/coordinate_systems.h"
#include "foundation/platform/platform.h"
#include "engine/render/backend/common/renderer_backend.h" // For RendererBackend (draw commands might still use it?)
#include "engine/text/font.h"
#include "engine/scene/scene.h"
#include "engine/assets/assets.h"
#include <string.h> // For memcpy
#include <stdio.h> // For snprintf

static void traverse_ui(const UiView* view, Scene* scene, const Assets* assets, bool debug_frame) {
    if (!view) return;

    // Create Scene Object for this view (Background)
    if (view->rect.w > 0 && view->rect.h > 0) {
        SceneObject obj = {0};
        obj.layer = LAYER_UI_BACKGROUND; // Background layer
        obj.position = (Vec3){view->rect.x, view->rect.y, 0.1f}; // Z=0.1
        obj.scale = (Vec3){view->rect.w, view->rect.h, 1.0f};
        obj.rotation = (Vec3){0, 0, 0};
        obj.mesh = &assets->unit_quad;
        obj.params.x = 0.0f; // No texture
        
        // Color based on type
        if (view->def->type == UI_NODE_CURVE) {
            obj.prim_type = SCENE_PRIM_CURVE;
            obj.color = (Vec4){0.8f, 0.8f, 0.8f, 1.0f}; // Grey wire
            obj.params.z = 0.02f; // Default thickness
            
            // Resolve Curve Bindings
            if (view->data_ptr && view->meta) {
                 float u1=0, v1=0, u2=1, v2=1;
                 
                 // Inline helper logic
                 #define GET_VAL(src, var) \
                    if (view->def->src) { \
                        for(size_t k=0; k<view->meta->field_count; ++k) { \
                            if (strcmp(view->meta->fields[k].name, view->def->src)==0) { \
                                if (view->meta->fields[k].type == META_TYPE_FLOAT) var = meta_get_float(view->data_ptr, &view->meta->fields[k]); \
                                break; \
                            } \
                        } \
                    }
                 
                 GET_VAL(u1_source, u1);
                 GET_VAL(v1_source, v1);
                 GET_VAL(u2_source, u2);
                 GET_VAL(v2_source, v2);
                 
                 obj.uv_rect.x = u1; obj.uv_rect.y = v1;
                 obj.uv_rect.z = u2; obj.uv_rect.w = v2;
            }
            
        } else if (view->def->type == UI_NODE_PANEL) {
            obj.color = (Vec4){0.2f, 0.2f, 0.2f, 1.0f};
        } else if (view->def->type == UI_NODE_BUTTON) {
            if (view->is_pressed) {
                obj.color = (Vec4){0.2f, 0.4f, 0.7f, 1.0f};
            } else if (view->is_hovered) {
                obj.color = (Vec4){0.4f, 0.6f, 0.9f, 1.0f};
            } else {
                obj.color = (Vec4){0.3f, 0.5f, 0.8f, 1.0f};
            }
        } else if (view->def->type == UI_NODE_SLIDER) {
            obj.color = (Vec4){0.4f, 0.4f, 0.4f, 1.0f};
        } else if (view->def->type == UI_NODE_LABEL) {
             obj.color = (Vec4){0, 0, 0, 0}; // Transparent
        } else {
            obj.color = (Vec4){1.0f, 0.0f, 1.0f, 1.0f}; // Debug magenta
        }
        
        // Add to scene
        scene_add_object(scene, obj);

        if (debug_frame) {
            LOG_TRACE("[Frame %llu] GenPanel id='%s' Type=%d Rect(%.1f, %.1f, %.1f, %.1f) Color(%.2f, %.2f, %.2f, %.2f)",
                (unsigned long long)scene->frame_number,
                view->def->id ? view->def->id : "(anon)",
                view->def->type,
                view->rect.x, view->rect.y, view->rect.w, view->rect.h,
                (double)obj.color.x, (double)obj.color.y, (double)obj.color.z, (double)obj.color.w);
        }
    } else {
        if (debug_frame) {
             LOG_WARN("Skipping Panel id='%s' due to invalid rect(%.1f, %.1f, %.1f, %.1f)",
                view->def->id ? view->def->id : "(anon)",
                view->rect.x, view->rect.y, view->rect.w, view->rect.h);
        }
    }

    // Text Rendering
    if (view->cached_text && strlen(view->cached_text) > 0) {
        float cursor_x = view->rect.x + 5.0f; // Padding
        // Shift down by approximate ascent (24px for 32px font) to act as baseline
        float cursor_y = view->rect.y + 24.0f; 
        
        // Center text for buttons
        if (view->def->type == UI_NODE_BUTTON) {
            // Approximate centering (would need text measurement first)
            cursor_x += 10.0f; 
            cursor_y += (view->rect.h - 15.0f) * 0.5f; 
        }
        
        if (debug_frame) {
            LOG_TRACE("[Frame %llu] GenText '%s': Start(%.1f, %.1f) Rect(%.1f, %.1f, %.1f, %.1f)", 
                (unsigned long long)scene->frame_number,
                view->cached_text, cursor_x, cursor_y,
                view->rect.x, view->rect.y, view->rect.w, view->rect.h);
        }

        const char* ptr = view->cached_text;
        int char_count = 0;
        while (*ptr) {
            uint32_t codepoint = (uint32_t)*ptr; // Simple ASCII for now
            Glyph g;
            if (font_get_glyph(codepoint, &g)) {
                SceneObject char_obj = {0};
                char_obj.layer = LAYER_UI_CONTENT;
                
                float x_pos = cursor_x + g.xoff;
                float y_pos = cursor_y + g.yoff; 
                
                char_obj.position = (Vec3){x_pos, y_pos, 0.0f}; // Z=0.0 (Closest, visible in -1.0 to 1.0 range)
                char_obj.scale = (Vec3){g.w, g.h, 1.0f};
                char_obj.rotation = (Vec3){0, 0, 0};
                char_obj.mesh = &assets->unit_quad;
                char_obj.color = (Vec4){1.0f, 1.0f, 1.0f, 1.0f}; // White text
                
                // Texture Params
                char_obj.uv_rect.x = g.u0;
                char_obj.uv_rect.y = g.v0;
                char_obj.uv_rect.z = g.u1 - g.u0; // Width in UV space
                char_obj.uv_rect.w = g.v1 - g.v0; // Height in UV space (positive if v1 > v0)
                
                char_obj.params.x = 1.0f; // Enable Texture
                
                scene_add_object(scene, char_obj);
                
                if (debug_frame && char_count == 0) {
                    LOG_TRACE("  [Frame %llu] First Char '%c': Pos(%.1f, %.1f) Sz(%.1f, %.1f) UV(%.3f,%.3f)",
                        (unsigned long long)scene->frame_number,
                        *ptr, x_pos, y_pos, g.w, g.h, g.u0, g.v0);
                }
                
                cursor_x += g.advance;
                char_count++;
            }
            ptr++;
        }
    }

    for (size_t i = 0; i < view->child_count; ++i) {
        traverse_ui(view->children[i], scene, assets, debug_frame);
    }
}


void ui_build_scene(const UiView* root, Scene* scene, const Assets* assets) {
    if (!root || !scene || !assets) return;
    
    static double last_log_time = -1.0; // Use -1.0 to ensure initial log
    bool debug_frame = false;

    double current_time = platform_get_time_ms();
    if (last_log_time < 0 || (current_time - last_log_time) / 1000.0 >= logger_get_trace_interval()) {
        debug_frame = true;
        last_log_time = current_time;
    }

    // Layout Pass
    ui_layout_root((UiView*)root, 1280.0f, 720.0f, scene->frame_number, debug_frame); 
    
    if (debug_frame) {
        LOG_TRACE("UI Build Scene [Frame %llu]: %zu objects generated. Root Rect: %.1f, %.1f, %.1f, %.1f", 
            (unsigned long long)scene->frame_number, scene->object_count, root->rect.x, root->rect.y, root->rect.w, root->rect.h);
    }
    
    traverse_ui(root, scene, assets, debug_frame);
}
