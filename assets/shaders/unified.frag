#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inParams; // x=use_tex, y=prim_type
layout(location = 3) in vec4 inExtra;  // xy=start_uv, zw=end_uv

layout(set = 0, binding = 0) uniform sampler2D texSampler; // Font/Atlas
layout(set = 2, binding = 0) uniform sampler2D userTex;    // Compute/User

layout(location = 0) out vec4 outColor;

// --- SDF Utilities ---

float sdSegment(in vec2 p, in vec2 a, in vec2 b) {
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

// Approximate distance to cubic bezier (Adaptive)
// We simplify by just evaluating distance to a few segments for now (Performance/Stability)
// A proper SDF is complex. Let's use a specialized "Wire" function.
float sdWire(vec2 p, vec2 a, vec2 b) {
    vec2 c1 = a + vec2((b.x - a.x) * 0.5, 0.0);
    vec2 c2 = b - vec2((b.x - a.x) * 0.5, 0.0);
    
    // Evaluate Bezier at t=0.25, 0.5, 0.75 for segment approximation
    // B(t) = (1-t)^3 A + 3(1-t)^2 t C1 + 3(1-t)t^2 C2 + t^3 B
    
    vec2 pts[5];
    pts[0] = a;
    pts[4] = b;
    
    // t=0.5
    vec2 m = 0.125*a + 0.375*c1 + 0.375*c2 + 0.125*b;
    pts[2] = m;
    
    // t=0.25
    // coefficients for t=0.25: (0.75)^3, 3*(0.75)^2*0.25, ...
    // approx to simplify
    pts[1] = mix(a, m, 0.5); // Simple linear mid for now to verify pipeline
    pts[3] = mix(m, b, 0.5);
    
    float d = 1e5;
    d = min(d, sdSegment(p, pts[0], pts[1]));
    d = min(d, sdSegment(p, pts[1], pts[2]));
    d = min(d, sdSegment(p, pts[2], pts[3]));
    d = min(d, sdSegment(p, pts[3], pts[4]));
    
    return d;
}

void main() {
    // 1. Texture/Color Base
    vec4 color = inColor;
    float alpha = color.a;
    
    if (inParams.x > 0.5) { 
        if (inParams.x < 1.5) {
             // Textured Quad (Font) - Param 1.0
             float texAlpha = texture(texSampler, inUV).r;
             alpha *= texAlpha;
        } else {
             // User Texture (Compute) - Param 2.0
             // Sample fully
             vec4 texColor = texture(userTex, inUV);
             color = texColor; // Replace color with texture
             alpha = texColor.a;
        }
    } 
    else if (inParams.y > 0.5) { // Curve (PrimType == 1)
        // SDF Rendering
        // inUV is 0..1 covering the bounding box
        // inExtra.xy = Start Point in UV space
        // inExtra.zw = End Point in UV space
        
        float dist = sdWire(inUV, inExtra.xy, inExtra.zw);
        
        // Add round caps (circles at endpoints)
        dist = min(dist, length(inUV - inExtra.xy));
        dist = min(dist, length(inUV - inExtra.zw));
        
        // Anti-aliased stroke
        // Thickness = 0.01 (in UV space) - depends on scale. 
        // Ideally pass thickness in params.z. Let's assume params.z
        float thickness = (inParams.z > 0.0) ? inParams.z : 0.01;
        float aa = 0.005; 
        
        alpha = 1.0 - smoothstep(thickness - aa, thickness + aa, dist);
        
        if (alpha < 0.01) discard;
    }

    outColor = vec4(color.rgb, alpha);
}