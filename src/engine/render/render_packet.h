#ifndef RENDER_PACKET_H
#define RENDER_PACKET_H

#include "engine/scene/scene_def.h"

typedef struct RenderFramePacket {
    // The unified scene state for this frame
    Scene scene;
    
    // Legacy support while migrating (optional, or just remove)
    // CoordinateSystem2D transformer; 
} RenderFramePacket;

#endif // RENDER_PACKET_H
