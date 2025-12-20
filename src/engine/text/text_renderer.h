#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include "foundation/math/coordinate_systems.h" // Needed for Vec3, Vec4 by value

// Forward Declarations
typedef struct Scene Scene;
typedef struct Font Font;

void scene_add_text(Scene* scene, const Font* font, const char* text, Vec3 pos, float scale, Vec4 color);
void scene_add_text_clipped(Scene* scene, const Font* font, const char* text, Vec3 pos, float scale, Vec4 color, Vec4 clip_rect);

#endif // TEXT_RENDERER_H
