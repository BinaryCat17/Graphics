#include "primitive_batcher.h"
#include "render_system.h"
#include "stream.h"
#include "engine/scene/render_packet.h"
#include "engine/assets/assets.h"
#include "foundation/memory/arena.h"
#include "foundation/logger/logger.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VERTICES 65536
#define MAX_INDICES  (MAX_VERTICES * 3)

typedef struct PrimitiveVertex {
    Vec3 pos;
    Vec4 color;
    Vec2 uv;
} PrimitiveVertex;

struct PrimitiveBatcher {
    RenderSystem* rs;
    Stream* vertex_stream;
    Stream* index_stream;
    
    // CPU Staging
    PrimitiveVertex* vertices;
    uint32_t* indices;
    
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_capacity;
    uint32_t index_capacity;

    uint32_t pipeline_id; 
    
    bool is_drawing;
};

PrimitiveBatcher* primitive_batcher_create(RenderSystem* rs) {
    if (!rs) return NULL;
    
    PrimitiveBatcher* batcher = calloc(1, sizeof(PrimitiveBatcher));
    batcher->rs = rs;
    batcher->vertex_capacity = MAX_VERTICES;
    batcher->index_capacity = MAX_INDICES;
    
    batcher->vertices = malloc(batcher->vertex_capacity * sizeof(PrimitiveVertex));
    batcher->indices = malloc(batcher->index_capacity * sizeof(uint32_t));
    
    // Create GPU Streams
    batcher->vertex_stream = stream_create(rs, STREAM_CUSTOM, batcher->vertex_capacity, sizeof(PrimitiveVertex));
    batcher->index_stream = stream_create(rs, STREAM_UINT, batcher->index_capacity, sizeof(uint32_t));
    
    batcher->pipeline_id = 0; 

    return batcher;
}

void primitive_batcher_destroy(PrimitiveBatcher* batcher) {
    if (!batcher) return;
    
    if (batcher->vertex_stream) stream_destroy(batcher->vertex_stream);
    if (batcher->index_stream) stream_destroy(batcher->index_stream);
    
    if (batcher->vertices) free(batcher->vertices);
    if (batcher->indices) free(batcher->indices);
    
    free(batcher);
}

void primitive_batcher_set_pipeline(PrimitiveBatcher* batcher, uint32_t pipeline_id) {
    if (batcher) batcher->pipeline_id = pipeline_id;
}

void primitive_batcher_begin(PrimitiveBatcher* batcher) {
    if (!batcher) return;
    batcher->vertex_count = 0;
    batcher->index_count = 0;
    batcher->is_drawing = true;
}

void primitive_batcher_end(PrimitiveBatcher* batcher, Scene* scene) {
    if (!batcher || !batcher->is_drawing || batcher->index_count == 0) {
        if(batcher) batcher->is_drawing = false;
        return;
    }
    
    // 1. Upload to GPU
    stream_set_data(batcher->vertex_stream, batcher->vertices, batcher->vertex_count);
    stream_set_data(batcher->index_stream, batcher->indices, batcher->index_count);
    
    // 2. Create RenderBatch
    RenderBatch batch = {0};
    batch.pipeline_id = batcher->pipeline_id; 
    
    // Use Vertex Pulling: Bind Vertex Buffer as SSBO (Slot 0)
    batch.bind_buffers[0] = batcher->vertex_stream;
    batch.bind_slots[0] = 0;
    batch.bind_count = 1;

    // Use Native Index Buffer
    batch.index_stream = batcher->index_stream;
    
    batch.vertex_count = batcher->vertex_count;
    batch.index_count = batcher->index_count;
    batch.instance_count = 1;
    batch.first_instance = 0;
    
    // Push
    scene_push_render_batch(scene, batch);
    
    batcher->is_drawing = false;
}

static void pb_check_capacity(PrimitiveBatcher* batcher, uint32_t v_add, uint32_t i_add) {
    if (batcher->vertex_count + v_add >= batcher->vertex_capacity) return; 
    if (batcher->index_count + i_add >= batcher->index_capacity) return;
}

void primitive_batcher_push_triangle(PrimitiveBatcher* batcher, Vec3 a, Vec3 b, Vec3 c, Vec4 color) {
    if (!batcher) return;
    pb_check_capacity(batcher, 3, 3);
    
    uint32_t base = batcher->vertex_count;
    
    batcher->vertices[base + 0] = (PrimitiveVertex){ a, color, {0,0} };
    batcher->vertices[base + 1] = (PrimitiveVertex){ b, color, {0.5f,1} };
    batcher->vertices[base + 2] = (PrimitiveVertex){ c, color, {1,0} };
    
    batcher->indices[batcher->index_count++] = base + 0;
    batcher->indices[batcher->index_count++] = base + 1;
    batcher->indices[batcher->index_count++] = base + 2;
    
    batcher->vertex_count += 3;
}

