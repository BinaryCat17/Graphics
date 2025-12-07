#version 450
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in float in_use_tex;
layout(location = 3) in vec4 in_col;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out float out_use_tex;
layout(location = 2) out vec4 out_col;

layout(push_constant) uniform Push { vec2 viewport; } pc;

void main() {
    vec2 ndc = vec2((in_pos.x / pc.viewport.x) * 2.0 - 1.0, -((in_pos.y / pc.viewport.y) * 2.0 - 1.0));
    gl_Position = vec4(ndc, 0.0, 1.0);
    out_uv = in_uv;
    out_use_tex = in_use_tex;
    out_col = in_col;
}
