#include "ui_renderer.h"
#include "ui_core.h"
#include "engine/graphics/scene/scene.h"
#include "foundation/logger/logger.h"
#include "engine/graphics/text/text_renderer.h" 

// Helper: Intersection
static Rect rect_intersect(Rect a, Rect b) {
    float x1 = (a.x > b.x) ? a.x : b.x;
    float y1 = (a.y > b.y) ? a.y : b.y;
    float x2 = (a.x + a.w < b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    float y2 = (a.y + a.h < b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    
    if (x2 < x1) x2 = x1;
    if (y2 < y1) y2 = y1;
    
    return (Rect){x1, y1, x2 - x1, y2 - y1};
}

static void process_node(const UiElement* el, Scene* scene, const Assets* assets, Rect current_clip) {
    if (!el || !el->spec) return;

    // Skip hidden
    if (el->spec->flags & UI_FLAG_HIDDEN) return;

    // Determine Logic Clip (Effective Clip)
    // If this node clips, it constrains the passed down clip.
    Rect effective_clip = current_clip;
    if (el->spec->flags & UI_FLAG_CLIPPED) {
        effective_clip = rect_intersect(current_clip, el->screen_rect);
    }
    
    // Prepare clip vec for shader (x,y,w,h)
    // If w/h are massive, shader treats as no-op or we use flag.
    // Let's rely on coordinate logic.
    Vec4 clip_vec = {effective_clip.x, effective_clip.y, effective_clip.w, effective_clip.h};

    // 1. Draw Background (Container)
    if (el->spec->kind == UI_KIND_CONTAINER) {
        SceneObject quad = {0};
        quad.prim_type = SCENE_PRIM_QUAD; 
        quad.position = (Vec3){el->screen_rect.x, el->screen_rect.y, 0.0f}; 
        quad.scale = (Vec3){el->screen_rect.w, el->screen_rect.h, 1.0f};
        quad.uv_rect = (Vec4){0, 0, 1, 1}; 
        quad.clip_rect = clip_vec; 
        
        // Colors
        if (el->spec->flags & UI_FLAG_CLICKABLE) {
             quad.color = (Vec4){0.3f, 0.3f, 0.3f, 1.0f}; 
        } else {
             quad.color = (Vec4){0.1f, 0.1f, 0.1f, 0.8f}; 
        }
        
        scene_add_object(scene, quad);
    }

    // 2. Draw Content (Text)
    if (el->spec->static_text || el->cached_text) {
        const char* text = el->cached_text ? el->cached_text : el->spec->static_text;
        if (text) {
            Vec3 pos = {el->screen_rect.x + el->spec->padding, el->screen_rect.y + el->spec->padding, 0.1f};
            
            // Pass the effective clip to the text renderer
            Vec4 clip_v = {effective_clip.x, effective_clip.y, effective_clip.w, effective_clip.h};
            scene_add_text_clipped(scene, text, pos, 0.5f, (Vec4){1.0f, 1.0f, 1.0f, 1.0f}, clip_v);
        }
    }

    // 3. Recurse
    for (size_t i = 0; i < el->child_count; ++i) {
        process_node(el->children[i], scene, assets, effective_clip);
    }
}

void ui_renderer_build_scene(const UiElement* root, Scene* scene, const Assets* assets) {
    if (!root) return;
    
    // Default "Infinite" Clip
    Rect infinite_clip = {-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    process_node(root, scene, assets, infinite_clip);
}