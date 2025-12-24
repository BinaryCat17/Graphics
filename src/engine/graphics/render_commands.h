#ifndef RENDER_COMMANDS_H
#define RENDER_COMMANDS_H

#include <stdint.h>
#include <stddef.h>
#include "foundation/math/math_types.h"

// Forward Declaration
typedef struct Stream Stream;

typedef enum RenderCommandType {
    RENDER_CMD_BIND_PIPELINE,
    RENDER_CMD_BIND_BUFFER,       // Bind SSBO/UBO to a specific slot
    RENDER_CMD_UPDATE_BUFFER,     // Update buffer data (inline)
    RENDER_CMD_DRAW,              // Draw non-indexed
    RENDER_CMD_DRAW_INDEXED,      // Draw indexed
    RENDER_CMD_DRAW_INDIRECT,     // Indirect draw
    RENDER_CMD_SET_VIEWPORT,
    RENDER_CMD_SET_SCISSOR,
    RENDER_CMD_PUSH_CONSTANTS,
    RENDER_CMD_BARRIER            // Memory barrier
} RenderCommandType;

typedef struct RenderCmdBindPipeline {
    uint32_t pipeline_id;
} RenderCmdBindPipeline;

typedef struct RenderCmdBindBuffer {
    uint32_t slot;
    Stream* stream;
} RenderCmdBindBuffer;

typedef struct RenderCmdUpdateBuffer {
    Stream* stream;
    const void* data;
    size_t size;
    size_t offset;
} RenderCmdUpdateBuffer;

typedef struct RenderCmdDraw {
    uint32_t vertex_count;
    uint32_t instance_count;
    uint32_t first_vertex;
    uint32_t first_instance;
} RenderCmdDraw;

typedef struct RenderCmdDrawIndexed {
    uint32_t index_count;
    uint32_t instance_count;
    uint32_t first_index;
    int32_t vertex_offset;
    uint32_t first_instance;
} RenderCmdDrawIndexed;

typedef struct RenderCmdDrawIndirect {
    Stream* stream; // Buffer containing draw commands
    size_t offset;
    uint32_t draw_count;
    uint32_t stride;
} RenderCmdDrawIndirect;

typedef struct RenderCmdViewport {
    float x, y, w, h;
    float min_depth, max_depth;
} RenderCmdViewport;

typedef struct RenderCmdScissor {
    int32_t x, y;
    uint32_t w, h;
} RenderCmdScissor;

typedef struct RenderCmdPushConstants {
    void* data;
    uint32_t size;
    uint32_t stage_flags; // 1=Vert, 2=Frag, 4=Comp
} RenderCmdPushConstants;

typedef struct RenderCommand {
    RenderCommandType type;
    union {
        RenderCmdBindPipeline bind_pipeline;
        RenderCmdBindBuffer bind_buffer;
        RenderCmdUpdateBuffer update_buffer;
        RenderCmdDraw draw;
        RenderCmdDrawIndexed draw_indexed;
        RenderCmdDrawIndirect draw_indirect;
        RenderCmdViewport viewport;
        RenderCmdScissor scissor;
        RenderCmdPushConstants push_constants;
    };
} RenderCommand;

typedef struct RenderCommandList {
    RenderCommand* commands;
    uint32_t capacity;
    uint32_t count;
} RenderCommandList;

#endif // RENDER_COMMANDS_H
