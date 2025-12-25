#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

struct GpuInstanceData {
    mat4 model;
    vec4 color;
    vec4 uv_rect;
    vec4 params_1;
    vec4 params_2;
    vec4 clip_rect;
};

layout(std140, set = 1, binding = 0) readonly buffer InstanceBuffer {
    GpuInstanceData objects[];
} instances;

layout(push_constant) uniform Push {
    mat4 view_proj;
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec4 fragParams;
layout(location = 3) out vec4 fragExtra;
layout(location = 4) out flat vec4 fragClipRect;
layout(location = 5) out vec3 fragWorldPos;
layout(location = 6) out vec2 fragOrigUV;
layout(location = 7) out vec4 fragUVRect;
layout(location = 8) out vec2 fragTargetSize;

void main() {
    GpuInstanceData inst = instances.objects[gl_InstanceIndex];
    
    // Transform Position
    vec4 world_pos = inst.model * vec4(inPosition, 1.0);
    gl_Position = pc.view_proj * world_pos;
    
    // Pass Data to Fragment
    fragColor = inst.color;
    
    // UV Calculation
    // inUV is 0..1. Map to uv_rect.
    // uv_rect = (u, v, w, h)
    fragUV = inst.uv_rect.xy + inUV * inst.uv_rect.zw;
    fragOrigUV = inUV; // 0..1 for SDF
    fragUVRect = inst.uv_rect;
    
    fragParams = inst.params_1;
    fragExtra = inst.params_2;
    fragClipRect = inst.clip_rect;
    fragWorldPos = world_pos.xyz;
    fragTargetSize = vec2(inst.model[0][0], inst.model[1][1]); // Extract scale X/Y
}