#ifndef RENDER_PACKET_H
#define RENDER_PACKET_H

#include "engine/scene/scene.h"

struct RenderFramePacket {
    // The unified scene state for this frame
    Scene* scene;
};
// Wait, I said I would remove the typedef because it's in render_system.h.
// BUT, if this file is included standalone, it needs the typedef?
// 'render_packet.h' seems internal.
// Let's assume it's included where RenderFramePacket is known or we should just use struct.
// Actually, re-reading the error: 'redefinition of typedef RenderFramePacket'.
// So yes, remove typedef.


#endif // RENDER_PACKET_H
