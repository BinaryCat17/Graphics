#include "font.h"
#include "internal/font_internal.h"
#include "stb_truetype.h"
#include "foundation/platform/platform.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024
#define FONT_SIZE_PIXELS 32.0f
#define UI_RECT_SIZE 32
#define UI_RECT_X 8
#define UI_RECT_Y 0
#define FONT_ARENA_SIZE (4 * 1024 * 1024) // 4MB

static float smoothstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

Font* font_create(const void* ttf_data, size_t ttf_size) {
    if (!ttf_data || ttf_size == 0) {
        LOG_ERROR("Font data is null or empty");
        return NULL;
    }

    Font* font = (Font*)calloc(1, sizeof(Font));
    if (!font) {
        LOG_FATAL("Failed to allocate Font struct");
        return NULL;
    }

    if (!arena_init(&font->arena, FONT_ARENA_SIZE)) {
        LOG_FATAL("Failed to initialize Font Arena");
        free(font);
        return NULL;
    }

    // Copy TTF data to our arena to ensure persistence
    font->ttf_buffer = arena_alloc(&font->arena, ttf_size);
    if (!font->ttf_buffer) {
        LOG_FATAL("Failed to allocate font buffer in arena");
        arena_destroy(&font->arena);
        free(font);
        return NULL;
    }
    memcpy(font->ttf_buffer, ttf_data, ttf_size);

    if (!stbtt_InitFont(&font->fontinfo, font->ttf_buffer, 0)) {
        LOG_ERROR("Failed to init stb_truetype");
        arena_destroy(&font->arena);
        free(font);
        return NULL;
    }

    // Build Atlas
    font->width = ATLAS_WIDTH;
    font->height = ATLAS_HEIGHT;
    // Use arena_alloc_zero to clear memory automatically
    font->pixels = arena_alloc_zero(&font->arena, font->width * font->height);
    memset(font->glyph_valid, 0, sizeof(font->glyph_valid));

    font->font_scale = stbtt_ScaleForPixelHeight(&font->fontinfo, FONT_SIZE_PIXELS);
    
    // Reserve space for white pixel at (0,0) and UI rect at (2,0)
    // We will start glyphs at y=32 to be safe
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            font->pixels[i * font->width + j] = 255;
        }
    }
    
    // Generate a simple 9-slice rounded rect
    int ui_x = UI_RECT_X, ui_y = UI_RECT_Y, ui_sz = UI_RECT_SIZE;
    for (int j = 0; j < ui_sz; ++j) {
        for (int i = 0; i < ui_sz; ++i) {
            float dx = (float)i - (ui_sz-1)*0.5f;
            float dy = (float)j - (ui_sz-1)*0.5f;
            float d = sqrtf(dx*dx + dy*dy);
            float radius = (ui_sz-1)*0.5f - 2.0f;
            
            // Outer shape
            float alpha = 1.0f - smoothstep(radius, radius + 1.0f, d);
            
            // Border (2px)
            float inner = radius - 2.0f;
            
            unsigned char val;
            // We want it to be a bit thicker at borders for visibility
            if (d > inner) val = (unsigned char)(alpha * 255); 
            else val = (unsigned char)(alpha * 180); // Lighter center
            
            font->pixels[(ui_y + j) * font->width + (ui_x + i)] = val;
        }
    }

    int raw_ascent = 0, raw_descent = 0;
    stbtt_GetFontVMetrics(&font->fontinfo, &raw_ascent, &raw_descent, NULL);
    font->ascent = (int)roundf(raw_ascent * font->font_scale);
    font->descent = (int)roundf(raw_descent * font->font_scale);

    static const int ranges[][2] = { {32, 126}, {0x0400, 0x04FF} }; // ASCII + Cyrillic
    int range_count = (int)(sizeof(ranges) / sizeof(ranges[0]));

    int x = 0, y = 40, rowh = 0; // Start below UI placeholders
    int glyph_count = 0;
    
    for (int r = 0; r < range_count; r++) {
        for (int c = ranges[r][0]; c <= ranges[r][1] && c < GLYPH_CAPACITY; c++) {
            int aw, ah, bx, by;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&font->fontinfo, 0, font->font_scale, c, &aw, &ah, &bx, &by);
            
            if (x + aw >= font->width) { 
                x = 0; 
                y += rowh; 
                rowh = 0; 
            }
            
            if (y + ah >= font->height) { 
                LOG_ERROR("Font atlas too small!"); 
                stbtt_FreeBitmap(bitmap, NULL); 
                break; 
            }
            
            for (int yy = 0; yy < ah; yy++) {
                for (int xx = 0; xx < aw; xx++) {
                    font->pixels[(y + yy) * font->width + (x + xx)] = bitmap[yy * aw + xx];
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
            
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font->fontinfo, c, &advance, &lsb);
            
            int box_x0, box_y0, box_x1, box_y1;
            stbtt_GetCodepointBitmapBox(&font->fontinfo, c, font->font_scale, font->font_scale, &box_x0, &box_y0, &box_x1, &box_y1);
            
            font->glyphs[c].advance = advance * font->font_scale;
            font->glyphs[c].xoff = (float)box_x0;
            font->glyphs[c].yoff = (float)box_y0;
            font->glyphs[c].w = (float)(box_x1 - box_x0);
            font->glyphs[c].h = (float)(box_y1 - box_y0);
            
            font->glyphs[c].u0 = (float)x / (float)font->width;
            font->glyphs[c].v0 = (float)y / (float)font->height;
            font->glyphs[c].u1 = (float)(x + aw) / (float)font->width;
            font->glyphs[c].v1 = (float)(y + ah) / (float)font->height;
            
            font->glyph_valid[c] = true;
            glyph_count++;
            
            x += aw + 1;
            if (ah > rowh) rowh = ah;
        }
    }
    
    LOG_INFO("Font Module: Atlas Built %dx%d, Glyphs: %d, Scale: %.4f", 
        font->width, font->height, glyph_count, font->font_scale);

    return font;
}

