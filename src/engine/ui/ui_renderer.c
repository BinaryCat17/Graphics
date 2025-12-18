#include "ui_renderer.h"
#include "ui_core.h"
#include "engine/graphics/scene/scene.h"
#include "foundation/logger/logger.h"
#include "engine/graphics/text/text_renderer.h" 
#include "engine/graphics/text/font.h" 
#include <string.h> 

// Helper: Intersection
static Rect rect_intersect(Rect a, Rect b) {
    float x1 = (a.x > b.x) ? a.x : b.x;
    float y1 = (a.y > b.y) ? a.y : b.y;
    float x2 = (a.x + a.w < b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    float y2 = (a.y + a.h < b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    
    if (x2 < x1) x2 = x1;
    if (y2 < y1) y2 = y1;
    
    return (Rect){x1, y1, x2 - x1, y2 - y1};
}

static void process_node(const UiElement* el, Scene* scene, const Assets* assets, Rect current_clip) {
    if (!el || !el->spec) return;

    // Skip hidden
    if (el->spec->flags & UI_FLAG_HIDDEN) return;

    // Determine Logic Clip (Effective Clip)
    // If this node clips, it constrains the passed down clip.
    Rect effective_clip = current_clip;
    if (el->spec->flags & UI_FLAG_CLIPPED) {
        effective_clip = rect_intersect(current_clip, el->screen_rect);
    }
    
    // Prepare clip vec for shader (x,y,w,h)
    // If w/h are massive, shader treats as no-op or we use flag.
    // Let's rely on coordinate logic.
    Vec4 clip_vec = {effective_clip.x, effective_clip.y, effective_clip.w, effective_clip.h};

    // 1. Draw Background (Container or Text Input)
    if (el->spec->kind == UI_KIND_CONTAINER || el->spec->kind == UI_KIND_TEXT_INPUT) {
        SceneObject quad = {0};
        quad.prim_type = SCENE_PRIM_QUAD; 
        quad.position = (Vec3){el->screen_rect.x, el->screen_rect.y, 0.0f}; 
        quad.scale = (Vec3){el->screen_rect.w, el->screen_rect.h, 1.0f};
        quad.clip_rect = clip_vec; 
        
        // Default color from spec
        quad.color = el->spec->color;
        if (quad.color.w == 0) quad.color = (Vec4){0.1f, 0.1f, 0.1f, 0.8f}; // Fallback

        // Hover/Active tints
        if (el->is_active) {
            quad.color.x *= 0.5f; quad.color.y *= 0.5f; quad.color.z *= 0.5f;
        } else if (el->is_hovered) {
            quad.color.x *= 1.2f; quad.color.y *= 1.2f; quad.color.z *= 1.2f;
        } else if (el->spec->kind == UI_KIND_TEXT_INPUT) {
             // Make inputs slightly lighter by default
             quad.color.x *= 1.1f; quad.color.y *= 1.1f; quad.color.z *= 1.1f;
        }

        // FORCE FLAT for debug
        if ((el->spec->border_l > 0.0f || el->spec->texture_path)) {
            // Use 9-Slice or Textured Quad
            quad.params.x = 3.0f; // 9-Slice
            
            float u0, v0, u1, v1;
            font_get_ui_rect_uv(&u0, &v0, &u1, &v1);
            quad.uv_rect = (Vec4){u0, v0, u1 - u0, v1 - v0};
            
            // Pass 9-slice data
            // inParams: z=tex_w, w=tex_h
            // inExtra: x=border_l, y=border_t, z=border_r, w=border_b
            quad.params.z = el->spec->tex_w > 0 ? el->spec->tex_w : 32.0f;
            quad.params.w = el->spec->tex_h > 0 ? el->spec->tex_h : 32.0f;
            quad.extra.x = el->spec->border_l;
            quad.extra.y = el->spec->border_t;
            quad.extra.z = el->spec->border_r;
            quad.extra.w = el->spec->border_b;
            
        } else {
            // Flat Color Quad
            quad.params.x = 0.0f;
            float u, v;
            font_get_white_pixel_uv(&u, &v);
            quad.uv_rect = (Vec4){u, v, 0.001f, 0.001f}; 
        }
        
        scene_add_object(scene, quad);
    }

    // 2. Draw Content (Text or Text Input)
    if (el->spec->static_text || el->cached_text || el->spec->kind == UI_KIND_TEXT_INPUT) {
        const char* text = el->cached_text ? el->cached_text : el->spec->static_text;
        
        // For inputs without text, show nothing or placeholder?
        if (el->spec->kind == UI_KIND_TEXT_INPUT && !text) text = "";

        if (text) {
            Vec3 pos = {el->screen_rect.x + el->spec->padding, el->screen_rect.y + el->spec->padding, 0.1f};
            
            // Pass the effective clip to the text renderer
            Vec4 clip_v = {effective_clip.x, effective_clip.y, effective_clip.w, effective_clip.h};
            scene_add_text_clipped(scene, text, pos, 0.5f, (Vec4){1.0f, 1.0f, 1.0f, 1.0f}, clip_v);

            // Draw Caret
            if (el->spec->kind == UI_KIND_TEXT_INPUT && el->is_focused) {
                // Measure substring to find caret X
                char temp[256];
                int len = (int)strlen(text);
                int c_idx = el->cursor_idx;
                if (c_idx > len) c_idx = len;
                if (c_idx < 0) c_idx = 0;
                
                // Copy substring
                int copy_len = c_idx < 255 ? c_idx : 255;
                memcpy(temp, text, copy_len);
                temp[copy_len] = '\0';
                
                float text_width = font_measure_text(temp) * 0.5f; // Scale 0.5f matches scene_add_text_clipped above
                
                SceneObject caret = {0};
                caret.prim_type = SCENE_PRIM_QUAD;
                caret.position = (Vec3){pos.x + text_width, pos.y, 0.15f}; // slightly in front of text
                caret.scale = (Vec3){2.0f, 20.0f, 1.0f}; // 2px wide, 20px high (approx)
                caret.clip_rect = clip_v;
                caret.color = (Vec4){1.0f, 1.0f, 1.0f, 1.0f}; // White cursor
                
                // Flat
                caret.params.x = 0.0f;
                float u, v;
                font_get_white_pixel_uv(&u, &v);
                caret.uv_rect = (Vec4){u, v, 0.001f, 0.001f};

                scene_add_object(scene, caret);
            }
        }
    }

    // 3. Recurse
    for (size_t i = 0; i < el->child_count; ++i) {
        process_node(el->children[i], scene, assets, effective_clip);
    }
}

void ui_renderer_build_scene(const UiElement* root, Scene* scene, const Assets* assets) {
    if (!root) return;
    
    // Default "Infinite" Clip
    Rect infinite_clip = {-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    process_node(root, scene, assets, infinite_clip);
}