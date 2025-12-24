#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main() {
    // Simple rounded box or just box
    vec2 uv = inUV;
    
    // Border
    float border = 0.02; // relative UV
    if (uv.x < border || uv.x > 1.0 - border || uv.y < border || uv.y > 1.0 - border) {
        outColor = vec4(0.8, 0.8, 0.8, 1.0); // Border color
    } else if (uv.y < 0.2) { // Header
        outColor = vec4(0.3, 0.3, 0.35, 1.0);
    } else {
        outColor = vec4(0.2, 0.2, 0.2, 1.0); // Body
    }
}
