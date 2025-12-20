#ifndef FONT_H
#define FONT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Initialize the font module using TTF data from memory.
// Returns true on success.
bool font_init(const void* ttf_data, size_t ttf_size);

// Clean up resources (pixels, etc.)
void font_shutdown(void);

// Get the current atlas texture data (R8 format).
// Used by the renderer backend to upload the texture.
void font_get_atlas_data(int* width, int* height, unsigned char** pixels);

// Measure the width of a text string.
float font_measure_text(const char* text);

// Special UVs
void font_get_white_pixel_uv(float* u, float* v);
void font_get_ui_rect_uv(float* u0, float* v0, float* u1, float* v1);

#endif // FONT_H
