#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "ui_json.h"
#include "scroll.h"

typedef struct {
    WidgetArray widgets;
    UiNode* root;
    LayoutNode* layout;
    Style* styles;
} LayoutFixture;

static LayoutFixture build_widgets(const char* styles_json, const char* layout_json) {
    LayoutFixture fx = {0};
    fx.styles = styles_json ? parse_styles_json(styles_json) : NULL;
    fx.root = parse_layout_json(layout_json, NULL, fx.styles);
    assert(fx.root);
    fx.layout = build_layout_tree(fx.root);
    assert(fx.layout);
    measure_layout(fx.layout);
    assign_layout(fx.layout, 0.0f, 0.0f);
    fx.widgets = materialize_widgets(fx.layout);
    populate_widgets_from_layout(fx.layout, fx.widgets.items, fx.widgets.count);
    return fx;
}

static void free_fixture(LayoutFixture* fx) {
    free_widgets(fx->widgets);
    free_layout_tree(fx->layout);
    free_ui_tree(fx->root);
    free_styles(fx->styles);
}

static void test_row_layout(void) {
    const char* styles_json = "{\"styles\":{\"zeroPad\":{\"padding\":0}}}";
    const char* layout_json = "{\"layout\":{\"type\":\"row\",\"style\":\"zeroPad\",\"spacing\":5,\"children\":[{\"type\":\"button\",\"w\":50,\"h\":20},{\"type\":\"label\",\"w\":30,\"h\":10}]}}";
    LayoutFixture fx = build_widgets(styles_json, layout_json);
    assert(fx.widgets.count == 2);
    assert(fx.widgets.items[0].rect.x == 0.0f);
    assert(fx.widgets.items[0].rect.y == 0.0f);
    assert(fx.widgets.items[1].rect.x == 55.0f);
    assert(fx.widgets.items[1].rect.y == 0.0f);
    free_fixture(&fx);
}

static void test_column_layout_with_scroll(void) {
    const char* styles_json = "{\"styles\":{\"zeroPad\":{\"padding\":0}}}";
    const char* layout_json = "{\"layout\":{\"type\":\"column\",\"style\":\"zeroPad\",\"spacing\":7,\"children\":[{\"type\":\"button\",\"w\":40,\"h\":18,\"scrollArea\":\"area1\"},{\"type\":\"button\",\"w\":40,\"h\":12,\"scrollArea\":\"area1\"}]}}";
    LayoutFixture fx = build_widgets(styles_json, layout_json);
    assert(fx.widgets.count == 2);
    assert(fx.widgets.items[0].rect.x == 0.0f);
    assert(fx.widgets.items[0].rect.y == 0.0f);
    assert(fx.widgets.items[1].rect.x == 0.0f);
    assert(fx.widgets.items[1].rect.y == 25.0f);
    ScrollContext* ctx = scroll_init(fx.widgets.items, fx.widgets.count);
    assert(ctx != NULL);
    assert(fx.widgets.items[0].scroll_offset == 0.0f);
    assert(fx.widgets.items[1].scroll_offset == 0.0f);
    scroll_free(ctx);
    free_fixture(&fx);
}

static void test_table_layout(void) {
    const char* styles_json = "{\"styles\":{\"zeroPad\":{\"padding\":0}}}";
    const char* layout_json = "{\"layout\":{\"type\":\"table\",\"style\":\"zeroPad\",\"columns\":2,\"spacing\":3,\"children\":[{\"type\":\"panel\",\"w\":10,\"h\":10},{\"type\":\"panel\",\"w\":12,\"h\":8},{\"type\":\"panel\",\"w\":6,\"h\":14}]}}";
    LayoutFixture fx = build_widgets(styles_json, layout_json);
    assert(fx.widgets.count == 3);
    assert(fx.widgets.items[0].rect.x == 0.0f && fx.widgets.items[0].rect.y == 0.0f);
    assert(fx.widgets.items[1].rect.x == 13.0f && fx.widgets.items[1].rect.y == 0.0f);
    assert(fx.widgets.items[2].rect.x == 0.0f && fx.widgets.items[2].rect.y == 13.0f);
    free_fixture(&fx);
}

static void test_padding_scale_is_stable(void) {
    Widget w = { .base_padding = 10.0f, .padding = 10.0f };
    WidgetArray arr = { .items = &w, .count = 1 };
    apply_widget_padding_scale(&arr, 2.0f);
    assert(fabsf(w.padding - 20.0f) < 0.001f);
    apply_widget_padding_scale(&arr, 2.0f);
    assert(fabsf(w.padding - 20.0f) < 0.001f);
    apply_widget_padding_scale(&arr, 0.5f);
    assert(fabsf(w.padding - 5.0f) < 0.001f);
}

int main(void) {
    test_row_layout();
    test_column_layout_with_scroll();
    test_table_layout();
    test_padding_scale_is_stable();
    printf("layout_tests passed\n");
    return 0;
}
