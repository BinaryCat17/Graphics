#ifndef COMPUTE_GRAPH_H
#define COMPUTE_GRAPH_H

#include "foundation/memory/arena.h"
#include "stream.h"
#include "render_system.h" // For RendererBackend access if needed, or just fwd declare

#include <stdint.h>
#include <stdbool.h>

typedef struct ComputeGraph ComputeGraph;
typedef struct ComputePass ComputePass;
typedef struct ComputeDoubleBuffer ComputeDoubleBuffer;

// --- Double Buffer (Ping-Pong) ---

// Creates a double buffer wrapper around two existing compatible streams.
// The graph does not take ownership of the streams (you must destroy them).
ComputeDoubleBuffer* compute_double_buffer_create(Stream* stream_a, Stream* stream_b);

void compute_double_buffer_destroy(ComputeDoubleBuffer* buffer);

// Swaps the read/write indices.
void compute_double_buffer_swap(ComputeDoubleBuffer* buffer);

// --- Graph Management ---

ComputeGraph* compute_graph_create(void);
void compute_graph_destroy(ComputeGraph* graph);

// Adds a pass to the graph execution order.
// Returns the Pass handle (pointer).
ComputePass* compute_graph_add_pass(ComputeGraph* graph, uint32_t pipeline_id, uint32_t group_x, uint32_t group_y, uint32_t group_z);

// Sets push constants for a specific pass.
// Data is copied.
void compute_pass_set_push_constants(ComputePass* pass, const void* data, size_t size);

// --- Resource Binding ---

// Bind a single stream to a slot.
void compute_pass_bind_stream(ComputePass* pass, uint32_t binding_slot, Stream* stream);

// Bind the "READ" (Current) buffer of a double buffer to a slot.
void compute_pass_bind_buffer_read(ComputePass* pass, uint32_t binding_slot, ComputeDoubleBuffer* buffer);

// Bind the "WRITE" (Next) buffer of a double buffer to a slot.
void compute_pass_bind_buffer_write(ComputePass* pass, uint32_t binding_slot, ComputeDoubleBuffer* buffer);

// --- Execution ---

// Execute the graph.
// 1. Iterates through passes in order.
// 2. Binds resources.
// 3. Dispatches compute.
// 4. Inserts memory barriers between passes.
void compute_graph_execute(ComputeGraph* graph, RenderSystem* sys);

#endif // COMPUTE_GRAPH_H
