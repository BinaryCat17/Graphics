#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple Circle
    vec2 c = inUV - 0.5;
    float dist = length(c);
    float alpha = 1.0 - smoothstep(0.45, 0.5, dist);
    
    outColor = vec4(inColor.rgb, inColor.a * alpha);
    if (outColor.a < 0.01) discard;
}
