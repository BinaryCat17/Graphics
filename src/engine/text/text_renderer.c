#include "text_renderer.h"
#include "engine/text/font.h"
#include "internal/font_internal.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdint.h>

void scene_add_text_clipped(Scene* scene, const Font* font, const char* text, Vec3 pos, float scale, Vec4 color, Vec4 clip_rect) {
    if (!scene || !text || !font) return;

    float cursor_x = pos.x;
    float cursor_y = pos.y;
    
    const char* ptr = text;
    while (*ptr) {
        uint32_t c = (uint32_t)(*ptr);
        Glyph g;
        // Check if font is initialized and has glyph
        if (font_get_glyph(font, c, &g)) {
            // Calculate dimensions
            float scaled_w = g.w * scale;
            float scaled_h = g.h * scale;
            float scaled_xoff = g.xoff * scale;
            float scaled_yoff = g.yoff * scale;
            float scaled_advance = g.advance * scale;

            // Create Scene Object
            SceneObject obj;
            memset(&obj, 0, sizeof(SceneObject));
            
            // Position
            // Assuming Top-Left origin for Quad and Screen
            obj.position.x = cursor_x + scaled_xoff;
            obj.position.y = cursor_y + scaled_yoff;
            obj.position.z = pos.z;
            
            // Scale
            obj.scale.x = scaled_w;
            obj.scale.y = scaled_h;
            obj.scale.z = 1.0f;
            
            // Color
            obj.color = color;
            
            // Texture Params (1.0 = use texture)
            obj.raw.params_0.x = (float)SCENE_MODE_TEXTURED;
            
            // UVs
            obj.uv_rect.x = g.u0;
            obj.uv_rect.y = g.v0;
            obj.uv_rect.z = g.u1 - g.u0; // Width
            obj.uv_rect.w = g.v1 - g.v0; // Height
            
            // Clipping
            obj.ui.clip_rect = clip_rect;
            
            scene_add_object(scene, obj);
            
            cursor_x += scaled_advance;
        }
        ptr++;
    }
}

void scene_add_text(Scene* scene, const Font* font, const char* text, Vec3 pos, float scale, Vec4 color) {
    // Default to infinite clip
    Vec4 infinite_clip = {-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    scene_add_text_clipped(scene, font, text, pos, scale, color, infinite_clip);
    
    static bool logged = false;
    if (!logged) {
        LOG_INFO("Added text '%s' at (%.1f, %.1f)", text, pos.x, pos.y);
        logged = true;
    }
}
