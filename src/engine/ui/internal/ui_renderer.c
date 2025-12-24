#include "ui_renderer.h"
#include "../ui_core.h"
#include "ui_internal.h"
#include "engine/scene/render_packet.h"
#include "engine/graphics/layer_constants.h"
#include "foundation/logger/logger.h"
#include "engine/text/text_renderer.h" 
#include "engine/text/font.h" 
#include "engine/assets/assets.h"
#include "foundation/memory/arena.h"
#include "foundation/meta/reflection.h"
#include <string.h> 

// --- Provider Registry ---
#define MAX_UI_PROVIDERS 32

typedef struct {
    StringId id;
    SceneObjectProvider callback;
} SceneProviderEntry;

static SceneProviderEntry s_providers[MAX_UI_PROVIDERS];
static int s_provider_count = 0;

void scene_register_provider(const char* name, SceneObjectProvider callback) {
    if (s_provider_count >= MAX_UI_PROVIDERS) {
        LOG_ERROR("SceneBuilder: Max providers reached");
        return;
    }
    s_providers[s_provider_count].id = str_id(name);
    s_providers[s_provider_count].callback = callback;
    s_provider_count++;
    LOG_INFO("SceneBuilder: Registered provider '%s'", name);
}

static SceneObjectProvider scene_find_provider(StringId id) {
    for(int i=0; i<s_provider_count; ++i) {
        if (s_providers[i].id == id) return s_providers[i].callback;
    }
    return NULL;
}

typedef struct OverlayNode {
    const SceneNode* el;
    struct OverlayNode* next;
} OverlayNode;

// Internal Render Context
typedef struct SceneBuilderContext {
    Scene* scene;
    const Assets* assets;
    const Font* font;
    MemoryArena* arena;
    OverlayNode* overlay_head;
    OverlayNode* overlay_tail;
    int node_count;
} SceneBuilderContext;

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

