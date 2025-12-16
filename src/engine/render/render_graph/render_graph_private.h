#ifndef RENDER_GRAPH_PRIVATE_H
#define RENDER_GRAPH_PRIVATE_H

#include "render_graph.h"

#define MAX_RESOURCES 64
#define MAX_PASSES 32
#define MAX_PASS_RESOURCES 16

typedef struct RgResource {
    RgResourceHandle handle;
    char name[64];
    RgResourceType type;
    RgTextureDesc tex_desc;
    bool is_imported;
    void* external_ptr; // For imported resources: usually points to a backend wrapper or raw VkImage handle
    
    // Tracking state
    uint32_t current_usage_flags; 
    
    // Backend specific data (void* to keep it decoupled, but casted in backend)
    void* backend_handle; 
} RgResource;

typedef struct RgPassResourceRef {
    RgResourceHandle handle;
    bool is_write;
    bool is_depth;
    RgLoadOp load_op;
    RgStoreOp store_op;
} RgPassResourceRef;

typedef struct RgPass {
    char name[64];
    RgPassResourceRef resources[MAX_PASS_RESOURCES];
    size_t resource_count;
    
    RgPassExecuteFn execute_fn;
    void* user_data;
} RgPass;

struct RgGraph {
    RgResource resources[MAX_RESOURCES];
    size_t resource_count;

    RgPass passes[MAX_PASSES];
    size_t pass_count;
    
    RgPass* current_pass; 
};

struct RgPassBuilder {
    RgGraph* graph;
    RgPass* pass;
};

#endif // RENDER_GRAPH_PRIVATE_H
