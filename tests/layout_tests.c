#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "ui/ui_json.h"
#include "ui/scroll.h"
#include "config/config_io.h"

typedef struct {
    WidgetArray widgets;
    UiNode* root;
    LayoutNode* layout;
    Style* styles;
} LayoutFixture;

static LayoutFixture build_widgets(const char* styles_json, const char* layout_json) {
    LayoutFixture fx = {0};
    ConfigNode* styles_root = NULL;
    ConfigNode* layout_root = NULL;
    ConfigError err = {0};
    if (styles_json) {
        assert(parse_config_text(styles_json, CONFIG_FORMAT_JSON, &styles_root, &err));
    }
    err = (ConfigError){0};
    assert(parse_config_text(layout_json, CONFIG_FORMAT_JSON, &layout_root, &err));
    fx.styles = styles_root ? parse_styles_config(styles_root) : NULL;
    fx.root = parse_layout_config(layout_root, NULL, fx.styles, NULL, NULL);
    config_node_free(styles_root);
    config_node_free(layout_root);
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
    const char* layout_json = "{\"layout\":{\"type\":\"column\",\"style\":\"zeroPad\",\"spacing\":7,\"children\":[{\"type\":\"button\",\"w\":40,\"h\":18},{\"type\":\"button\",\"w\":40,\"h\":12}]}}";
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

static void test_label_text_preserved_utf8(void) {
    const char* layout_json = "{\"layout\":{\"type\":\"column\",\"children\":[{\"type\":\"label\",\"text\":\"Привет мир\",\"style\":\"zero\"}]}}";
    const char* styles_json = "{\"styles\":{\"zero\":{\"padding\":0}}}";
    LayoutFixture fx = build_widgets(styles_json, layout_json);
    assert(fx.widgets.count == 1);
    assert(strcmp(fx.widgets.items[0].text, "Привет мир") == 0);
    free_fixture(&fx);
}

static void test_scrollbar_shown_for_overflow(void) {
    const char* styles_json = "{\"styles\":{\"zero\":{\"padding\":0}}}";
    const char* layout_json = "{\"layout\":{\"type\":\"column\",\"style\":\"zero\",\"children\":[{\"type\":\"column\",\"scrollStatic\":true,\"maxHeight\":40,\"children\":[{\"type\":\"button\",\"h\":30},{\"type\":\"button\",\"h\":30},{\"type\":\"button\",\"h\":30}]} ]}}";
    LayoutFixture fx = build_widgets(styles_json, layout_json);
    ScrollContext* ctx = scroll_init(fx.widgets.items, fx.widgets.count);
    assert(ctx != NULL);
    int found = 0;
    for (size_t i = 0; i < fx.widgets.count; i++) {
        Widget* w = &fx.widgets.items[i];
        if (w->scroll_static) {
            found = 1;
            assert(w->show_scrollbar);
        }
    }
    assert(found);
    scroll_free(ctx);
    free_fixture(&fx);
}

static void test_border_changes_size(void) {
    const char* styles_json = "{\"styles\":{\"bordered\":{\"padding\":0,\"borderThickness\":2}}}";
    const char* layout_json = "{\"layout\":{\"type\":\"column\",\"children\":[{\"type\":\"label\",\"style\":\"bordered\"}]}}";
    LayoutFixture fx = build_widgets(styles_json, layout_json);
    assert(fx.widgets.count == 1);
    assert(fabsf(fx.widgets.items[0].rect.h - 22.0f) < 0.01f);
    free_fixture(&fx);
}

int main(void) {
    test_row_layout();
    test_column_layout_with_scroll();
    test_table_layout();
    test_padding_scale_is_stable();
    test_label_text_preserved_utf8();
    test_scrollbar_shown_for_overflow();
    test_border_changes_size();
    printf("layout_tests passed\n");
    return 0;
}
