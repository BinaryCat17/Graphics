#ifndef RENDER_BATCH_H
#define RENDER_BATCH_H

#include "foundation/math/math_types.h"
#include <stdint.h>
#include <stddef.h>

// Forward decl
struct Mesh;
typedef struct Stream Stream;

// Represents a 3D draw call or compute dispatch
typedef struct RenderBatch {
    // Pipeline / Shader
    uint32_t pipeline_id;
    
    // Resources
    struct Mesh* mesh;         // If drawing a mesh
    
    // Custom Bindings (for SSBOs/UBOs)
    Stream* bind_buffers[4];   // Streams
    uint32_t bind_slots[4];
    uint32_t bind_count;

    void* material_buffer;     // Legacy/Specific material data
    uint32_t material_size;

    // Draw Parameters
    uint32_t vertex_count;     // Used if mesh is NULL
    uint32_t index_count;      // Used if mesh is NULL but indexed (rare)
    uint32_t instance_count;
    uint32_t first_instance;
    
    // Transform / Instance Data
    // Pointer to an array of instance data (matrices, colors, etc.)
    // In Phase 2, we assume this is handled by the backend reading a Stream, 
    // but for now we might pass a pointer to CPU memory to be uploaded.
    void* instance_buffer; 
    size_t instance_buffer_size;

    // Sorting
    float sort_key; // Distance to camera or layer index
    uint32_t layer_id;
} RenderBatch;

#endif // RENDER_BATCH_H
