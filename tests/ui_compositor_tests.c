#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ui/compositor.h"
#include "ui/scroll.h"
#include "ui/model_style.h"
#include "ui/ui_node.h"
#include "ui/layout_tree.h"
#include "ui/widget_list.h"
#include "ui/compositor.h"

static void test_popup_not_clipped(void) {
    UiNode popup = {.layout = UI_LAYOUT_NONE, .widget_type = W_PANEL, .has_w = 1, .has_h = 1, .has_x = 1, .has_y = 1};
    popup.rect = (Rect){150.0f, 10.0f, 80.0f, 30.0f};
    popup.id = "popup";

    UiNode parent = {.layout = UI_LAYOUT_NONE, .widget_type = W_PANEL, .has_w = 1, .has_h = 1, .has_x = 1, .has_y = 1};
    parent.rect = (Rect){20.0f, 20.0f, 100.0f, 80.0f};
    parent.id = "parent";

    UiNode children[] = {parent, popup};

    UiNode root = {.layout = UI_LAYOUT_ABSOLUTE, .widget_type = W_PANEL, .has_w = 1, .has_h = 1};
    root.rect = (Rect){0.0f, 0.0f, 300.0f, 200.0f};
    root.children = children;
    root.child_count = sizeof(children) / sizeof(children[0]);

    LayoutNode* layout = build_layout_tree(&root);
    assert(layout);
    measure_layout(layout, NULL);
    assign_layout(layout, 0.0f, 0.0f);

    WidgetArray widgets = materialize_widgets(layout);
    assert(widgets.count == 2);

    DisplayList list = ui_compositor_build(layout, widgets.items, widgets.count);
    assert(list.count == 2);

    size_t popup_idx = 0;
    for (size_t i = 0; i < list.count; ++i) {
        if (list.items[i].widget && list.items[i].widget->id && strcmp(list.items[i].widget->id, "popup") == 0) {
            popup_idx = i;
            break;
        }
    }
    assert(list.items[popup_idx].clip_depth == 0);

    ui_compositor_free(list);
    free_widgets(widgets);
    free_layout_tree(layout);
}

static void test_scrollbar_not_clipped(void) {
    UiNode content = {
        .layout = UI_LAYOUT_NONE,
        .widget_type = W_PANEL,
        .scroll_area = "area",
        .clip_to_viewport = 1,
        .has_clip_to_viewport = 1,
        .has_w = 1,
        .has_h = 1,
    };
    content.rect = (Rect){0.0f, 0.0f, 100.0f, 240.0f};
    content.id = "content";

    UiNode scrollbar = {
        .layout = UI_LAYOUT_NONE,
        .widget_type = W_SCROLLBAR,
        .scroll_area = "area",
        .scroll_static = 1,
        .has_w = 1,
        .has_h = 1,
        .child_count = 1,
    };
    scrollbar.rect = (Rect){0.0f, 0.0f, 100.0f, 120.0f};
    scrollbar.id = "viewport";
    scrollbar.children = &content;

    UiNode root = {.layout = UI_LAYOUT_ABSOLUTE, .widget_type = W_PANEL, .has_w = 1, .has_h = 1};
    root.rect = (Rect){0.0f, 0.0f, 200.0f, 200.0f};
    root.children = &scrollbar;
    root.child_count = 1;

    LayoutNode* layout = build_layout_tree(&root);
    assert(layout);
    measure_layout(layout, NULL);
    assign_layout(layout, 0.0f, 0.0f);

    WidgetArray widgets = materialize_widgets(layout);
    assert(widgets.count == 2);
    apply_widget_padding_scale(&widgets, 1.0f);

    ScrollContext* scroll = scroll_init(widgets.items, widgets.count);
    assert(scroll);
    scroll_apply_offsets(scroll, widgets.items, widgets.count);

    DisplayList list = ui_compositor_build(layout, widgets.items, widgets.count);
    assert(list.count == 2);

    const DisplayItem* content_item = NULL;
    const DisplayItem* scrollbar_item = NULL;
    for (size_t i = 0; i < list.count; ++i) {
        if (!list.items[i].widget) continue;
        if (list.items[i].widget->id && strcmp(list.items[i].widget->id, "content") == 0) content_item = &list.items[i];
        if (list.items[i].widget->type == W_SCROLLBAR) scrollbar_item = &list.items[i];
    }
    assert(content_item && scrollbar_item);
    assert(content_item->clip_depth > 0);
    assert(scrollbar_item->clip_depth > 0);

    ui_compositor_free(list);
    scroll_free(scroll);
    free_widgets(widgets);
    free_layout_tree(layout);
}

int main(void) {
    test_popup_not_clipped();
    test_scrollbar_not_clipped();
    return 0;
}

