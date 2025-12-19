#ifndef RENDER_PACKET_H
#define RENDER_PACKET_H

#include "engine/graphics/scene/scene.h"

typedef struct RenderFramePacket {
    // The unified scene state for this frame
    Scene scene;
} RenderFramePacket;

#endif // RENDER_PACKET_H
