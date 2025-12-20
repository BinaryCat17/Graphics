#include "font.h"
#include "internal/font_internal.h"
#include "stb_truetype.h"
#include "foundation/platform/platform.h"
#include "foundation/logger/logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024
#define FONT_SIZE_PIXELS 32.0f
#define UI_RECT_SIZE 32
#define UI_RECT_X 8
#define UI_RECT_Y 0

static struct {
    FontAtlas atlas;
    stbtt_fontinfo fontinfo;
    unsigned char* ttf_buffer;
    bool initialized;
} g_font_state;

static float smoothstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

bool font_init(const char* font_path) {
    if (g_font_state.initialized) return true;
    if (!font_path) {
        LOG_ERROR("Font path is null");
        return false;
    }

    FILE* f = platform_fopen(font_path, "rb");
    if (!f) {
        LOG_ERROR("Font not found at %s", font_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    g_font_state.ttf_buffer = malloc(sz);
    if (!g_font_state.ttf_buffer) {
        fclose(f);
        return false;
    }
    
    fread(g_font_state.ttf_buffer, 1, sz, f);
    fclose(f);

    if (!stbtt_InitFont(&g_font_state.fontinfo, g_font_state.ttf_buffer, 0)) {
        LOG_ERROR("Failed to init stb_truetype");
        free(g_font_state.ttf_buffer);
        g_font_state.ttf_buffer = NULL;
        return false;
    }

    // Build Atlas
    g_font_state.atlas.width = ATLAS_WIDTH;
    g_font_state.atlas.height = ATLAS_HEIGHT;
    g_font_state.atlas.pixels = malloc(g_font_state.atlas.width * g_font_state.atlas.height);
    memset(g_font_state.atlas.pixels, 0, g_font_state.atlas.width * g_font_state.atlas.height);
    memset(g_font_state.atlas.glyph_valid, 0, sizeof(g_font_state.atlas.glyph_valid));

    g_font_state.atlas.font_scale = stbtt_ScaleForPixelHeight(&g_font_state.fontinfo, FONT_SIZE_PIXELS);
    
    // Reserve space for white pixel at (0,0) and UI rect at (2,0)
    // We will start glyphs at y=32 to be safe
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            g_font_state.atlas.pixels[i * g_font_state.atlas.width + j] = 255;
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
            
            unsigned char val = (unsigned char)(alpha * 255);
            // We want it to be a bit thicker at borders for visibility
            if (d > inner) val = (unsigned char)(alpha * 255); 
            else val = (unsigned char)(alpha * 180); // Lighter center
            
            g_font_state.atlas.pixels[(ui_y + j) * g_font_state.atlas.width + (ui_x + i)] = val;
        }
    }

    int raw_ascent = 0, raw_descent = 0;
    stbtt_GetFontVMetrics(&g_font_state.fontinfo, &raw_ascent, &raw_descent, NULL);
    g_font_state.atlas.ascent = (int)roundf(raw_ascent * g_font_state.atlas.font_scale);
    g_font_state.atlas.descent = (int)roundf(raw_descent * g_font_state.atlas.font_scale);

    static const int ranges[][2] = { {32, 126}, {0x0400, 0x04FF} }; // ASCII + Cyrillic
    int range_count = (int)(sizeof(ranges) / sizeof(ranges[0]));

    int x = 0, y = 40, rowh = 0; // Start below UI placeholders
    int glyph_count = 0;
    
    for (int r = 0; r < range_count; r++) {
        for (int c = ranges[r][0]; c <= ranges[r][1] && c < GLYPH_CAPACITY; c++) {
            int aw, ah, bx, by;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&g_font_state.fontinfo, 0, g_font_state.atlas.font_scale, c, &aw, &ah, &bx, &by);
            
            if (x + aw >= g_font_state.atlas.width) { 
                x = 0; 
                y += rowh; 
                rowh = 0; 
            }
            
            if (y + ah >= g_font_state.atlas.height) { 
                LOG_ERROR("Font atlas too small!"); 
                stbtt_FreeBitmap(bitmap, NULL); 
                break; 
            }
            
            for (int yy = 0; yy < ah; yy++) {
                for (int xx = 0; xx < aw; xx++) {
                    g_font_state.atlas.pixels[(y + yy) * g_font_state.atlas.width + (x + xx)] = bitmap[yy * aw + xx];
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
            
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&g_font_state.fontinfo, c, &advance, &lsb);
            
            int box_x0, box_y0, box_x1, box_y1;
            stbtt_GetCodepointBitmapBox(&g_font_state.fontinfo, c, g_font_state.atlas.font_scale, g_font_state.atlas.font_scale, &box_x0, &box_y0, &box_x1, &box_y1);
            
            g_font_state.atlas.glyphs[c].advance = advance * g_font_state.atlas.font_scale;
            g_font_state.atlas.glyphs[c].xoff = (float)box_x0;
            g_font_state.atlas.glyphs[c].yoff = (float)box_y0;
            g_font_state.atlas.glyphs[c].w = (float)(box_x1 - box_x0);
            g_font_state.atlas.glyphs[c].h = (float)(box_y1 - box_y0);
            
            g_font_state.atlas.glyphs[c].u0 = (float)x / (float)g_font_state.atlas.width;
            g_font_state.atlas.glyphs[c].v0 = (float)y / (float)g_font_state.atlas.height;
            g_font_state.atlas.glyphs[c].u1 = (float)(x + aw) / (float)g_font_state.atlas.width;
            g_font_state.atlas.glyphs[c].v1 = (float)(y + ah) / (float)g_font_state.atlas.height;
            
            g_font_state.atlas.glyph_valid[c] = true;
            glyph_count++;
            
            x += aw + 1;
            if (ah > rowh) rowh = ah;
        }
    }
    
    LOG_INFO("Font Module: Atlas Built %dx%d, Glyphs: %d, Scale: %.4f", 
        g_font_state.atlas.width, g_font_state.atlas.height, glyph_count, g_font_state.atlas.font_scale);

    g_font_state.initialized = true;
    return true;
}

