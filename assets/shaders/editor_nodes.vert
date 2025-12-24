#version 450

struct NodeData {
    vec2 pos;
    vec2 size;
    uint id;
    uint padding;
};

layout(std430, set = 1, binding = 0) readonly buffer Nodes {
    NodeData nodes[];
};

layout(push_constant) uniform Push {
    mat4 view_proj;
} push;

layout(location = 0) out vec2 outUV;

void main() {
    NodeData node = nodes[gl_InstanceIndex];
    
    // Quad vertices (Triangle Strip or indexed triangles)
    // 0: 0,0
    // 1: 1,0
    // 2: 0,1
    // 3: 1,1
    // If drawing with 4 vertices and Triangle Strip
    
    // Let's assume we draw 6 vertices (2 triangles) or 4 with index buffer.
    // Zero-Copy usually draws without vertex buffer, so we generate coords.
    // Vertex Indices: 0, 1, 2, 0, 2, 3 (CCW)
    
    const vec2 corners[6] = vec2[](
        vec2(0, 0), vec2(1, 0), vec2(1, 1),
        vec2(0, 0), vec2(1, 1), vec2(0, 1)
    );
    
    vec2 local_uv = corners[gl_VertexIndex % 6];
    outUV = local_uv;
    
    vec2 world_pos = node.pos + local_uv * node.size;
    
    // Invert Y for Vulkan if using orthographic projection that matches screen coords (Y down)
    // If Y is up, standard. Assuming Y down (UI coords).
    
    gl_Position = push.view_proj * vec4(world_pos, 0.0, 1.0);
}
