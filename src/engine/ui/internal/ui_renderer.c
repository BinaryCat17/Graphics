#include "ui_renderer.h"
#include "../ui_core.h"
#include "engine/scene/scene.h"
#include "foundation/logger/logger.h"
#include "engine/text/text_renderer.h" 
#include "engine/text/font.h" 
#include "foundation/memory/arena.h"
#include <string.h> 
// #include <stdlib.h> // Malloc removed

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

static void render_background(const UiElement* el, Scene* scene, Vec4 clip_vec, float z) {
    if (el->spec->kind != UI_KIND_CONTAINER && el->spec->kind != UI_KIND_TEXT_INPUT) return;

    SceneObject quad = {0};
    quad.prim_type = SCENE_PRIM_QUAD; 
    quad.position = (Vec3){el->screen_rect.x, el->screen_rect.y, z}; 
    quad.scale = (Vec3){el->screen_rect.w, el->screen_rect.h, 1.0f};
    quad.clip_rect = clip_vec; 
    
    // Use animated color
    quad.color = el->render_color;
    if (quad.color.w == 0) quad.color = (Vec4){0.1f, 0.1f, 0.1f, 0.8f}; // Fallback

    // Hover/Active tints
    if (el->is_active) {
        quad.color.x *= 0.5f; quad.color.y *= 0.5f; quad.color.z *= 0.5f;
    } else if (el->is_hovered && (el->spec->hover_color.x == 0 && el->spec->hover_color.y == 0 && el->spec->hover_color.z == 0 && el->spec->hover_color.w == 0)) {
        // Only apply legacy tint if no declarative hover color is set
        quad.color.x *= 1.2f; quad.color.y *= 1.2f; quad.color.z *= 1.2f;
    } else if (el->spec->kind == UI_KIND_TEXT_INPUT) {
         // Make inputs slightly lighter by default
         quad.color.x *= 1.1f; quad.color.y *= 1.1f; quad.color.z *= 1.1f;
    }

    if ((el->spec->texture_path)) {
        // Use 9-Slice or Textured Quad
        quad.shader_params_0.x = 3.0f; // 9-Slice (UI Shader Mode)
        
        float u0, v0, u1, v1;
        font_get_ui_rect_uv(&u0, &v0, &u1, &v1);
        quad.uv_rect = (Vec4){u0, v0, u1 - u0, v1 - v0};
        
        // Pass 9-slice data
        // shader_params_0: x=type, y=unused, z=width, w=height
        quad.shader_params_0.z = el->spec->tex_w > 0 ? el->spec->tex_w : 32.0f;
        quad.shader_params_0.w = el->spec->tex_h > 0 ? el->spec->tex_h : 32.0f;
        
        // shader_params_1: borders (top, right, bottom, left)
        quad.shader_params_1.x = el->spec->border_t;
        quad.shader_params_1.y = el->spec->border_r;
        quad.shader_params_1.z = el->spec->border_b;
        quad.shader_params_1.w = el->spec->border_l;
        
    } else {
        // SDF Rounded Box (Mode 4)
        quad.shader_params_0.x = 4.0f; // Mode 4: SDF Rect
        quad.shader_params_0.y = el->spec->corner_radius; // Radius
        
        // Pass borders for SDF stroke
        quad.shader_params_0.z = el->spec->border_t; 

        float u, v;
        font_get_white_pixel_uv(&u, &v);
        quad.uv_rect = (Vec4){u, v, 0.001f, 0.001f}; 
    }
    
    scene_add_object(scene, quad);
}

static void render_content(const UiElement* el, Scene* scene, Vec4 clip_vec, float z) {
    // Resolve Text (Use Cache)
    const char* text = el->cached_text;
    if (!text || text[0] == '\0') {
        text = el->spec->static_text;
    }
    
    if (el->spec->kind == UI_KIND_TEXT_INPUT && !text) text = "";

    // Skip if nothing to draw
    if ((!text || text[0] == '\0') && el->spec->kind != UI_KIND_TEXT_INPUT) return;

    if (text) {
        Vec3 pos = {el->screen_rect.x + el->spec->padding, el->screen_rect.y + el->spec->padding, z + 0.001f};
        
        scene_add_text_clipped(scene, text, pos, 0.5f, (Vec4){1.0f, 1.0f, 1.0f, 1.0f}, clip_vec);

        // Draw Caret
        if (el->spec->kind == UI_KIND_TEXT_INPUT && el->is_focused) {
            char temp[256];
            int len = (int)strlen(text);
            int c_idx = el->cursor_idx;
            if (c_idx > len) c_idx = len;
            if (c_idx < 0) c_idx = 0;
            
            int copy_len = c_idx < 255 ? c_idx : 255;
            memcpy(temp, text, copy_len);
            temp[copy_len] = '\0';
            
            float text_width = font_measure_text(temp) * 0.5f; 
            
            SceneObject caret = {0};
            caret.prim_type = SCENE_PRIM_QUAD;
            caret.position = (Vec3){pos.x + text_width, pos.y, z + 0.002f}; 
            caret.scale = (Vec3){2.0f, 20.0f, 1.0f}; 
            caret.clip_rect = clip_vec;
            caret.color = (Vec4){1.0f, 1.0f, 1.0f, 1.0f}; 
            
            caret.shader_params_0.x = 0.0f;
            float u, v;
            font_get_white_pixel_uv(&u, &v);
            caret.uv_rect = (Vec4){u, v, 0.001f, 0.001f};

            scene_add_object(scene, caret);
        }
    }
}

