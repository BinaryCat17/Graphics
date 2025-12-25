#version 450

struct Vertex {
    vec3 pos;
    vec4 color;
    vec2 uv;
};

layout(std430, set = 1, binding = 0) readonly buffer Verts {
    Vertex data[];
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

layout(push_constant) uniform Push {
    mat4 view_proj;
};

void main() {
    Vertex v = data[gl_VertexIndex];
    gl_Position = view_proj * vec4(v.pos, 1.0);
    outColor = v.color;
    outUV = v.uv;
}