#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "test_framework.h"

#include "services/ui/compositor.h"
#include "services/ui/scroll.h"
#include "services/ui/model_style.h"
#include "services/ui/ui_node.h"
#include "services/ui/layout_tree.h"
#include "services/ui/widget_list.h"
#include "services/ui/compositor.h"

static int test_popup_not_clipped(void) {
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
    TEST_ASSERT(layout);
    measure_layout(layout, NULL);
    assign_layout(layout, 0.0f, 0.0f);

    WidgetArray widgets = materialize_widgets(layout);
    TEST_ASSERT_INT_EQ(2, widgets.count);

    DisplayList list = ui_compositor_build(layout, widgets.items, widgets.count);
    TEST_ASSERT_INT_EQ(2, list.count);

    size_t popup_idx = 0;
    for (size_t i = 0; i < list.count; ++i) {
        if (list.items[i].widget && list.items[i].widget->id && strcmp(list.items[i].widget->id, "popup") == 0) {
            popup_idx = i;
            break;
        }
    }
    TEST_ASSERT_INT_EQ(0, list.items[popup_idx].clip_depth);

    ui_compositor_free(list);
    free_widgets(widgets);
    free_layout_tree(layout);
    return 1;
}

static int test_scrollbar_not_clipped(void) {
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
    TEST_ASSERT(layout);
    measure_layout(layout, NULL);
    assign_layout(layout, 0.0f, 0.0f);

    WidgetArray widgets = materialize_widgets(layout);
    TEST_ASSERT_INT_EQ(2, widgets.count);
    apply_widget_padding_scale(&widgets, 1.0f);

    ScrollContext* scroll = scroll_init(widgets.items, widgets.count);
    TEST_ASSERT(scroll);
    scroll_apply_offsets(scroll, widgets.items, widgets.count);

    DisplayList list = ui_compositor_build(layout, widgets.items, widgets.count);
    TEST_ASSERT_INT_EQ(2, list.count);

    const DisplayItem* content_item = NULL;
    const DisplayItem* scrollbar_item = NULL;
    for (size_t i = 0; i < list.count; ++i) {
        if (!list.items[i].widget) continue;
        if (list.items[i].widget->id && strcmp(list.items[i].widget->id, "content") == 0) content_item = &list.items[i];
        if (list.items[i].widget->type == W_SCROLLBAR) scrollbar_item = &list.items[i];
    }
    TEST_ASSERT(content_item && scrollbar_item);
    TEST_ASSERT(content_item->clip_depth > 0);
    TEST_ASSERT_INT_EQ(0, scrollbar_item->clip_depth);

    ui_compositor_free(list);
    scroll_free(scroll);
    free_widgets(widgets);
    free_layout_tree(layout);
    return 1;
}

int main(void) {
    RUN_TEST(test_popup_not_clipped);
    RUN_TEST(test_scrollbar_not_clipped);
    
    printf("Tests Run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}