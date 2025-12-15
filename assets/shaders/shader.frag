#version 450
layout(location = 0) in vec2 in_uv;
layout(location = 1) in float in_use_tex;
layout(location = 2) in vec4 in_col;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D font_tex;

void main() {
    if (in_use_tex > 0.5) {
        float alpha = texture(font_tex, in_uv).r;
        out_color = vec4(in_col.rgb, in_col.a * alpha);
    } else {
        out_color = in_col;
    }
}