void font_shutdown(void) {
    if (g_font_state.atlas.pixels) {
        free(g_font_state.atlas.pixels);
        g_font_state.atlas.pixels = NULL;
    }
    if (g_font_state.ttf_buffer) {
        free(g_font_state.ttf_buffer);
        g_font_state.ttf_buffer = NULL;
    }
    g_font_state.initialized = false;
}

void font_get_atlas_data(int* width, int* height, unsigned char** pixels) {
    if (!g_font_state.initialized) {
        if (width) *width = 0;
        if (height) *height = 0;
        if (pixels) *pixels = NULL;
        return;
    }
    if (width) *width = g_font_state.atlas.width;
    if (height) *height = g_font_state.atlas.height;
    if (pixels) *pixels = g_font_state.atlas.pixels;
}

const FontAtlas* font_get_atlas_internal(void) {
    if (!g_font_state.initialized) return NULL;
    return &g_font_state.atlas;
}

bool font_get_glyph(uint32_t codepoint, Glyph* out_glyph) {
    if (!g_font_state.initialized || codepoint >= GLYPH_CAPACITY || !g_font_state.atlas.glyph_valid[codepoint]) {
        return false;
    }
    if (out_glyph) {
        *out_glyph = g_font_state.atlas.glyphs[codepoint];
    }
    return true;
}

float font_measure_text(const char* text) {
    if (!g_font_state.initialized || !text) return 0.0f;
    
    float width = 0.0f;
    const char* ptr = text;
    while (*ptr) {
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&g_font_state.fontinfo, *ptr, &advance, &lsb);
        width += advance * g_font_state.atlas.font_scale;
        ptr++;
    }
    return width;
}

void font_get_white_pixel_uv(float* u, float* v) {
    if (u) *u = 1.0f / (float)g_font_state.atlas.width;
    if (v) *v = 1.0f / (float)g_font_state.atlas.height;
}

void font_get_ui_rect_uv(float* u0, float* v0, float* u1, float* v1) {
    if (u0) *u0 = (float)UI_RECT_X / (float)g_font_state.atlas.width;
    if (v0) *v0 = (float)UI_RECT_Y / (float)g_font_state.atlas.height;
    if (u1) *u1 = (float)(UI_RECT_X + UI_RECT_SIZE) / (float)g_font_state.atlas.width;
    if (v1) *v1 = (float)(UI_RECT_Y + UI_RECT_SIZE) / (float)g_font_state.atlas.height;
}
