#version 450
// Zero-Copy Instanced Vertex Shader

// Set 1: Data Buffers
layout(std430, set = 1, binding = 0) readonly buffer PosX { float data[]; } posX;
layout(std430, set = 1, binding = 1) readonly buffer PosY { float data[]; } posY;
layout(std430, set = 1, binding = 2) readonly buffer PosZ { float data[]; } posZ;
layout(std430, set = 1, binding = 3) readonly buffer Color { vec4 data[]; } colors;

layout(push_constant) uniform Push {
    mat4 view_proj;
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    uint instance_id = gl_InstanceIndex;
    uint vertex_id = gl_VertexIndex; // 0..5 for a quad

    // Fetch Data
    vec3 center = vec3(
        posX.data[instance_id],
        posY.data[instance_id],
        posZ.data[instance_id]
    );
    vec4 color = colors.data[instance_id];

    // Generate Quad (Billboard)
    const vec2 offsets[6] = vec2[](
        vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5), // Tri 1
        vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5)  // Tri 2
    );
    
    const vec2 uvs[6] = vec2[](
        vec2(0, 0), vec2(1, 0), vec2(1, 1),
        vec2(0, 0), vec2(1, 1), vec2(0, 1)
    );

    vec3 pos = center + vec3(offsets[vertex_id], 0.0); 
    
    gl_Position = pc.view_proj * vec4(pos, 1.0);
    fragColor = color;
    fragUV = uvs[vertex_id];
}
