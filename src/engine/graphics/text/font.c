#include "font.h"
#include "stb_truetype.h"
#include "foundation/platform/platform.h"
#include "foundation/logger/logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

static struct {
    FontAtlas atlas;
    stbtt_fontinfo fontinfo;
    unsigned char* ttf_buffer;
    bool initialized;
} g_font_state;

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
    g_font_state.atlas.width = 1024;
    g_font_state.atlas.height = 1024;
    g_font_state.atlas.pixels = malloc(g_font_state.atlas.width * g_font_state.atlas.height);
    memset(g_font_state.atlas.pixels, 0, g_font_state.atlas.width * g_font_state.atlas.height);
    memset(g_font_state.atlas.glyph_valid, 0, sizeof(g_font_state.atlas.glyph_valid));

    g_font_state.atlas.font_scale = stbtt_ScaleForPixelHeight(&g_font_state.fontinfo, 32.0f);
    
    int raw_ascent = 0, raw_descent = 0;
    stbtt_GetFontVMetrics(&g_font_state.fontinfo, &raw_ascent, &raw_descent, NULL);
    g_font_state.atlas.ascent = (int)roundf(raw_ascent * g_font_state.atlas.font_scale);
    g_font_state.atlas.descent = (int)roundf(raw_descent * g_font_state.atlas.font_scale);

    int ranges[][2] = { {32, 126}, {0x0400, 0x04FF} }; // ASCII + Cyrillic
    int range_count = (int)(sizeof(ranges) / sizeof(ranges[0]));

    int x = 0, y = 0, rowh = 0;
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

void font_cleanup(void) {
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

const FontAtlas* font_get_atlas(void) {
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