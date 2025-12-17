#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inParams; // x = use_texture

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    float alpha = inColor.a;
    if (inParams.x > 0.5) {
        float texAlpha = texture(texSampler, inUV).r;
        alpha *= texAlpha;
    }
    outColor = vec4(inColor.rgb, alpha);
}
