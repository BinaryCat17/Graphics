#include "services/ui/layout_tree.h"
#include "core/platform/platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_truetype.h"

static unsigned char* g_font_buffer = NULL;
static stbtt_fontinfo g_font_info;
static float g_font_scale = 0.0f;
static int g_font_ascent = 0;
static int g_font_descent = 0;
static int g_font_ready = 0;

static float fallback_line_height(void) {
    float line = (float)(g_font_ascent - g_font_descent);
    return line > 0.0f ? line : 18.0f;
}

static int utf8_decode(const char* s, int* out_advance) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { *out_advance = 1; return c; }
    if ((c >> 5) == 0x6) { *out_advance = 2; return ((int)(c & 0x1F) << 6) | ((int)(s[1] & 0x3F)); }
    if ((c >> 4) == 0xE) { *out_advance = 3; return ((int)(c & 0x0F) << 12) | (((int)s[1] & 0x3F) << 6) | ((int)(s[2] & 0x3F)); }
    if ((c >> 3) == 0x1E) { *out_advance = 4; return ((int)(c & 0x07) << 18) | (((int)s[1] & 0x3F) << 12) | (((int)s[2] & 0x3F) << 6) | ((int)(s[3] & 0x3F)); }
    *out_advance = 1;
    return '?';
}

static void measure_text(const char* text, float* out_w, float* out_h) {
    float width = 0.0f;
    float height = fallback_line_height();

    if (g_font_ready && text && *text) {
        int prev = 0;
        for (const char* c = text; *c; ) {
            int adv = 0;
            int ch = utf8_decode(c, &adv);
            if (adv <= 0) break;
            int advance = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&g_font_info, ch, &advance, &lsb);
            width += advance * g_font_scale;
            if (prev) width += stbtt_GetCodepointKernAdvance(&g_font_info, prev, ch) * g_font_scale;
            prev = ch;
            c += adv;
        }
    }

    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
}

static void ensure_font_metrics(const char* font_path) {
    if (g_font_ready || !font_path) return;

    FILE* f = platform_fopen(font_path, "rb");
    if (!f) {
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }

    g_font_buffer = (unsigned char*)malloc((size_t)sz);
    if (!g_font_buffer) { fclose(f); return; }
    fread(g_font_buffer, 1, (size_t)sz, f);
    fclose(f);

    if (!stbtt_InitFont(&g_font_info, g_font_buffer, 0)) {
        free(g_font_buffer);
        g_font_buffer = NULL;
        return;
    }

    g_font_scale = stbtt_ScaleForPixelHeight(&g_font_info, 32.0f);
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&g_font_info, &ascent, &descent, &gap);
    g_font_ascent = (int)roundf((float)ascent * g_font_scale);
    g_font_descent = (int)roundf((float)descent * g_font_scale);
    g_font_ready = 1;
}

static LayoutNode* build_layout_tree_recursive(const UiNode* node) {
    if (!node) return NULL;
    LayoutNode* l = (LayoutNode*)calloc(1, sizeof(LayoutNode));
    if (!l) return NULL;
    l->source = node;
    l->child_count = node->child_count;
    if (node->child_count > 0) {
        l->children = (LayoutNode*)calloc(node->child_count, sizeof(LayoutNode));
        if (!l->children) { free(l); return NULL; }
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* child = build_layout_tree_recursive(&node->children[i]);
            if (child) {
                l->children[i] = *child;
                free(child);
            }
        }
    }
    return l;
}

LayoutNode* build_layout_tree(const UiNode* root) {
    return build_layout_tree_recursive(root);
}

static void free_layout_node(LayoutNode* root) {
    if (!root) return;
    for (size_t i = 0; i < root->child_count; i++) {
        free_layout_node(&root->children[i]);
    }
    free(root->children);
}

void free_layout_tree(LayoutNode* root) {
    if (!root) return;
    free_layout_node(root);
    free(root);
}

