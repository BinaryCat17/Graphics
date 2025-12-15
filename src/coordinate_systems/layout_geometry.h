#pragma once

#include "coordinate_systems.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LayoutBox {
    Vec2 origin;
    Vec2 size;
} LayoutBox;

typedef struct LayoutResult {
    LayoutBox logical;
    LayoutBox device;
} LayoutResult;

LayoutResult layout_resolve(const LayoutBox *logical, const RenderContext *ctx);
int layout_hit_test(const LayoutResult *layout, Vec2 logical_point);

#ifdef __cplusplus
}
#endif

