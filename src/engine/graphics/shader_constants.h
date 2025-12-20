#ifndef SHADER_CONSTANTS_H
#define SHADER_CONSTANTS_H

// UI/Generic Shader Modes
// Must match assets/shaders/ui_default.frag logic
typedef enum ShaderUiMode {
    SHADER_UI_MODE_SOLID        = 0, // Solid Color
    SHADER_UI_MODE_TEXTURED     = 1, // Font/Bitmap
    SHADER_UI_MODE_USER_TEXTURE = 2, // Compute Result/Image
    SHADER_UI_MODE_9_SLICE      = 3, // UI Panel
    SHADER_UI_MODE_SDF_BOX      = 4  // Rounded Box
} ShaderUiMode;

#endif // SHADER_CONSTANTS_H
