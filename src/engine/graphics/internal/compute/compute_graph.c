#include "engine/graphics/compute_graph.h"
#include "engine/graphics/render_system.h"
#include "engine/graphics/stream.h"
#include "foundation/logger/logger.h"
#include "engine/graphics/internal/backend/renderer_backend.h"
#include <stdlib.h>
#include <string.h>

// --- Structures ---

typedef struct ComputeDoubleBuffer {
    Stream* streams[2];
    int read_index; // 0 or 1
} ComputeDoubleBuffer;

typedef enum {
    RESOURCE_STREAM,
    RESOURCE_DOUBLE_BUFFER_READ,
    RESOURCE_DOUBLE_BUFFER_WRITE
} ResourceType;

typedef struct ComputeResource {
    uint32_t binding;
    ResourceType type;
    union {
        Stream* stream;
        ComputeDoubleBuffer* db;
    };
} ComputeResource;

typedef struct ComputePass {
    uint32_t pipeline_id;
    uint32_t group_x;
    uint32_t group_y;
    uint32_t group_z;
    
    void* push_constants;
    size_t push_constants_size;
    
    ComputeResource* resources;
    size_t resource_count;
    size_t resource_capacity;
} ComputePass;

typedef struct ComputeGraph {
    ComputePass** passes;
    size_t pass_count;
    size_t pass_capacity;
} ComputeGraph;

// --- Double Buffer ---

ComputeDoubleBuffer* compute_double_buffer_create(Stream* stream_a, Stream* stream_b) {
    if (!stream_a || !stream_b) return NULL;
    
    ComputeDoubleBuffer* db = calloc(1, sizeof(ComputeDoubleBuffer));
    db->streams[0] = stream_a;
    db->streams[1] = stream_b;
    db->read_index = 0;
    return db;
}

void compute_double_buffer_destroy(ComputeDoubleBuffer* buffer) {
    if (buffer) {
        free(buffer);
    }
}

void compute_double_buffer_swap(ComputeDoubleBuffer* buffer) {
    if (buffer) {
        buffer->read_index = !buffer->read_index;
    }
}

// --- Graph Management ---

ComputeGraph* compute_graph_create(void) {
    ComputeGraph* graph = calloc(1, sizeof(ComputeGraph));
    graph->pass_capacity = 8;
    graph->passes = (ComputePass**)calloc(graph->pass_capacity, sizeof(ComputePass*));
    return graph;
}

void compute_graph_destroy(ComputeGraph* graph) {
    if (!graph) return;
    
    for (size_t i = 0; i < graph->pass_count; ++i) {
        ComputePass* pass = graph->passes[i];
        if (pass->push_constants) free(pass->push_constants);
        if (pass->resources) free(pass->resources);
        free(pass);
    }
    
    free((void*)graph->passes);
    free(graph);
}

ComputePass* compute_graph_add_pass(ComputeGraph* graph, uint32_t pipeline_id, uint32_t group_x, uint32_t group_y, uint32_t group_z) {
    if (!graph) return NULL;
    
    if (graph->pass_count >= graph->pass_capacity) {
        size_t new_cap = graph->pass_capacity * 2;
        ComputePass** new_arr = (ComputePass**)realloc((void*)graph->passes, new_cap * sizeof(ComputePass*));
        if (!new_arr) return NULL;
        graph->passes = new_arr;
        graph->pass_capacity = new_cap;
    }
    
    ComputePass* pass = calloc(1, sizeof(ComputePass));
    pass->pipeline_id = pipeline_id;
    pass->group_x = group_x;
    pass->group_y = group_y;
    pass->group_z = group_z;
    
    pass->resource_capacity = 8;
    pass->resources = calloc(pass->resource_capacity, sizeof(ComputeResource));
    
    graph->passes[graph->pass_count++] = pass;
    return pass;
}

void compute_pass_set_push_constants(ComputePass* pass, const void* data, size_t size) {
    if (!pass) return;
    
    if (pass->push_constants) free(pass->push_constants);
    pass->push_constants = NULL;
    pass->push_constants_size = 0;
    
    if (data && size > 0) {
        pass->push_constants = malloc(size);
        memcpy(pass->push_constants, data, size);
        pass->push_constants_size = size;
    }
}

void compute_pass_set_dispatch_size(ComputePass* pass, uint32_t group_x, uint32_t group_y, uint32_t group_z) {
    if (!pass) return;
    pass->group_x = group_x;
    pass->group_y = group_y;
    pass->group_z = group_z;
}

static void add_resource(ComputePass* pass, uint32_t binding, ResourceType type, void* ptr) {
    if (!pass) return;
    
    if (pass->resource_count >= pass->resource_capacity) {
        size_t new_cap = pass->resource_capacity * 2;
        ComputeResource* new_arr = realloc(pass->resources, new_cap * sizeof(ComputeResource));
        if (!new_arr) return;
        pass->resources = new_arr;
        pass->resource_capacity = new_cap;
    }
    
    ComputeResource* res = &pass->resources[pass->resource_count++];
    res->binding = binding;
    res->type = type;
    if (type == RESOURCE_STREAM) res->stream = (Stream*)ptr;
    else res->db = (ComputeDoubleBuffer*)ptr;
}

void compute_pass_bind_stream(ComputePass* pass, uint32_t binding_slot, Stream* stream) {
    add_resource(pass, binding_slot, RESOURCE_STREAM, stream);
}

void compute_pass_bind_buffer_read(ComputePass* pass, uint32_t binding_slot, ComputeDoubleBuffer* buffer) {
    add_resource(pass, binding_slot, RESOURCE_DOUBLE_BUFFER_READ, buffer);
}

void compute_pass_bind_buffer_write(ComputePass* pass, uint32_t binding_slot, ComputeDoubleBuffer* buffer) {
    add_resource(pass, binding_slot, RESOURCE_DOUBLE_BUFFER_WRITE, buffer);
}

// --- Execution ---

void compute_graph_execute(ComputeGraph* graph, RenderSystem* sys) {
    if (!graph || !sys) return;
    
    RendererBackend* backend = render_system_get_backend(sys);
    if (!backend || !backend->compute_dispatch) return;

    for (size_t i = 0; i < graph->pass_count; ++i) {
        ComputePass* pass = graph->passes[i];
        
        // 0. Bind Global Input (Reserved Slot 1)
        Stream* input_stream = render_system_get_input_stream(sys);
        if (input_stream) {
            stream_bind_compute(input_stream, 1);
        }

        // 1. Bind Resources
        for (size_t r = 0; r < pass->resource_count; ++r) {
            ComputeResource* res = &pass->resources[r];
            Stream* stream = NULL;
            
            switch (res->type) {
                case RESOURCE_STREAM:
                    stream = res->stream;
                    break;
                case RESOURCE_DOUBLE_BUFFER_READ:
                    stream = res->db->streams[res->db->read_index];
                    break;
                case RESOURCE_DOUBLE_BUFFER_WRITE:
                    stream = res->db->streams[!res->db->read_index];
                    break;
            }
            
            if (stream) {
                stream_bind_compute(stream, res->binding);
            }
        }
        
        // 2. Dispatch
        backend->compute_dispatch(backend, pass->pipeline_id, 
                                pass->group_x, pass->group_y, pass->group_z, 
                                pass->push_constants, pass->push_constants_size);
                                
        // 3. Barrier
        // TODO: More granular barriers based on dependency analysis?
        // For now, global barrier between passes is safe and simple.
        if (backend->compute_wait) {
            backend->compute_wait(backend);
        }
    }
}
