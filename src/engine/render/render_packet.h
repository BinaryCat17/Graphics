#ifndef RENDER_PACKET_H
#define RENDER_PACKET_H

#include "engine/ui/ui_renderer.h"
#include "foundation/math/coordinate_systems.h"

typedef struct RenderFramePacket {
    UiDrawList ui_draw_list;
    CoordinateSystem2D transformer;
    // Add 3D scene data here later (e.g., Camera, MeshInstances)
} RenderFramePacket;

#endif // RENDER_PACKET_H
