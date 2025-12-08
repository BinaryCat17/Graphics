// Minimal stub version of stb_truetype.h to satisfy builds when the full
// library is unavailable in the environment. This provides basic metrics and
// bitmap generation placeholders sufficient for the sample application to
// compile and run without external dependencies.
//
// This stub is NOT a full implementation of stb_truetype. For production use,
// replace it with the official header from https://github.com/nothings/stb.

#ifndef STB_TRUETYPE_H
#define STB_TRUETYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

typedef unsigned char stbtt_uint8;
typedef signed char stbtt_int8;
typedef unsigned short stbtt_uint16;
typedef signed short stbtt_int16;
typedef unsigned int stbtt_uint32;
typedef signed int stbtt_int32;

#ifndef STBTT_DEF
#define STBTT_DEF static
#endif

#ifndef STBTT_FABS
#include <math.h>
#define STBTT_FABS(x) ((float)fabs(x))
#endif

#ifndef STBTT_assert
#include <assert.h>
#define STBTT_assert(x) assert(x)
#endif

#ifndef STBTT_malloc
#define STBTT_malloc(x,u) ((void)(u),malloc(x))
#define STBTT_free(x,u) ((void)(u),free(x))
#endif

// Minimal font info storing only the pointer to the font buffer.
typedef struct
{
    const unsigned char *data;
} stbtt_fontinfo;

// Initialize font info from a buffer. Always succeeds if pointers are valid.
STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset)
{
    (void)offset;
    if (!info || !data) return 0;
    info->data = data;
    return 1;
}

// Use a simple linear scale based on requested pixel height.
STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels)
{
    (void)info;
    return pixels > 0.0f ? pixels / 16.0f : 0.0f;
}

// Provide basic vertical metrics: ascent approximates pixel height, descent is
// negative quarter height, and line gap is zero.
STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap)
{
    (void)info;
    if (ascent) *ascent = 16;
    if (descent) *descent = -4;
    if (lineGap) *lineGap = 0;
}

// Return a tiny 1x1 bitmap for any codepoint to allow callers to proceed even
// without real glyph data.
STBTT_DEF unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
    (void)info; (void)scale_x; (void)scale_y; (void)codepoint;
    if (width) *width = 1;
    if (height) *height = 1;
    if (xoff) *xoff = 0;
    if (yoff) *yoff = 0;
    unsigned char *bitmap = (unsigned char *)STBTT_malloc(1, NULL);
    if (bitmap) bitmap[0] = 0xFF;
    return bitmap;
}

STBTT_DEF void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata)
{
    (void)userdata;
    STBTT_free(bitmap, NULL);
}

// Provide simple horizontal metrics: advance equals 8 pixels, left side bearing
// is zero.
STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing)
{
    (void)info; (void)codepoint;
    if (advanceWidth) *advanceWidth = 8;
    if (leftSideBearing) *leftSideBearing = 0;
}

STBTT_DEF int stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2)
{
    (void)info; (void)ch1; (void)ch2;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif // STB_TRUETYPE_H
