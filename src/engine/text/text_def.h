#ifndef TEXT_DEF_H
#define TEXT_DEF_H

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

#endif // TEXT_DEF_H
