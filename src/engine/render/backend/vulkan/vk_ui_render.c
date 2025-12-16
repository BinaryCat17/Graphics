#include "vk_ui_render.h"
#include "vk_utils.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// --- Font Utils ---

static int utf8_decode(const char* s, int* out_advance) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { *out_advance = 1; return c; }
    if ((c >> 5) == 0x6) { *out_advance = 2; return ((int)(c & 0x1F) << 6) | ((int)(s[1] & 0x3F)); }
    if ((c >> 4) == 0xE) { *out_advance = 3; return ((int)(c & 0x0F) << 12) | (((int)s[1] & 0x3F) << 6) | ((int)(s[2] & 0x3F)); }
    if ((c >> 3) == 0x1E) { *out_advance = 4; return ((int)(c & 0x07) << 18) | (((int)s[1] & 0x3F) << 12) |
                                        (((int)s[2] & 0x3F) << 6) | ((int)(s[3] & 0x3F)); }
    *out_advance = 1;
    return '?';
}

static const Glyph* get_glyph(VulkanRendererState* state, int codepoint) {
    if (codepoint >= 0 && codepoint < GLYPH_CAPACITY && state->glyph_valid[codepoint]) {
        return &state->glyphs[codepoint];
    }
    if (state->glyph_valid['?']) return &state->glyphs['?'];
    return NULL;
}

// --- Vertex Building ---

static bool ensure_vtx_capacity(FrameCpuArena *cpu, size_t required) {
    if (required <= cpu->vertex_capacity) return true;
    size_t new_cap = cpu->vertex_capacity == 0 ? required : cpu->vertex_capacity * 2;
    while (new_cap < required) new_cap *= 2;
    
    Vtx *new_verts = realloc(cpu->vertices, new_cap * sizeof(Vtx));
    if (!new_verts) return false;
    
    cpu->vertices = new_verts;
    cpu->vertex_capacity = new_cap;
    return true;
}

static void emit_quad(FrameCpuArena* cpu, size_t* cursor, 
                      float x, float y, float w, float h, float z,
                      float u0, float v0, float u1, float v1,
                      Color color, float use_tex) {
    Vtx* v = &cpu->vertices[*cursor];
    
    // 0--1
    // | /|
    // |/ |
    // 2--3
    
    // Top-Left
    v[0] = (Vtx){ x, y, z, u0, v0, use_tex, color.r, color.g, color.b, color.a };
    // Top-Right
    v[1] = (Vtx){ x + w, y, z, u1, v0, use_tex, color.r, color.g, color.b, color.a };
    // Bottom-Left
    v[2] = (Vtx){ x, y + h, z, u0, v1, use_tex, color.r, color.g, color.b, color.a };
    
    // Bottom-Left
    v[3] = v[2];
    // Top-Right
    v[4] = v[1];
    // Bottom-Right
    v[5] = (Vtx){ x + w, y + h, z, u1, v1, use_tex, color.r, color.g, color.b, color.a };
    
    *cursor += 6;
}

bool vk_build_vertices_from_draw_list(VulkanRendererState* state, FrameResources *frame, const UiDrawList* draw_list) {
    if (!state || !frame || !draw_list) return false;
    
    frame->vertex_count = 0;
    if (draw_list->count == 0) return true;

    // Estimate capacity: 2 quads per command (conservative average for text/rects)
    // Actually text has many quads.
    size_t estimated_quads = draw_list->count * 4; 
    if (!ensure_vtx_capacity(&frame->cpu, estimated_quads * 6)) return false;

    size_t cursor = 0;
    
    // Z-ordering: Painter's algorithm
    // We draw back-to-front. Z should decrease to be "closer" if depth test is LEQUAL/LESS.
    // Or if we disable depth test, Z doesn't matter.
    // The current pipeline likely uses depth test.
    // Let's use a small epsilon step.
    float z_step = 1.0f / (float)(draw_list->count + 1);
    float current_z = 0.9f; 

    for (size_t i = 0; i < draw_list->count; ++i) {
        UiDrawCmd* cmd = &draw_list->commands[i];
        
        // Check capacity again (dynamic growth)
        if (cursor + 1024 > frame->cpu.vertex_capacity) { // Buffer zone
             if (!ensure_vtx_capacity(&frame->cpu, frame->cpu.vertex_capacity * 2)) break;
        }

        if (cmd->type == 0) { // Rect
            emit_quad(&frame->cpu, &cursor, 
                      cmd->rect.x, cmd->rect.y, cmd->rect.w, cmd->rect.h, current_z,
                      0, 0, 0, 0, cmd->color, 0.0f);
        } 
        else if (cmd->type == 1 && cmd->text) { // Text
            float pen_x = cmd->rect.x;
            float pen_y = cmd->rect.y + state->ascent; // Baseline approximation
            
            for (const char* p = cmd->text; *p; ) {
                int adv = 0;
                int codepoint = utf8_decode(p, &adv);
                if (adv <= 0) break;
                
                const Glyph* g = get_glyph(state, codepoint);
                if (g) {
                     float x0 = pen_x + g->xoff;
                     float y0 = pen_y + g->yoff;
                     emit_quad(&frame->cpu, &cursor,
                               x0, y0, g->w, g->h, current_z,
                               g->u0, g->v0, g->u1, g->v1,
                               cmd->color, 1.0f);
                     pen_x += g->advance;
                }
                p += adv;
            }
        }
        
        current_z -= z_step;
        if (current_z < 0.0f) current_z = 0.0f;
    }

    frame->vertex_count = cursor;
    return true;
}
