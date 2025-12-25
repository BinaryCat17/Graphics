#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inParams;     // x=mode, y=radius, z=border, w=extra
layout(location = 3) in vec4 inExtra;      // Borders for 9-slice or other params
layout(location = 4) in flat vec4 inClipRect;   // Scissor/Clip rect (x,y,w,h)
layout(location = 5) in vec3 inWorldPos;
layout(location = 6) in vec2 inOrigUV;     // 0..1 local UV
layout(location = 7) in vec4 inUVRect;     // Atlas UV rect (u,v,w,h)
layout(location = 8) in vec2 inTargetSize; // Size of the quad in pixels

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

// SDF Functions
float sdRoundedBox(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x  : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float map_9slice(float uv, float size, float b_start, float b_end, float tex_size) {
    // Pixel coordinates
    float px = uv * size;
    
    // Borders in texture space (pixels)
    float t_start = b_start;
    float t_end = tex_size - b_end;
    
    // Middle size
    float size_mid = size - b_start - b_end;
    float tex_mid = t_end - t_start;
    
    float res = 0.0;
    
    if (px < b_start) {
        res = px;
    } else if (px > size - b_end) {
        res = t_end + (px - (size - b_end));
    } else {
        // Map middle
        float t = (px - b_start) / size_mid;
        res = t_start + t * tex_mid;
    }
    
    return res / tex_size;
}

void main() {
    // 1. Clipping
    if (gl_FragCoord.x < inClipRect.x || gl_FragCoord.x > inClipRect.x + inClipRect.z ||
        gl_FragCoord.y < inClipRect.y || gl_FragCoord.y > inClipRect.y + inClipRect.w) {
        discard;
    }

    vec4 color = inColor;
    float alpha = 1.0;
    vec2 uv = inUV;

    if (inParams.x < 0.5) {
        // 0.0: Solid Color
        // No Texture
    } else if (inParams.x < 1.5) {
        // 1.0: Textured (Font or Icon)
        float dist = texture(texSampler, uv).r;
        alpha = dist;
    } else if (inParams.x < 2.5) {
        // 2.0: User Texture (Image)
        vec4 texColor = texture(texSampler, uv);
        color = texColor; 
        if (inColor.a < 1.0) {
             color.rgb *= inColor.rgb;
             alpha = texColor.a * inColor.a;
        }
    } else if (inParams.x < 3.5) {
         // 3.0: 9-Slice Quad
         float u9 = map_9slice(inOrigUV.x, inTargetSize.x, inExtra.x, inExtra.z, inParams.z);
         float v9 = map_9slice(inOrigUV.y, inTargetSize.y, inExtra.y, inExtra.w, inParams.w);
         
         uv = vec2(u9, v9) * inUVRect.zw + inUVRect.xy;
         
         float mask = texture(texSampler, uv).r;
         alpha *= mask;
    } else if (inParams.x < 4.5) {
         // 4.0: SDF Rounded Box
         vec2 size = inTargetSize; // W, H
         vec2 p = (inOrigUV - 0.5) * size; // Center at 0,0
         vec2 halfSize = size * 0.5;
         float radius = inParams.y;
         float border = inParams.z;
         
         radius = min(radius, min(halfSize.x, halfSize.y));
         
         float dist = sdRoundedBox(p, halfSize, vec4(radius));
         float softness = 1.0; 
         
         // Fill
         float fillAlpha = 1.0 - smoothstep(0.0, softness, dist);
         
         // Border
         if (border > 0.0) {
             float insideBorder = smoothstep(-border - softness, -border, dist);
             color.rgb = mix(color.rgb * 0.8, color.rgb, 1.0 - insideBorder);
         }

         alpha *= fillAlpha;
    } 
    else if (inParams.y > 0.5) { 
        // Curve
        alpha = 1.0; 
    }

    outColor = vec4(color.rgb, color.a * alpha);
}