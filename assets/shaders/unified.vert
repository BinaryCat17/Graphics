#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

struct GpuInstanceData {
    mat4 model;
    vec4 color;
    vec4 uv_rect;
    vec4 params;
    vec4 extra; // Used for Curve Data (P0, P3)
    vec4 clip_rect; // x,y,w,h (World/Screen Space)
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
layout(location = 4) out vec4 fragClipRect;
layout(location = 5) out vec3 fragWorldPos;

void main() {
    GpuInstanceData obj = instances.objects[gl_InstanceIndex];
    
    vec4 worldPos = obj.model * vec4(inPosition, 1.0);
    gl_Position = pc.view_proj * worldPos;
    
    fragColor = obj.color;
    fragParams = obj.params;
    fragExtra = obj.extra;
    fragClipRect = obj.clip_rect;
    fragWorldPos = worldPos.xyz;
    
    // Transform UV
    fragUV = inUV * obj.uv_rect.zw + obj.uv_rect.xy;
}