void primitive_batcher_push_rect(PrimitiveBatcher* batcher, Vec3 pos, Vec2 size, Vec4 color) {
    if (!batcher) return;
    
    Vec3 p0 = pos;
    Vec3 p1 = {pos.x + size.x, pos.y, pos.z};
    Vec3 p2 = {pos.x + size.x, pos.y + size.y, pos.z};
    Vec3 p3 = {pos.x, pos.y + size.y, pos.z};
    
    pb_check_capacity(batcher, 4, 6);
    uint32_t base = batcher->vertex_count;
    
    batcher->vertices[base + 0] = (PrimitiveVertex){ p0, color, {0,0} };
    batcher->vertices[base + 1] = (PrimitiveVertex){ p1, color, {1,0} };
    batcher->vertices[base + 2] = (PrimitiveVertex){ p2, color, {1,1} };
    batcher->vertices[base + 3] = (PrimitiveVertex){ p3, color, {0,1} };
    
    batcher->indices[batcher->index_count++] = base + 0;
    batcher->indices[batcher->index_count++] = base + 1;
    batcher->indices[batcher->index_count++] = base + 2;
    
    batcher->indices[batcher->index_count++] = base + 0;
    batcher->indices[batcher->index_count++] = base + 2;
    batcher->indices[batcher->index_count++] = base + 3;
    
    batcher->vertex_count += 4;
}

void primitive_batcher_push_line(PrimitiveBatcher* batcher, Vec3 start, Vec3 end, Vec4 color, float thickness) {
    if (!batcher) return;
    
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.0001f) return;
    
    float nx = -dy / len * (thickness * 0.5f);
    float ny = dx / len * (thickness * 0.5f);
    
    Vec3 p0 = {start.x + nx, start.y + ny, start.z};
    Vec3 p1 = {start.x - nx, start.y - ny, start.z};
    Vec3 p2 = {end.x - nx, end.y - ny, end.z};
    Vec3 p3 = {end.x + nx, end.y + ny, end.z};
    
    pb_check_capacity(batcher, 4, 6);
    uint32_t base = batcher->vertex_count;
    
    batcher->vertices[base + 0] = (PrimitiveVertex){ p0, color, {0,0} };
    batcher->vertices[base + 1] = (PrimitiveVertex){ p1, color, {0,1} };
    batcher->vertices[base + 2] = (PrimitiveVertex){ p2, color, {1,1} };
    batcher->vertices[base + 3] = (PrimitiveVertex){ p3, color, {1,0} };
    
    batcher->indices[batcher->index_count++] = base + 0;
    batcher->indices[batcher->index_count++] = base + 1;
    batcher->indices[batcher->index_count++] = base + 2;
    
    batcher->indices[batcher->index_count++] = base + 0;
    batcher->indices[batcher->index_count++] = base + 2;
    batcher->indices[batcher->index_count++] = base + 3;
    
    batcher->vertex_count += 4;
}

void primitive_batcher_push_rect_line(PrimitiveBatcher* batcher, Vec3 pos, Vec2 size, Vec4 color, float thickness) {
    primitive_batcher_push_line(batcher, pos, (Vec3){pos.x + size.x, pos.y, pos.z}, color, thickness);
    primitive_batcher_push_line(batcher, (Vec3){pos.x, pos.y + size.y, pos.z}, (Vec3){pos.x + size.x, pos.y + size.y, pos.z}, color, thickness);
    primitive_batcher_push_line(batcher, pos, (Vec3){pos.x, pos.y + size.y, pos.z}, color, thickness);
    primitive_batcher_push_line(batcher, (Vec3){pos.x + size.x, pos.y, pos.z}, (Vec3){pos.x + size.x, pos.y + size.y, pos.z}, color, thickness);
}

void primitive_batcher_push_cubic_bezier(PrimitiveBatcher* batcher, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec4 color, float thickness, int segments) {
    if (!batcher || segments < 2) return;
    
    Vec3 prev = p0;
    float dt = 1.0f / (float)segments;
    
    for (int i = 1; i <= segments; ++i) {
        float t = (float)i * dt;
        float it = 1.0f - t;
        
        // B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3
        float c0 = it * it * it;
        float c1 = 3 * it * it * t;
        float c2 = 3 * it * t * t;
        float c3 = t * t * t;
        
        Vec3 curr = {
            c0 * p0.x + c1 * p1.x + c2 * p2.x + c3 * p3.x,
            c0 * p0.y + c1 * p1.y + c2 * p2.y + c3 * p3.y,
            c0 * p0.z + c1 * p1.z + c2 * p2.z + c3 * p3.z
        };
        
        primitive_batcher_push_line(batcher, prev, curr, color, thickness);
        prev = curr;
    }
}