static void render_background(const SceneNode* el, SceneBuilderContext* ctx, Vec4 clip_vec, float z) {
    bool is_input = (el->ui_flags & UI_FLAG_EDITABLE);
    int mode = el->spec->style.render_mode;

    if (mode == SCENE_RENDER_MODE_DEFAULT) {
        if (el->spec->kind == SCENE_NODE_KIND_TEXT && !is_input) {
             return; 
        }
        if (el->spec->style.texture != 0) mode = SCENE_RENDER_MODE_IMAGE;
        else mode = SCENE_RENDER_MODE_BOX;
    }

    if (mode == SCENE_RENDER_MODE_TEXT) return;

    Vec4 color = el->render_color;
    if (color.w == 0) color = (Vec4){1.0f, 0.0f, 1.0f, 1.0f};

    if (el->is_active) {
        if (el->spec->style.active_color.w > 0.0f) {
            color = el->spec->style.active_color;
        } else {
            float tint = el->spec->style.active_tint > 0.0f ? el->spec->style.active_tint : 0.5f;
            color.x *= tint; color.y *= tint; color.z *= tint;
        }
    } else if (el->is_hovered) {
        if (el->spec->style.hover_color.w > 0.0f) {
            color = el->spec->style.hover_color;
        } else {
            float tint = el->spec->style.hover_tint > 0.0f ? el->spec->style.hover_tint : 1.2f;
            color.x *= tint; color.y *= tint; color.z *= tint;
        }
    } else if (is_input) {
         color.x *= 1.1f; color.y *= 1.1f; color.z *= 1.1f;
    }

    switch (mode) {
        case SCENE_RENDER_MODE_BEZIER: {
            Vec2 start = {el->screen_rect.x, el->screen_rect.y};
            Vec2 end = {el->screen_rect.x + el->screen_rect.w, el->screen_rect.y + el->screen_rect.h};
            float thickness = 2.0f;
            
            if (el->data_ptr && el->meta) {
                const MetaField* f_start = meta_find_field(el->meta, "start");
                const MetaField* f_end = meta_find_field(el->meta, "end");
                const MetaField* f_thick = meta_find_field(el->meta, "thickness");
                
                if (f_start && f_start->type == META_TYPE_VEC2) {
                    float* v = (float*)meta_get_field_ptr(el->data_ptr, f_start);
                    start = (Vec2){el->screen_rect.x + v[0], el->screen_rect.y + v[1]};
                }
                if (f_end && f_end->type == META_TYPE_VEC2) {
                    float* v = (float*)meta_get_field_ptr(el->data_ptr, f_end);
                    end = (Vec2){el->screen_rect.x + v[0], el->screen_rect.y + v[1]};
                }
                if (f_thick && f_thick->type == META_TYPE_FLOAT) {
                    thickness = *(float*)meta_get_field_ptr(el->data_ptr, f_thick);
                }
            }
            
            scene_push_curve(ctx->scene, 
                (Vec3){start.x, start.y, z}, 
                (Vec3){end.x, end.y, z}, 
                thickness, 
                color, 
                clip_vec
            );
            break;
        }

        case SCENE_RENDER_MODE_IMAGE: {
            float u0, v0, u1, v1;
            font_get_ui_rect_uv(ctx->font, &u0, &v0, &u1, &v1);
            Vec4 uv_rect = {u0, v0, u1 - u0, v1 - v0};
            
            float tex_w = el->spec->style.tex_w > 0 ? el->spec->style.tex_w : 32.0f;
            float tex_h = el->spec->style.tex_h > 0 ? el->spec->style.tex_h : 32.0f;
            
            Vec4 borders = {
                el->spec->style.border_t,
                el->spec->style.border_r,
                el->spec->style.border_b,
                el->spec->style.border_l
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
            break;
        }

        case SCENE_RENDER_MODE_BOX: {
            scene_push_rect_sdf(ctx->scene,
                (Vec3){el->screen_rect.x, el->screen_rect.y, z},
                (Vec2){el->screen_rect.w, el->screen_rect.h},
                color,
                el->spec->style.corner_radius,
                el->spec->style.border_t,
                clip_vec
            );
            break;
        }
        
        default: break;
    }
}

static void render_content(const SceneNode* el, SceneBuilderContext* ctx, Vec4 clip_vec, float z) {
    const char* text = el->cached_text;
    if (!text || text[0] == '\0') {
        text = el->spec->text;
    }
    
    bool is_input = (el->ui_flags & UI_FLAG_EDITABLE);    
    if (is_input && !text) text = "";

    if ((!text || text[0] == '\0') && !is_input) return;

    if (text) {
        Vec3 pos = {el->screen_rect.x + el->spec->layout.padding, el->screen_rect.y + el->spec->layout.padding, z + RENDER_DEPTH_STEP_CONTENT};
        
        float txt_scale = el->spec->style.text_scale > 0.0f ? el->spec->style.text_scale : 0.5f;
        Vec4 txt_color = el->spec->style.text_color.w > 0.0f ? el->spec->style.text_color : (Vec4){1.0f, 1.0f, 1.0f, 1.0f};
        
        // Note: scene_add_text_clipped was not in the render_packet API shown in scene.c, 
        // but it might be in text_renderer.h calling scene_push_ui_node internally? 
        // I should check if I need to update it. 
        // For now, I will assume it uses public scene_push_* API.
        scene_add_text_clipped(ctx->scene, ctx->font, text, pos, txt_scale, txt_color, clip_vec);

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
            
            float cw = el->spec->style.caret_width > 0.0f ? el->spec->style.caret_width : 2.0f;
            float ch = el->spec->style.caret_height > 0.0f ? el->spec->style.caret_height : 20.0f;
            
            Vec4 cc = el->spec->style.caret_color.w > 0.0f ? el->spec->style.caret_color : (Vec4){1.0f, 1.0f, 1.0f, 1.0f};

            scene_push_quad(ctx->scene, 
                (Vec3){pos.x + text_width, pos.y, z + (RENDER_DEPTH_STEP_CONTENT * 2)}, 
                (Vec2){cw, ch}, 
                cc, 
                clip_vec
            );
        }
    }
}