static void measure_node(LayoutNode* node) {
    if (!node || !node->source) return;
    const Style* default_style = ui_default_style();
    float padding = node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : default_style->padding);
    float border = node->source->border_thickness;
    for (size_t i = 0; i < node->child_count; i++) measure_node(&node->children[i]);

    if (node->source->layout == UI_LAYOUT_ROW) {
        float content_w = 0.0f;
        float content_h = 0.0f;
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* ch = &node->children[i];
            content_w += ch->rect.w;
            if (i + 1 < node->child_count) content_w += node->source->spacing;
            if (ch->rect.h > content_h) content_h = ch->rect.h;
        }
        node->rect.w = content_w + padding * 2.0f + border * 2.0f;
        node->rect.h = content_h + padding * 2.0f + border * 2.0f;
        if (node->source->has_max_w && node->rect.w > node->source->max_w) node->rect.w = node->source->max_w;
    } else if (node->source->layout == UI_LAYOUT_COLUMN) {
        float content_w = 0.0f;
        float content_h = 0.0f;
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* ch = &node->children[i];
            if (ch->rect.w > content_w) content_w = ch->rect.w;
            content_h += ch->rect.h;
            if (i + 1 < node->child_count) content_h += node->source->spacing;
        }
        node->rect.w = content_w + padding * 2.0f + border * 2.0f;
        node->rect.h = content_h + padding * 2.0f + border * 2.0f;
        if (node->source->has_max_h && node->rect.h > node->source->max_h) node->rect.h = node->source->max_h;
    } else if (node->source->layout == UI_LAYOUT_TABLE && node->source->columns > 0) {
        int cols = node->source->columns;
        int rows = (int)((node->child_count + (size_t)cols - 1) / (size_t)cols);
        float* col_w = (float*)calloc((size_t)cols, sizeof(float));
        float* row_h = (float*)calloc((size_t)rows, sizeof(float));
        if (col_w && row_h) {
            for (size_t i = 0; i < node->child_count; i++) {
                int col = (int)(i % (size_t)cols);
                int row = (int)(i / (size_t)cols);
                LayoutNode* ch = &node->children[i];
                if (ch->rect.w > col_w[col]) col_w[col] = ch->rect.w;
                if (ch->rect.h > row_h[row]) row_h[row] = ch->rect.h;
            }
            float content_w = 0.0f;
            float content_h = 0.0f;
            for (int c = 0; c < cols; c++) {
                content_w += col_w[c];
                if (c + 1 < cols) content_w += node->source->spacing;
            }
            for (int r = 0; r < rows; r++) {
                content_h += row_h[r];
                if (r + 1 < rows) content_h += node->source->spacing;
            }
            node->rect.w = content_w + padding * 2.0f + border * 2.0f;
            node->rect.h = content_h + padding * 2.0f + border * 2.0f;
        }
        free(col_w);
        free(row_h);
    } else if (node->child_count > 0) { /* absolute container */
        float max_w = 0.0f, max_h = 0.0f;
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* ch = &node->children[i];
            float child_x = ch->rect.x;
            float child_y = ch->rect.y;
            if (ch->source) {
                if (ch->source->has_x) child_x = ch->source->rect.x;
                if (ch->source->has_y) child_y = ch->source->rect.y;
            }

            float right = child_x + ch->rect.w;
            float bottom = child_y + ch->rect.h;
            if (right > max_w) max_w = right;
            if (bottom > max_h) max_h = bottom;
        }
        node->rect.w = max_w + padding * 2.0f + border * 2.0f;
        node->rect.h = max_h + padding * 2.0f + border * 2.0f;
    } else {
        if (node->source->widget_type == W_SPACER) {
            node->rect.w = node->source->has_w ? node->source->rect.w : 0.0f;
            node->rect.h = node->source->has_h ? node->source->rect.h : 0.0f;
        } else {
            float text_w = 0.0f, text_h = fallback_line_height();
            if (node->source->text) {
                measure_text(node->source->text, &text_w, &text_h);
            }
            node->rect.w = node->source->has_w ? node->source->rect.w : text_w + padding * 2.0f + border * 2.0f;
            node->rect.h = node->source->has_h ? node->source->rect.h : text_h + padding * 2.0f + border * 2.0f;
        }
    }

    if (node->source->has_floating_rect) {
        if (node->source->floating_rect.w > 0.0f) node->rect.w = node->source->floating_rect.w;
        if (node->source->floating_rect.h > 0.0f) node->rect.h = node->source->floating_rect.h;
    }

    if (node->source->has_min_w && node->rect.w < node->source->min_w) node->rect.w = node->source->min_w;
    if (node->source->has_min_h && node->rect.h < node->source->min_h) node->rect.h = node->source->min_h;
    if (node->source->has_w) node->rect.w = node->source->rect.w;
    if (node->source->has_h) node->rect.h = node->source->rect.h;
    if (node->source->has_max_w && node->rect.w > node->source->max_w) node->rect.w = node->source->max_w;
    if (node->source->has_max_h && node->rect.h > node->source->max_h) node->rect.h = node->source->max_h;

    if (node->source->has_floating_min) {
        if (node->rect.w < node->source->floating_min_w) node->rect.w = node->source->floating_min_w;
        if (node->rect.h < node->source->floating_min_h) node->rect.h = node->source->floating_min_h;
    }
    if (node->source->has_floating_max) {
        if (node->rect.w > node->source->floating_max_w) node->rect.w = node->source->floating_max_w;
        if (node->rect.h > node->source->floating_max_h) node->rect.h = node->source->floating_max_h;
    }
}

