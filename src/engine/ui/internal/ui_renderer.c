#include "ui_renderer.h"
#include "../ui_core.h"
#include "ui_internal.h"
#include "engine/scene/scene.h"
#include "engine/graphics/layer_constants.h"
#include "foundation/logger/logger.h"
#include "engine/text/text_renderer.h" 
#include "engine/text/font.h" 
#include "engine/assets/assets.h"
#include "foundation/memory/arena.h"
#include <string.h> 

// --- Provider Registry ---
#define MAX_UI_PROVIDERS 32

typedef struct {
    StringId id;
    SceneObjectProvider callback;
} UiProviderEntry;

static UiProviderEntry s_providers[MAX_UI_PROVIDERS];
static int s_provider_count = 0;

void ui_register_provider(const char* name, SceneObjectProvider callback) {
    if (s_provider_count >= MAX_UI_PROVIDERS) {
        LOG_ERROR("UiRenderer: Max providers reached");
        return;
    }
    s_providers[s_provider_count].id = str_id(name);
    s_providers[s_provider_count].callback = callback;
    s_provider_count++;
    LOG_INFO("UiRenderer: Registered provider '%s'", name);
}

static SceneObjectProvider ui_find_provider(StringId id) {
    for(int i=0; i<s_provider_count; ++i) {
        if (s_providers[i].id == id) return s_providers[i].callback;
    }
    return NULL;
}

typedef struct OverlayNode {
    const UiElement* el;
    struct OverlayNode* next;
} OverlayNode;

// Internal Render Context to avoid passing too many args
typedef struct UiRenderContext {
    Scene* scene;
    const Font* font;
    MemoryArena* arena;
    OverlayNode* overlay_head;
    OverlayNode* overlay_tail;
} UiRenderContext;

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

static void render_background(const UiElement* el, UiRenderContext* ctx, Vec4 clip_vec, float z) {
    bool is_input = (el->flags & UI_FLAG_EDITABLE);
    if (el->spec->kind != UI_KIND_CONTAINER && !is_input) return;

    // Resolve base color
    Vec4 color = el->render_color;
    if (color.w == 0) color = (Vec4){0.1f, 0.1f, 0.1f, 0.8f}; // Fallback

    // Apply Hover/Active tints
    if (el->is_active) {
        if (el->spec->active_color.w > 0.0f) {
            color = el->spec->active_color;
        } else {
            float tint = el->spec->active_tint > 0.0f ? el->spec->active_tint : 0.5f;
            color.x *= tint; color.y *= tint; color.z *= tint;
        }
    } else if (el->is_hovered) {
        if (el->spec->hover_color.w > 0.0f) {
            color = el->spec->hover_color;
        } else {
            float tint = el->spec->hover_tint > 0.0f ? el->spec->hover_tint : 1.2f;
            color.x *= tint; color.y *= tint; color.z *= tint;
        }
    } else if (is_input) {
         // Default style for inputs: slightly lighter when idle
         color.x *= 1.1f; color.y *= 1.1f; color.z *= 1.1f;
    }

    if ((el->spec->texture != 0)) {
        // Use 9-Slice or Textured Quad
        float u0, v0, u1, v1;
        font_get_ui_rect_uv(ctx->font, &u0, &v0, &u1, &v1);
        Vec4 uv_rect = {u0, v0, u1 - u0, v1 - v0};
        
        float tex_w = el->spec->tex_w > 0 ? el->spec->tex_w : 32.0f;
        float tex_h = el->spec->tex_h > 0 ? el->spec->tex_h : 32.0f;
        
        // extra_params: borders (top, right, bottom, left)
        Vec4 borders = {
            el->spec->border_t,
            el->spec->border_r,
            el->spec->border_b,
            el->spec->border_l
        };
        
        scene_push_quad_9slice(ctx->scene, 
            (Vec3){el->screen_rect.x, el->screen_rect.y, z}, 
            (Vec2){el->screen_rect.w, el->screen_rect.h}, 
            color, 
            uv_rect, 
            (Vec2){tex_w, tex_h}, 
            borders, 
            clip_vec
        );
        
    } else {
        // SDF Rounded Box (Mode 4)
        scene_push_rect_sdf(ctx->scene,
            (Vec3){el->screen_rect.x, el->screen_rect.y, z},
            (Vec2){el->screen_rect.w, el->screen_rect.h},
            color,
            el->spec->corner_radius,
            el->spec->border_t,
            clip_vec
        );
    }
}

