#ifndef PRIMITIVE_BATCHER_H
#define PRIMITIVE_BATCHER_H

#include "foundation/math/math_types.h"
#include "graphics_types.h"

typedef struct PrimitiveBatcher PrimitiveBatcher;
typedef struct RenderSystem RenderSystem;
typedef struct Scene Scene;

// Lifecycle
PrimitiveBatcher* primitive_batcher_create(RenderSystem* rs);
void primitive_batcher_destroy(PrimitiveBatcher* batcher);

void primitive_batcher_set_pipeline(PrimitiveBatcher* batcher, uint32_t pipeline_id);
void primitive_batcher_set_tag(PrimitiveBatcher* batcher, const char* tag);

void primitive_batcher_begin(PrimitiveBatcher* batcher);
void primitive_batcher_end(PrimitiveBatcher* batcher, Scene* scene);

// Drawing commands (Immediate Mode style)
void primitive_batcher_push_line(PrimitiveBatcher* batcher, Vec3 start, Vec3 end, Vec4 color, float thickness);
void primitive_batcher_push_triangle(PrimitiveBatcher* batcher, Vec3 a, Vec3 b, Vec3 c, Vec4 color);
void primitive_batcher_push_rect(PrimitiveBatcher* batcher, Vec3 pos, Vec2 size, Vec4 color);
void primitive_batcher_push_rect_line(PrimitiveBatcher* batcher, Vec3 pos, Vec2 size, Vec4 color, float thickness);
void primitive_batcher_push_cubic_bezier(PrimitiveBatcher* batcher, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec4 color, float thickness, int segments);

#endif // PRIMITIVE_BATCHER_H
