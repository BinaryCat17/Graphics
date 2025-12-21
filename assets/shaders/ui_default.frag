#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inParams; // x=use_tex, y=prim_type
layout(location = 3) in vec4 inExtra;  // xy=start_uv, zw=end_uv
layout(location = 4) in flat vec4 inClipRect; // x,y,w,h
layout(location = 5) in vec3 inWorldPos;
layout(location = 6) in vec2 inOrigUV;
layout(location = 7) in vec4 inUVRect;
layout(location = 8) in vec2 inTargetSize;

layout(set = 0, binding = 0) uniform sampler2D texSampler; // Font/Atlas
layout(set = 2, binding = 0) uniform sampler2D userTex;    // Compute/User

layout(location = 0) out vec4 outColor;

// --- 9-Slice Helper ---
float map_9slice(float t, float target_size, float b1, float b2, float tex_size) {
    float px = t * target_size;
    if (px < b1) {
        return (px / b1) * (b1 / tex_size);
    } else if (px > target_size - b2) {
        return 1.0 - ((target_size - px) / b2) * (b2 / tex_size);
    } else {
        float middle_t = (px - b1) / (target_size - b1 - b2);
        return (b1 / tex_size) + middle_t * ((tex_size - b1 - b2) / tex_size);
    }
}

// --- SDF Utilities ---

float sdSegment(in vec2 p, in vec2 a, in vec2 b) {
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

vec2 cubicBezier(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
    float u = 1.0 - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    return uuu * p0 + 3.0 * uu * t * p1 + 3.0 * u * tt * p2 + ttt * p3;
}

float sdWire(vec2 p, vec2 a, vec2 b) {
    vec2 c1 = a + vec2((b.x - a.x) * 0.5, 0.0);
    vec2 c2 = b - vec2((b.x - a.x) * 0.5, 0.0);
    
    float d = 1e5;
    vec2 prev = a;
    const int STEPS = 16;
    for (int i = 1; i <= STEPS; ++i) {
        float t = float(i) / float(STEPS);
        vec2 curr = cubicBezier(a, c1, c2, b, t);
        d = min(d, sdSegment(p, prev, curr));
        prev = curr;
    }
    
    return d;
}

float sdRoundedBox( in vec2 p, in vec2 b, in vec4 r )
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

void main() {
    // 0. Clipping
    if (inClipRect.z > 0.0 && inClipRect.w > 0.0) {
        if (inWorldPos.x < inClipRect.x || 
            inWorldPos.x > (inClipRect.x + inClipRect.z) ||
            inWorldPos.y < inClipRect.y || 
            inWorldPos.y > (inClipRect.y + inClipRect.w)) {
            discard;
        }
    }

    // 1. Texture/Color Base
    vec4 color = inColor;
    float alpha = color.a;
    
    vec2 uv = inUV;

    if (inParams.x > 0.5) { 
        if (inParams.x < 1.5) {
             // 1.0: Textured Quad (Font)
             float texAlpha = texture(texSampler, inUV).r;
             alpha *= texAlpha;
        } else if (inParams.x < 2.5) {
             // 2.0: User Texture (Compute)
             vec4 texColor = texture(userTex, inUV);
             color = texColor * inColor; 
             alpha = texColor.a * inColor.a;
        } else if (inParams.x < 3.5) {
             // 3.0: 9-Slice Quad
             // Layout:
             // inParams.z = tex_w (px)
             // inParams.w = tex_h (px)
             // inExtra.x = border_l (px)
             // inExtra.y = border_t (px)
             // inExtra.z = border_r (px)
             // inExtra.w = border_b (px)
             // inTargetSize = from vertex shader (px)
             
             float u9 = map_9slice(inOrigUV.x, inTargetSize.x, inExtra.x, inExtra.z, inParams.z);
             float v9 = map_9slice(inOrigUV.y, inTargetSize.y, inExtra.y, inExtra.w, inParams.w);
             
             uv = vec2(u9, v9) * inUVRect.zw + inUVRect.xy;
             
             float mask = texture(texSampler, uv).r;
             alpha *= mask;
        } else if (inParams.x < 4.5) {
             // 4.0: SDF Rounded Box
             // inParams.y = radius (px)
             // inParams.z = border_thickness (px)
             
             vec2 size = inTargetSize; // W, H
             vec2 p = (inOrigUV - 0.5) * size; // Center at 0,0
             vec2 halfSize = size * 0.5;
             float radius = inParams.y;
             float border = inParams.z;
             
             // Clamp radius to half-size to avoid artifacts
             radius = min(radius, min(halfSize.x, halfSize.y));
             
             float dist = sdRoundedBox(p, halfSize, vec4(radius));
             
             // Softness (AA)
             float softness = 1.0; 
             
             // Fill
             float fillAlpha = 1.0 - smoothstep(0.0, softness, dist);
             
             // Border
             if (border > 0.0) {
                 // Distance to inner edge is dist + border? No, dist is 0 at edge.
                 // We want the stroke to be *inside* the shape usually, or centered?
                 // Let's assume border is *inset*.
                 // Inner edge is at dist = -border.
                 
                 float borderAlpha = 1.0 - smoothstep(border - softness, border, abs(dist + border * 0.5));
                 // Wait, simpler: 
                 // We want to render if dist <= 0.
                 // If dist > -border, it's border color.
                 // If dist < -border, it's fill color.
                 
                 // Let's just do a simple mix for now.
                 // For now, let's assume the color passed IS the background color. 
                 // If we want a separate border color, we need more inputs.
                 // But typically borders are darker/lighter or specified separately.
                 // As a hack for "Visual Polish", let's darken the border automatically if no color is provided.
                 
                 float insideBorder = smoothstep(-border - softness, -border, dist);
                 // 0 = inside core, 1 = inside border area
                 
                 color.rgb = mix(color.rgb * 0.8, color.rgb, 1.0 - insideBorder);
             }

             alpha *= fillAlpha;
        }
    } 
    else if (inParams.y > 0.5) { // Curve (PrimType == 1)
        // Aspect Ratio Correction
        float ar = (inParams.w > 0.0) ? inParams.w : 1.0;
        vec2 scale = vec2(ar, 1.0);
        
        vec2 p = inUV * scale;
        vec2 a = inExtra.xy * scale;
        vec2 b = inExtra.zw * scale;
        
        float dist = sdWire(p, a, b);
        
        // Anti-aliased stroke
        float thickness = (inParams.z > 0.0) ? inParams.z : 0.01;
        float aa = 0.005; 

        dist = min(dist, length(p - a) - thickness);
        dist = min(dist, length(p - b) - thickness);
        
        alpha = 1.0 - smoothstep(thickness - aa, thickness + aa, dist);
        
        if (alpha < 0.01) discard;
    }

    outColor = vec4(color.rgb, alpha);
}