#pragma once

#include "core/math/coordinate_spaces.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Logical layout rect in UI units. */
typedef struct LayoutBox {
    Vec2 origin;
    Vec2 size;
} LayoutBox;

/** Logical layout along with device-space result. */
typedef struct LayoutResult {
    LayoutBox logical;
    LayoutBox device;
} LayoutResult;

LayoutResult layout_resolve(const LayoutBox *logical, const RenderContext *ctx);
int layout_hit_test(const LayoutResult *layout, Vec2 logical_point);

#ifdef __cplusplus
}
#endif