void measure_layout(LayoutNode* root, const char* font_path) {
    ensure_font_metrics(font_path);
    measure_node(root);
}

static void layout_node(LayoutNode* node, float origin_x, float origin_y, const Vec2* parent_transform) {
    if (!node || !node->source) return;
    const Style* default_style = ui_default_style();
    float padding = node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : default_style->padding);
    float border = node->source->border_thickness;
    float local_x = 0.0f;
    float local_y = 0.0f;
    if (node->source->has_floating_rect) {
        local_x = node->source->floating_rect.x;
        local_y = node->source->floating_rect.y;
    } else {
        if (node->source->has_x) local_x = node->source->rect.x;
        if (node->source->has_y) local_y = node->source->rect.y;
    }
    float base_x = origin_x + local_x;
    float base_y = origin_y + local_y;
    node->rect.x = base_x;
    node->rect.y = base_y;
    node->local_rect = (Rect){0.0f, 0.0f, node->rect.w, node->rect.h};
    node->transform = parent_transform ? (Vec2){parent_transform->x + base_x, parent_transform->y + base_y} : (Vec2){base_x, base_y};
    node->wants_clip = node->source->clip_to_viewport;

    if (node->source->layout == UI_LAYOUT_ROW) {
        float cursor_x = base_x + padding + border;
        float cursor_y = base_y + padding + border;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], cursor_x, cursor_y, &node->transform);
            cursor_x += node->children[i].rect.w + node->source->spacing;
        }
    } else if (node->source->layout == UI_LAYOUT_COLUMN) {
        float cursor_x = base_x + padding + border;
        float cursor_y = base_y + padding + border;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], cursor_x, cursor_y, &node->transform);
            cursor_y += node->children[i].rect.h + node->source->spacing;
        }
    } else if (node->source->layout == UI_LAYOUT_TABLE && node->source->columns > 0) {
        int cols = node->source->columns;
        int rows = (int)((node->child_count + (size_t)cols - 1) / (size_t)cols);
        float* col_w = (float*)calloc((size_t)cols, sizeof(float));
        float* row_h = (float*)calloc((size_t)rows, sizeof(float));
        if (col_w && row_h) {
            for (size_t i = 0; i < node->child_count; i++) {
                int col = (int)(i % (size_t)cols);
                int row = (int)(i / (size_t)cols);
                LayoutNode* ch = &node->children[i];
                if (ch->rect.w > col_w[col]) col_w[col] = ch->rect.w;
                if (ch->rect.h > row_h[row]) row_h[row] = ch->rect.h;
            }
            float y = base_y + padding + border;
            size_t idx = 0;
            for (int r = 0; r < rows; r++) {
                float x = base_x + padding + border;
                for (int c = 0; c < cols && idx < node->child_count; c++, idx++) {
                    layout_node(&node->children[idx], x, y, &node->transform);
                    x += col_w[c] + node->source->spacing;
                }
                y += row_h[r] + node->source->spacing;
            }
        }
        free(col_w);
        free(row_h);
    } else if (node->child_count > 0) {
        float offset_x = base_x + padding;
        float offset_y = base_y + padding;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], offset_x, offset_y, &node->transform);
        }
    }
}

void assign_layout(LayoutNode* root, float origin_x, float origin_y) {
    layout_node(root, origin_x, origin_y, NULL);
}

static void copy_base_rect(LayoutNode* node) {
    if (!node) return;
    node->base_rect = node->rect;
    for (size_t i = 0; i < node->child_count; i++) copy_base_rect(&node->children[i]);
}

void capture_layout_base(LayoutNode* root) { copy_base_rect(root); }

