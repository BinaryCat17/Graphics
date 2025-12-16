#include "render_graph.h"
#include "render_graph_private.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- Implementation ---

RgGraph* rg_create(void) {
    RgGraph* graph = (RgGraph*)calloc(1, sizeof(RgGraph));
    return graph;
}

void rg_destroy(RgGraph* graph) {
    if (!graph) return;
    for (size_t i = 0; i < graph->pass_count; ++i) {
        if (graph->passes[i].user_data) free(graph->passes[i].user_data);
    }
    free(graph);
}

static RgResourceHandle add_resource(RgGraph* graph, const char* name) {
    if (graph->resource_count >= MAX_RESOURCES) return RG_INVALID_HANDLE;
    RgResource* res = &graph->resources[graph->resource_count];
    res->handle = (uint32_t)(graph->resource_count + 1);
    strncpy(res->name, name, 63);
    graph->resource_count++;
    return res->handle;
}

RgResourceHandle rg_create_texture(RgGraph* graph, const char* name, uint32_t w, uint32_t h, RgFormat fmt) {
    RgResourceHandle hdl = add_resource(graph, name);
    if (hdl == RG_INVALID_HANDLE) return RG_INVALID_HANDLE;
    
    RgResource* res = &graph->resources[hdl - 1];
    res->type = RG_RESOURCE_TEXTURE;
    res->tex_desc.width = w;
    res->tex_desc.height = h;
    res->tex_desc.format = fmt;
    res->is_imported = false;
    return hdl;
}

RgResourceHandle rg_import_texture(RgGraph* graph, const char* name, void* texture_ptr, uint32_t w, uint32_t h, RgFormat fmt) {
    RgResourceHandle hdl = add_resource(graph, name);
    if (hdl == RG_INVALID_HANDLE) return RG_INVALID_HANDLE;
    
    RgResource* res = &graph->resources[hdl - 1];
    res->type = RG_RESOURCE_TEXTURE;
    res->tex_desc.width = w;
    res->tex_desc.height = h;
    res->tex_desc.format = fmt;
    res->is_imported = true;
    res->external_ptr = texture_ptr;
    return hdl;
}

RgPassBuilder* rg_add_pass(RgGraph* graph, const char* name, size_t user_data_size, void** out_user_data) {
    if (graph->pass_count >= MAX_PASSES) return NULL;
    
    RgPass* pass = &graph->passes[graph->pass_count++];
    strncpy(pass->name, name, 63);
    
    if (user_data_size > 0) {
        pass->user_data = calloc(1, user_data_size);
        if (out_user_data) *out_user_data = pass->user_data;
    }
    
    // We return a pointer to a static builder for simplicity in this C implementation,
    // or we could allocate one. Let's use a static thread-local or just malloc one.
    // Malloc is safer.
    RgPassBuilder* builder = malloc(sizeof(RgPassBuilder));
    builder->graph = graph;
    builder->pass = pass;
    return builder;
}

void rg_pass_read(RgPassBuilder* builder, RgResourceHandle res) {
    RgPass* pass = builder->pass;
    if (pass->resource_count >= MAX_PASS_RESOURCES) return;
    
    RgPassResourceRef* ref = &pass->resources[pass->resource_count++];
    ref->handle = res;
    ref->is_write = false;
    ref->is_depth = false;
}

void rg_pass_write(RgPassBuilder* builder, RgResourceHandle res, RgLoadOp load, RgStoreOp store) {
    RgPass* pass = builder->pass;
    if (pass->resource_count >= MAX_PASS_RESOURCES) return;
    
    RgPassResourceRef* ref = &pass->resources[pass->resource_count++];
    ref->handle = res;
    ref->is_write = true;
    ref->is_depth = false;
    ref->load_op = load;
    ref->store_op = store;
}

void rg_pass_set_depth(RgPassBuilder* builder, RgResourceHandle res, RgLoadOp load, RgStoreOp store) {
    RgPass* pass = builder->pass;
    if (pass->resource_count >= MAX_PASS_RESOURCES) return;
    
    RgPassResourceRef* ref = &pass->resources[pass->resource_count++];
    ref->handle = res;
    ref->is_write = true; // Depth is technically a write usually
    ref->is_depth = true;
    ref->load_op = load;
    ref->store_op = store;
}

void rg_pass_set_execution(RgPassBuilder* builder, RgPassExecuteFn execute_fn) {
    builder->pass->execute_fn = execute_fn;
    // Builder is done, free it
    free(builder);
}

bool rg_compile(RgGraph* graph) {
    // In a full implementation, this would:
    // 1. Cull unused passes (if their outputs aren't used).
    // 2. Reorder passes based on dependencies.
    // 3. Compute image layout transitions and barriers.
    
    // For now, we assume the user adds passes in correct order (Immediate Mode Graph).
    // We just return true properly.
    (void)graph;
    return true;
}

// Execution depends on the backend. This file is generic.
// However, the header declares rg_execute. 
// We should likely move rg_execute to a backend specific file 
// or have this function call a backend interface.
// For this refactor, we will implement rg_execute inside the backend-specific file
// OR providing a backend interface struct to rg_execute.
//
// The header says: void rg_execute(RgGraph* graph, void* backend_context);
// But render_graph.c doesn't know about Vulkan.
// So rg_execute here must just delegate or be defined elsewhere.
//
// Plan: We will implement rg_execute in `vk_render_graph.c`.
// So we DO NOT implement it here. 
// But C doesn't allow split implementation of the same library easily without symbols.
// I will mark rg_execute as extern or leave it out of this .c file.
