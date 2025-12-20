#include "image.h"
#include "foundation/logger/logger.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool image_write_png(const char* path, int width, int height, int channels, const void* data, int stride_bytes) {
    if (!path || !data) return false;
    
    // stbi_write_png returns 0 on failure, non-zero on success
    int res = stbi_write_png(path, width, height, channels, data, stride_bytes);
    
    if (res == 0) {
        LOG_ERROR("Failed to write PNG to %s", path);
        return false;
    }
    
    return true;
}

void image_swizzle_bgra_to_rgba(uint8_t* data, int pixel_count) {
    if (!data) return;
    for (int i = 0; i < pixel_count; i++) {
        uint8_t b = data[i * 4 + 0];
        uint8_t r = data[i * 4 + 2];
        data[i * 4 + 0] = r;
        data[i * 4 + 2] = b;
    }
}
