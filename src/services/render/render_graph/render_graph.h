#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Handle Types ---
typedef uint32_t RgResourceHandle;
#define RG_INVALID_HANDLE 0

// --- Enums ---

typedef enum RgResourceType {
    RG_RESOURCE_TEXTURE,
    RG_RESOURCE_BUFFER
} RgResourceType;

typedef enum RgFormat {
    RG_FORMAT_UNDEFINED = 0,
    RG_FORMAT_R8G8B8A8_UNORM,
    RG_FORMAT_B8G8R8A8_UNORM, // Common swapchain format
    RG_FORMAT_D32_SFLOAT,     // Depth
    RG_FORMAT_RGBA32_SFLOAT,  // Compute/Math precision
    // Add more as needed
} RgFormat;

typedef enum RgTextureUsage {
    RG_USAGE_COLOR_ATTACHMENT = 1 << 0,
    RG_USAGE_DEPTH_ATTACHMENT = 1 << 1,
    RG_USAGE_SAMPLED          = 1 << 2,
    RG_USAGE_TRANSFER_DST     = 1 << 3,
    RG_USAGE_TRANSFER_SRC     = 1 << 4,
    RG_USAGE_PRESENT          = 1 << 5  // Special flag for swapchain backbuffer
} RgTextureUsage;

typedef enum RgLoadOp {
    RG_LOAD_OP_DONT_CARE,
    RG_LOAD_OP_CLEAR,
    RG_LOAD_OP_LOAD
} RgLoadOp;

typedef enum RgStoreOp {
    RG_STORE_OP_DONT_CARE,
    RG_STORE_OP_STORE
} RgStoreOp;

// --- Resource Descriptions ---

typedef struct RgTextureDesc {
    const char* name;
    uint32_t width;
    uint32_t height;
    RgFormat format;
} RgTextureDesc;

// --- Pass Construction ---

typedef struct RgGraph RgGraph;
typedef struct RgPassBuilder RgPassBuilder;

// Data passed to the execution callback (contains backend-specific command buffer, etc.)
typedef struct RgCmdBuffer {
    void* backend_cmd; // e.g. VkCommandBuffer
    void* backend_state;
} RgCmdBuffer;

typedef void (*RgPassExecuteFn)(RgCmdBuffer* cmd, void* user_data);

// --- API ---

RgGraph* rg_create(void);
void rg_destroy(RgGraph* graph);

// Declare a transient resource managed by the graph
RgResourceHandle rg_create_texture(RgGraph* graph, const char* name, uint32_t w, uint32_t h, RgFormat fmt);

// Import an external resource (e.g., Swapchain Image)
// The resource is not managed/destroyed by the graph, but its barriers are handled.
RgResourceHandle rg_import_texture(RgGraph* graph, const char* name, void* texture_ptr, uint32_t w, uint32_t h, RgFormat fmt);

// Start defining a pass
RgPassBuilder* rg_add_pass(RgGraph* graph, const char* name, size_t user_data_size, void** out_user_data);

// --- Builder API (used inside setup phase) ---

// Declare that this pass reads from a resource
void rg_pass_read(RgPassBuilder* builder, RgResourceHandle res);

// Declare that this pass writes to a color attachment
void rg_pass_write(RgPassBuilder* builder, RgResourceHandle res, RgLoadOp load, RgStoreOp store);

// Declare that this pass uses a depth attachment
void rg_pass_set_depth(RgPassBuilder* builder, RgResourceHandle res, RgLoadOp load, RgStoreOp store);

// Set the execution callback
void rg_pass_set_execution(RgPassBuilder* builder, RgPassExecuteFn execute_fn);

// --- Execution ---

// Compiles the graph (sorts passes, calculates barriers)
bool rg_compile(RgGraph* graph);

// Executes the graph using a backend implementation
// 'backend_context' is passed to the execute callbacks
void rg_execute(RgGraph* graph, void* backend_context);

#ifdef __cplusplus
}
#endif

#endif // RENDER_GRAPH_H
