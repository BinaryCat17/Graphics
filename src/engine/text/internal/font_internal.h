#ifndef FONT_INTERNAL_H
#define FONT_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#define GLYPH_CAPACITY 2048

typedef struct {
    float u0, v0, u1, v1; // Texture coordinates in atlas
    float xoff, yoff;     // Offset from cursor to top-left of glyph
    float w, h;           // Glyph size in pixels
    float advance;        // Horizontal advance
} Glyph;

typedef struct {
    int width;
    int height;
    unsigned char* pixels; // R8 format (alpha/intensity only)
    Glyph glyphs[GLYPH_CAPACITY];
    bool glyph_valid[GLYPH_CAPACITY];
    float font_scale;
    int ascent;
    int descent;
} FontAtlas;

// Internal function to get a glyph. 
// This is used by text_renderer.c but shouldn't be exposed to the App or other systems.
bool font_get_glyph(uint32_t codepoint, Glyph* out_glyph);

// Internal accessor for the global atlas state if needed
const FontAtlas* font_get_atlas_internal(void);

#endif // FONT_INTERNAL_H
