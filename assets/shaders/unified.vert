#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform Push {
    mat4 model;
    mat4 view_proj;
    vec4 color; 
    vec4 uv_rect; // xy=offset, zw=scale
    vec4 params; // x = use_texture (1.0 = yes)
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    gl_Position = pc.view_proj * pc.model * vec4(inPosition, 1.0);
    fragColor = pc.color;
    // Transform UV: scale first, then offset
    fragUV = inUV * pc.uv_rect.zw + pc.uv_rect.xy;
}