static void render_content(const UiElement* el, UiRenderContext* ctx, Vec4 clip_vec, float z) {
    // Resolve Text (Use Cache)
    const char* text = el->cached_text;
    if (!text || text[0] == '\0') {
        text = el->spec->text;
    }
    
    bool is_input = (el->flags & UI_FLAG_EDITABLE);
    if (is_input && !text) text = "";

    // Skip if nothing to draw
    if ((!text || text[0] == '\0') && !is_input) return;

    if (text) {
        Vec3 pos = {el->screen_rect.x + el->spec->padding, el->screen_rect.y + el->spec->padding, z + RENDER_DEPTH_STEP_CONTENT};
        
        float txt_scale = el->spec->text_scale > 0.0f ? el->spec->text_scale : 0.5f;
        Vec4 txt_color = el->spec->text_color.w > 0.0f ? el->spec->text_color : (Vec4){1.0f, 1.0f, 1.0f, 1.0f};
        
        scene_add_text_clipped(ctx->scene, ctx->font, text, pos, txt_scale, txt_color, clip_vec);

        // Draw Caret
        if (is_input && el->is_focused) {
            char temp[256];
            int len = (int)strlen(text);
            int c_idx = el->cursor_idx;
            if (c_idx > len) c_idx = len;
            if (c_idx < 0) c_idx = 0;
            
            int copy_len = c_idx < 255 ? c_idx : 255;
            memcpy(temp, text, copy_len);
            temp[copy_len] = '\0';
            
            float text_width = font_measure_text(ctx->font, temp) * txt_scale; 
            
            float cw = el->spec->caret_width > 0.0f ? el->spec->caret_width : 2.0f;
            float ch = el->spec->caret_height > 0.0f ? el->spec->caret_height : 20.0f;
            
            Vec4 cc = el->spec->caret_color.w > 0.0f ? el->spec->caret_color : (Vec4){1.0f, 1.0f, 1.0f, 1.0f};

            scene_push_quad(ctx->scene, 
                (Vec3){pos.x + text_width, pos.y, z + (RENDER_DEPTH_STEP_CONTENT * 2)}, 
                (Vec2){cw, ch}, 
                cc, 
                clip_vec
            );
        }
    }
}

static void push_overlay(UiRenderContext* ctx, const UiElement* el) {
    if (!ctx || !ctx->arena) return;
    OverlayNode* node = arena_alloc_zero(ctx->arena, sizeof(OverlayNode));
    if (!node) return;
    
    node->el = el;
    node->next = NULL;
    
    if (!ctx->overlay_head) {
        ctx->overlay_head = node;
        ctx->overlay_tail = node;
    } else {
        ctx->overlay_tail->next = node;
        ctx->overlay_tail = node;
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
    render_background(el, ctx, clip_vec, base_z);

    // 2. Viewport Delegation
    if (el->spec->kind == UI_KIND_VIEWPORT && el->spec->provider_id) {
         SceneObjectProvider cb = ui_find_provider(el->spec->provider_id);
         if (cb) {
             // Invoke provider (e.g. Graph Editor) to inject scene objects
             // Z-Depth: slightly above background
             cb(el->data_ptr, el->screen_rect, base_z + RENDER_DEPTH_STEP_UI, ctx->scene, ctx->arena);
         }
    }

    // 3. Draw Content
    render_content(el, ctx, clip_vec, base_z);

    // 4. Recurse
    for (UiElement* child = el->first_child; child; child = child->next_sibling) {
        process_node(child, ctx, effective_clip, base_z + RENDER_DEPTH_STEP_UI, is_overlay_pass);
    }
}

void ui_renderer_build_scene(const UiElement* root, Scene* scene, const Assets* assets, MemoryArena* arena) {
    if (!root || !arena) return;

    Rect infinite_clip = {-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    
    UiRenderContext ctx = {0};
    ctx.scene = scene;
    ctx.font = assets_get_font(assets);
    ctx.arena = arena;
    
    // Pass 1: Draw Normal, Defer Overlays
    process_node(root, &ctx, infinite_clip, RENDER_LAYER_UI_BASE, false);
    
    // Pass 2: Draw Overlays
    OverlayNode* curr = ctx.overlay_head;
    while (curr) {
        process_node(curr->el, &ctx, infinite_clip, RENDER_LAYER_UI_OVERLAY, true);
        curr = curr->next;
    }
}
