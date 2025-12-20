#pragma once
#include "engine/scene/scene.h"
#include "foundation/math/coordinate_systems.h"
#include "engine/text/font.h"

void scene_add_text(Scene* scene, const Font* font, const char* text, Vec3 pos, float scale, Vec4 color);
void scene_add_text_clipped(Scene* scene, const Font* font, const char* text, Vec3 pos, float scale, Vec4 color, Vec4 clip_rect);
