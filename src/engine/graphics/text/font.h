#ifndef FONT_H
#define FONT_H

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

// Initialize the font module, loading a font from disk and building the atlas.
// Returns true on success.
bool font_init(const char* font_path);

// Clean up resources (pixels, etc.)
void font_cleanup(void);

// Get the current atlas data (e.g. for uploading to GPU).
const FontAtlas* font_get_atlas(void);

// Get a glyph for a codepoint. Returns false if invalid/missing.
bool font_get_glyph(uint32_t codepoint, Glyph* out_glyph);

// Measure the width of a text string.
float font_measure_text(const char* text);

#endif // FONT_H