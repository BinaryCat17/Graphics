#ifndef GRAPHICS_TYPES_H
#define GRAPHICS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "foundation/math/math_types.h"

// =================================================================================================
// [ENUMS]
// =================================================================================================

typedef enum PixelFormat {
    PIXEL_FORMAT_UNKNOWN = 0,
    PIXEL_FORMAT_R8_UNORM,
    PIXEL_FORMAT_RG8_UNORM,
    PIXEL_FORMAT_RGB8_UNORM,
    PIXEL_FORMAT_RGBA8_UNORM,
    PIXEL_FORMAT_R16_FLOAT,
    PIXEL_FORMAT_RG16_FLOAT,
    PIXEL_FORMAT_RGBA16_FLOAT,
    PIXEL_FORMAT_D32_SFLOAT,
    PIXEL_FORMAT_D24_UNORM_S8_UINT
} PixelFormat;

// Forward Declarations
typedef struct Stream Stream;
typedef struct Mesh Mesh;

// =================================================================================================
// [LAYER CONSTANTS]
// =================================================================================================

// --- Orthographic Projection Range ---
// Defined in render_system_begin_frame: mat4_orthographic(..., -100.0f, 100.0f)
// Note: Due to OpenGL->Vulkan clip space differences and the specific projection matrix,
// Visible Z range is effectively [-100.0, 0.0] where:
// Z = 0.0   -> Depth 0.0 (Near / Topmost)
// Z = -100.0 -> Depth 1.0 (Far / Bottommost)
// Therefore, HIGHER Z values (closer to 0) render ON TOP of LOWER Z values.

#define RENDER_ORTHO_Z_NEAR (-100.0f)
#define RENDER_ORTHO_Z_FAR  (100.0f)

// --- UI Depth Layers ---

// The deepest background layer (e.g., Canvas background)
// Start deeper so children can stack on top (towards 0.0)
#define RENDER_LAYER_UI_BASE        (-10.0f)

// Standard UI Panels (Windows, Sidebars)
#define RENDER_LAYER_UI_PANEL       (-9.0f)

// Overlay Elements
#define RENDER_LAYER_UI_OVERLAY     (-1.0f)

// --- Increments ---
// Increment Z (move closer to 0 / positive) for each child
#define RENDER_DEPTH_STEP_UI        (0.01f)
#define RENDER_DEPTH_STEP_CONTENT   (0.001f)


// =================================================================================================
// [DESCRIPTOR LAYOUT]
// =================================================================================================

typedef struct DescriptorBindingDef {
    uint32_t binding;
    uint32_t descriptor_type; // 0=Sampler, 1=StorageBuffer, 2=StorageImage
    uint32_t descriptor_count;
    uint32_t stage_flags;     // 1=Vert, 2=Frag, 4=Comp
} DescriptorBindingDef;

typedef struct DescriptorLayoutDef {
    uint32_t binding_count;
    DescriptorBindingDef bindings[16];
} DescriptorLayoutDef;


// =================================================================================================
// [RENDER COMMANDS]
// =================================================================================================

typedef enum RenderCommandType {
    RENDER_CMD_BIND_PIPELINE,
    RENDER_CMD_BIND_BUFFER,       // Bind SSBO/UBO to a specific slot
    RENDER_CMD_BIND_VERTEX_BUFFER,// Bind Vertex Buffer
    RENDER_CMD_BIND_INDEX_BUFFER, // Bind Index Buffer
    RENDER_CMD_UPDATE_BUFFER,     // Update buffer data (inline)
    RENDER_CMD_DRAW,              // Draw non-indexed
    RENDER_CMD_DRAW_INDEXED,      // Draw indexed
    RENDER_CMD_DRAW_INDIRECT,     // Indirect draw
    RENDER_CMD_SET_VIEWPORT,
    RENDER_CMD_SET_SCISSOR,
    RENDER_CMD_PUSH_CONSTANTS,
    RENDER_CMD_BARRIER,           // Memory barrier
    RENDER_CMD_BEGIN_PASS,        // Start a render pass
    RENDER_CMD_END_PASS           // End a render pass
} RenderCommandType;

typedef struct RenderCmdBeginPass {
    uint32_t target_image_id; // 0 for swapchain
    float clear_color[4];
    bool should_clear;
} RenderCmdBeginPass;

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
        RenderCmdBeginPass begin_pass;
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


// GPU Instance Data Layout (std140/std430 compatible)
// Used for UI and Sprite rendering
typedef struct GpuInstanceData {
    Mat4 model;
    Vec4 color;
    Vec4 uv_rect;
    Vec4 params_1;
    Vec4 params_2;
    Vec4 clip_rect;
} GpuInstanceData;


// =================================================================================================
// [RENDER BATCH]
// =================================================================================================

// Represents a 3D draw call or compute dispatch
typedef struct RenderBatch {
    // Pipeline / Shader
    uint32_t pipeline_id;
    
    // Resources
    Mesh* mesh;                // If drawing a mesh
    
    // Custom Bindings (for SSBOs/UBOs)
    Stream* bind_buffers[4];   // Streams
    uint32_t bind_slots[4];
    uint32_t bind_count;

    // Geometry Streams (Optional, overrides Mesh)
    Stream* vertex_stream;
    Stream* index_stream;

    void* material_buffer;     // Legacy/Specific material data
    uint32_t material_size;

    // Draw Parameters
    uint32_t vertex_count;     // Used if mesh is NULL
    uint32_t index_count;      // Used if mesh is NULL but indexed (rare)
    uint32_t instance_count;
    uint32_t first_instance;
    
    // Transform / Instance Data
    // Pointer to an array of instance data (matrices, colors, etc.)
    void* instance_buffer; 
    size_t instance_buffer_size;

    // Sorting
    float sort_key; // Distance to camera or layer index
    uint32_t layer_id;

    // Pipeline Tagging
    char draw_list[32]; 
} RenderBatch;

#endif // GRAPHICS_TYPES_H
