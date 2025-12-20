#include "test_framework.h"
#include "engine/ui/ui_core.h"
#include "engine/ui/ui_parser.h"
#include "engine/ui/ui_layout.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Helper: Build Def Tree first ---
static UiNodeSpec* create_node(UiAsset* asset, UiLayoutStrategy layout, float w, float h, const char* id) {
    UiNodeSpec* spec = ui_asset_push_node(asset);
    if (!spec) return NULL;
    spec->layout = layout;
    spec->width = w;
    spec->height = h;
    spec->id = arena_push_string(&asset->arena, id ? id : "node");
    return spec;
}

static void add_child_spec(UiAsset* asset, UiNodeSpec* parent, UiNodeSpec* child) {
    parent->child_count++;
    UiNodeSpec** new_children = (UiNodeSpec**)arena_alloc(&asset->arena, parent->child_count * sizeof(UiNodeSpec*));
    if (parent->children) {
        memcpy(new_children, parent->children, (parent->child_count - 1) * sizeof(UiNodeSpec*));
    }
    new_children[parent->child_count - 1] = child;
    parent->children = new_children;
}

// --- TESTS ---

int test_column_layout() {
    UiAsset* asset = ui_asset_create(4096);

    UiNodeSpec* root = create_node(asset, UI_LAYOUT_FLEX_COLUMN, 100.0f, 200.0f, "root");
    root->spacing = 10.0f;
    root->padding = 5.0f;
    
    UiNodeSpec* c1 = create_node(asset, UI_LAYOUT_FLEX_COLUMN, 50.0f, 50.0f, "c1");
    UiNodeSpec* c2 = create_node(asset, UI_LAYOUT_FLEX_COLUMN, 50.0f, 50.0f, "c2");
    
    add_child_spec(asset, root, c1);
    add_child_spec(asset, root, c2);
    
    UiInstance instance;
    ui_instance_init(&instance, 4096);

    UiElement* el = ui_element_create(&instance, root, NULL, NULL);
    ui_layout_root(el, 800, 600, 0, false, NULL, NULL);

    TEST_ASSERT_FLOAT_EQ(el->rect.w, 100.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(el->rect.h, 200.0f, 0.1f);

    UiElement* v1 = el->first_child;
    UiElement* v2 = el->first_child->next_sibling;

    TEST_ASSERT_FLOAT_EQ(v1->rect.x, 5.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v1->rect.y, 5.0f, 0.1f);
    
    TEST_ASSERT_FLOAT_EQ(v2->rect.x, 5.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v2->rect.y, 65.0f, 0.1f);

    ui_instance_destroy(&instance);
    ui_asset_free(asset);
    return 1;
}

int main() {
    printf("--- Running UI Tests ---\n");
    RUN_TEST(test_column_layout);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%dTESTS FAILED\n" TERM_RESET, g_tests_failed);
        return 1;
    }
    printf(TERM_GREEN "\nALL TESTS PASSED\n" TERM_RESET);
    return 0;
}