// Internal Render Context to avoid passing too many args
typedef struct UiRenderContext {
    Scene* scene;
} UiRenderContext;

// Persistent scratch arena for frame-local data (Overlay List)
static MemoryArena g_ui_renderer_arena = {0};
static bool g_ui_renderer_arena_init = false;

typedef struct OverlayNode {
    const UiElement* el;
    struct OverlayNode* next;
} OverlayNode;

static OverlayNode* g_overlay_head = NULL;
static OverlayNode* g_overlay_tail = NULL;

static void push_overlay(UiRenderContext* ctx, const UiElement* el) {
    (void)ctx;
    OverlayNode* node = arena_alloc_zero(&g_ui_renderer_arena, sizeof(OverlayNode));
    if (!node) return;
    
    node->el = el;
    node->next = NULL;
    
    if (!g_overlay_head) {
        g_overlay_head = node;
        g_overlay_tail = node;
    } else {
        g_overlay_tail->next = node;
        g_overlay_tail = node;
    }
}

static void process_node(const UiElement* el, UiRenderContext* ctx, Rect current_clip, float base_z, bool is_overlay_pass) {
    if (!el || !el->spec) return;

    // Skip hidden
    if (el->flags & UI_FLAG_HIDDEN) return;

    // Check Overlay Logic
    bool is_node_overlay = (el->spec->layer == UI_LAYER_OVERLAY);
    
    // If we are in the Normal pass, and encounter an Overlay node -> Defer it
    if (!is_overlay_pass && is_node_overlay) {
        push_overlay(ctx, el);
        return; 
    }

    // Determine Clip
    Rect effective_clip = current_clip;
    
    // If this node IS an overlay root (and we are likely in overlay pass or processing it), reset clip
    // Note: If we are recursively inside an overlay, we respect the parent clip, 
    // but the root of the overlay resets it.
    if (is_node_overlay) {
        effective_clip = (Rect){-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    }
    
    // Apply Standard Clipping
    if (el->flags & UI_FLAG_CLIPPED) {
        effective_clip = rect_intersect(effective_clip, el->screen_rect);
    }
    
    Vec4 clip_vec = {effective_clip.x, effective_clip.y, effective_clip.w, effective_clip.h};

    // 1. Draw Background
    render_background(el, ctx->scene, clip_vec, base_z);

    // 2. Draw Content
    render_content(el, ctx->scene, clip_vec, base_z);

    // 3. Recurse
    for (UiElement* child = el->first_child; child; child = child->next_sibling) {
        process_node(child, ctx, effective_clip, base_z, is_overlay_pass);
    }
}

void ui_renderer_build_scene(const UiElement* root, Scene* scene, const Assets* assets) {
    (void)assets;
    if (!root) return;

    // Lazy Init Arena
    if (!g_ui_renderer_arena_init) {
        // 1MB scratch for overlay pointers is plenty
        if (arena_init(&g_ui_renderer_arena, 1024 * 1024)) {
            g_ui_renderer_arena_init = true;
        } else {
            LOG_ERROR("UiRenderer: Failed to init scratch arena");
            return;
        }
    }
    
    // Reset per frame
    arena_reset(&g_ui_renderer_arena);
    g_overlay_head = NULL;
    g_overlay_tail = NULL;
    
    Rect infinite_clip = {-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    
    UiRenderContext ctx = {0};
    ctx.scene = scene;
    
    // Pass 1: Draw Normal, Defer Overlays
    process_node(root, &ctx, infinite_clip, 0.0f, false);
    
    // Pass 2: Draw Overlays
    OverlayNode* curr = g_overlay_head;
    while (curr) {
        process_node(curr->el, &ctx, infinite_clip, 0.8f, true);
        curr = curr->next;
    }
}