void font_destroy(Font* font) {
    if (font) {
        arena_destroy(&font->arena);
        free(font);
    }
}

void font_get_atlas_data(const Font* font, int* width, int* height, unsigned char** pixels) {
    if (!font) {
        if (width) *width = 0;
        if (height) *height = 0;
        if (pixels) *pixels = NULL;
        return;
    }
    if (width) *width = font->width;
    if (height) *height = font->height;
    if (pixels) *pixels = font->pixels;
}

bool font_get_glyph(const Font* font, uint32_t codepoint, Glyph* out_glyph) {
    if (!font || codepoint >= GLYPH_CAPACITY || !font->glyph_valid[codepoint]) {
        return false;
    }
    if (out_glyph) {
        *out_glyph = font->glyphs[codepoint];
    }
    return true;
}

float font_measure_text(const Font* font, const char* text) {
    if (!font || !text) return 0.0f;
    
    float width = 0.0f;
    const char* ptr = text;
    while (*ptr) {
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&((Font*)font)->fontinfo, *ptr, &advance, &lsb);
        width += advance * font->font_scale;
        ptr++;
    }
    return width;
}

void font_get_white_pixel_uv(const Font* font, float* u, float* v) {
    if (font) {
        if (u) *u = 1.0f / (float)font->width;
        if (v) *v = 1.0f / (float)font->height;
    }
}

void font_get_ui_rect_uv(const Font* font, float* u0, float* v0, float* u1, float* v1) {
    if (font) {
        if (u0) *u0 = (float)UI_RECT_X / (float)font->width;
        if (v0) *v0 = (float)UI_RECT_Y / (float)font->height;
        if (u1) *u1 = (float)(UI_RECT_X + UI_RECT_SIZE) / (float)font->width;
        if (v1) *v1 = (float)(UI_RECT_Y + UI_RECT_SIZE) / (float)font->height;
    }
}
