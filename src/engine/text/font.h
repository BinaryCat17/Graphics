#ifndef FONT_H
#define FONT_H

#include "text_def.h"

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