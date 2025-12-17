#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

struct GpuInstanceData {
    mat4 model;
    vec4 color;
    vec4 uv_rect;
    vec4 params;
};

// Set 0: Texture (Sampler)
// Set 1: Instances (SSBO)

layout(std430, set = 1, binding = 0) readonly buffer InstanceBuffer {
    GpuInstanceData objects[];
} instances;

layout(push_constant) uniform Push {
    mat4 view_proj;
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec4 fragParams;

void main() {
    GpuInstanceData obj = instances.objects[gl_InstanceIndex];
    
    gl_Position = pc.view_proj * obj.model * vec4(inPosition, 1.0);
    fragColor = obj.color;
    fragParams = obj.params;
    
    // Transform UV
    fragUV = inUV * obj.uv_rect.zw + obj.uv_rect.xy;
}
