#include "text_renderer.h"
#include "engine/scene/render_packet.h"
#include "engine/text/font.h"
#include "internal/font_internal.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdint.h>

#include "engine/ui/ui_node.h"

void scene_add_text_clipped(Scene* scene, const Font* font, const char* text, Vec3 pos, float scale, Vec4 color, Vec4 clip_rect) {
    if (!scene || !text || !font) return;

    float cursor_x = pos.x;
    // Shift cursor down to baseline because 'pos' is top-left, 
    // but glyph offsets are relative to baseline.
    float cursor_y = pos.y + (font->ascent * scale);
    
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

            // Create UiNode
            UiNode node = {0};
            
            // Position
            node.rect = (Rect){cursor_x + scaled_xoff, cursor_y + scaled_yoff, scaled_w, scaled_h};
            node.z_index = pos.z;
            
            // Color
            node.color = color;
            
            // Texture Params
            node.primitive_type = SCENE_MODE_TEXTURED;
            node.flags = UI_RENDER_FLAG_TEXTURED | UI_RENDER_FLAG_HAS_BG;
            
            // UVs
            node.uv_rect = (Vec4){g.u0, g.v0, g.u1 - g.u0, g.v1 - g.v0};
            
            // Clipping
            node.clip_rect = (Rect){clip_rect.x, clip_rect.y, clip_rect.z, clip_rect.w};
            
            scene_push_ui_node(scene, node);
            
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
