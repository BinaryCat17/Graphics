#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 model;
    mat4 view_proj;
    vec4 color;
    vec4 params; // x = use_texture
} pc;

void main() {
    float alpha = inColor.a;
    if (pc.params.x > 0.5) {
        float texAlpha = texture(texSampler, inUV).r;
        alpha *= texAlpha;
    }
    outColor = vec4(inColor.rgb, alpha);
}
