#ifndef FONT_INTERNAL_H
#define FONT_INTERNAL_H

#include "../font.h"
#include "foundation/memory/arena.h"
#include "stb_truetype.h"

#define GLYPH_CAPACITY 2048

typedef struct {
    float u0, v0, u1, v1; // Texture coordinates in atlas
    float xoff, yoff;     // Offset from cursor to top-left of glyph
    float w, h;           // Glyph size in pixels
    float advance;        // Horizontal advance
} Glyph;

struct Font {
    MemoryArena arena;
    int width;
    int height;
    unsigned char* pixels; // R8 format (alpha/intensity only)
    
    Glyph glyphs[GLYPH_CAPACITY];
    bool glyph_valid[GLYPH_CAPACITY];
    
    stbtt_fontinfo fontinfo;
    unsigned char* ttf_buffer;
    float font_scale;
    int ascent;
    int descent;
};

// Internal function to get a glyph. 
// This is used by text_renderer.c but shouldn't be exposed to the App or other systems.
bool font_get_glyph(const Font* font, uint32_t codepoint, Glyph* out_glyph);

#endif // FONT_INTERNAL_H
