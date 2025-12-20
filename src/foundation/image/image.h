#ifndef FOUNDATION_IMAGE_H
#define FOUNDATION_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Writes raw image data to a PNG file.
 * 
 * @param path The output file path.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param channels Number of channels (1=Grey, 3=RGB, 4=RGBA).
 * @param data Pointer to the raw image data.
 * @param stride_bytes Row stride in bytes. 0 for tightly packed (width * channels).
 * @return true if successful, false otherwise.
 */
bool image_write_png(const char* path, int width, int height, int channels, const void* data, int stride_bytes);

/**
 * @brief Swizzles BGRA data to RGBA in place.
 * 
 * @param data Pointer to the raw image data (4 channels per pixel assumed).
 * @param pixel_count Total number of pixels to process.
 */
void image_swizzle_bgra_to_rgba(uint8_t* data, int pixel_count);

#endif // FOUNDATION_IMAGE_H