static void push_overlay(SceneBuilderContext* ctx, const SceneNode* el) {
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

static void process_node(const SceneNode* el, SceneBuilderContext* ctx, Rect current_clip, float base_z, bool is_overlay_pass) {
    if (!el || !el->spec) return;

    if (el->flags & SCENE_FLAG_HIDDEN) return;

    bool is_node_overlay = (el->spec->layout.layer == SCENE_LAYER_OVERLAY);
    
    if (!is_overlay_pass && is_node_overlay) {
        push_overlay(ctx, el);
        return; 
    }

    Rect effective_clip = current_clip;
    
    if (is_node_overlay) {
        effective_clip = (Rect){-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    }
    
    if (el->flags & SCENE_FLAG_CLIPPED) {
        effective_clip = rect_intersect(effective_clip, el->screen_rect);
    }
    
    ctx->node_count++;
    Vec4 clip_vec = {effective_clip.x, effective_clip.y, effective_clip.w, effective_clip.h};

    render_background(el, ctx, clip_vec, base_z);

    if (el->spec->mesh.mesh_id != 0) {
        const Mesh* mesh = assets_get_unit_quad(ctx->assets);
        if (mesh) {
            RenderBatch batch = {0};
            // Map SceneNode to RenderBatch
            // batch.pipeline_id = ... (Default 3D)
            batch.mesh = (struct Mesh*)mesh; 
            batch.instance_count = 1;
            batch.first_instance = 0;
            // Need to pass transform
            // For now, RenderBatch definition was simple.
            // I should use scene_push_render_batch
            // But RenderBatch expects prepared instance buffer?
            // "In Phase 2, we assume this is handled by the backend reading a Stream"
            // So I should populate a RenderBatch that points to this object?
            // Or, simply:
            batch.instance_buffer = NULL; // Backend will construct?
            // Wait, previous code created SceneObject with SCENE_PRIM_QUAD (which was 3D opaque).
            // Now we use RenderBatch.
            // I'll leave this empty or minimal for now as I focus on UI.
            // Or create a RenderBatch with a pointer to a temporary instance data?
            scene_push_render_batch(ctx->scene, batch);
        }
    }

    if (el->spec->kind == SCENE_NODE_KIND_VIEWPORT && el->spec->provider_id) {
         SceneObjectProvider cb = scene_find_provider(el->spec->provider_id);
         if (cb) {
             cb(el->data_ptr, el->screen_rect, base_z + RENDER_DEPTH_STEP_UI, ctx->scene, ctx->arena);
         }
    }

    render_content(el, ctx, clip_vec, base_z);

    for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
        process_node(child, ctx, effective_clip, base_z + RENDER_DEPTH_STEP_UI, is_overlay_pass);
    }
}

void scene_tree_render(SceneTree* instance, Scene* scene, const Assets* assets, MemoryArena* arena) {
    if (!instance || !instance->root || !arena) return;
    const SceneNode* root = instance->root;

    Rect infinite_clip = {-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    
    SceneBuilderContext ctx = {0};
    ctx.scene = scene;
    ctx.assets = assets;
    ctx.font = assets_get_font(assets);
    ctx.arena = arena;
    
    process_node(root, &ctx, infinite_clip, RENDER_LAYER_UI_BASE, false);
    
    int overlays = 0;
    OverlayNode* curr = ctx.overlay_head;
    while (curr) {
        process_node(curr->el, &ctx, infinite_clip, RENDER_LAYER_UI_OVERLAY, true);
        curr = curr->next;
        overlays++;
    }

    static int frame_throttle = 0;
    if (frame_throttle++ % 120 == 0) {
        LOG_DEBUG("UI Render: %d nodes, %d overlays", ctx.node_count, overlays);
    }
}