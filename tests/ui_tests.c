#include "test_framework.h"
#include "engine/ui/ui_def.h"
#include "engine/ui/ui_layout.h"
#include <stdio.h>

// Mock logger if needed, but we link against foundation_logger
// But we need to ensure we can run without crashing.

int test_row_layout_label_autosize() {
    // Parent Panel (Row, Width 400)
    UiDef* parent_def = ui_def_create(UI_NODE_PANEL);
    parent_def->layout = UI_LAYOUT_ROW;
    parent_def->width = 400.0f;
    parent_def->height = 40.0f;
    parent_def->spacing = 10.0f;
    parent_def->padding = 0.0f;
    parent_def->id = strdup("parent");

    // Child 1: Label "Label 1" (Auto width)
    UiDef* child1_def = ui_def_create(UI_NODE_LABEL);
    child1_def->text = strdup("Label 1"); // 7 chars -> ~70px + padding
    child1_def->width = -1.0f;
    child1_def->id = strdup("child1");

    // Child 2: Label "Label 2" (Auto width)
    UiDef* child2_def = ui_def_create(UI_NODE_LABEL);
    child2_def->text = strdup("Label 2");
    child2_def->width = -1.0f;
    child2_def->id = strdup("child2");

    // Link definitions (Manually since we don't use loader)
    parent_def->child_count = 2;
    parent_def->children = (UiDef**)calloc(2, sizeof(UiDef*));
    parent_def->children[0] = child1_def;
    parent_def->children[1] = child2_def;

    // Create View
    UiView* root_view = ui_view_create(parent_def, NULL, NULL);
    
    // Run Layout
    ui_layout_root(root_view, 800.0f, 600.0f, 0, false);

    // Verify
    UiView* child1 = root_view->children[0];
    UiView* child2 = root_view->children[1];

    printf("Child 1 Rect: %.1f, %.1f, %.1f, %.1f\n", child1->rect.x, child1->rect.y, child1->rect.w, child1->rect.h);
    printf("Child 2 Rect: %.1f, %.1f, %.1f, %.1f\n", child2->rect.x, child2->rect.y, child2->rect.w, child2->rect.h);

    // Child 1 should NOT be 400.0f. It should be much smaller (e.g. < 150.0f).
    // The previous bug caused it to be 400.0f (available.w).
    
    int result = 1;
    
    if (child1->rect.w >= 390.0f) {
        printf(TERM_RED "FAILED: Child 1 took full width (%.1f) in ROW layout!\n" TERM_RESET, child1->rect.w);
        result = 0;
    }

    if (child2->rect.x >= 400.0f) {
        printf(TERM_RED "FAILED: Child 2 pushed out of bounds (x=%.1f)!\n" TERM_RESET, child2->rect.x);
        result = 0;
    }
    
    // Expect rough size: 7 chars * 10 = 70 + 10 padding = ~80.
    if (child1->rect.w < 50.0f || child1->rect.w > 200.0f) {
        printf(TERM_RED "FAILED: Child 1 width (%.1f) is suspicious.\n" TERM_RESET, child1->rect.w);
        result = 0;
    }

    // Cleanup
    ui_view_free(root_view);
    ui_def_free(parent_def); // Frees children too

    return result;
}

int main() {
    RUN_TEST(test_row_layout_label_autosize);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d TESTS FAILED\n" TERM_RESET, g_tests_failed);
        return 1;
    }
    printf(TERM_GREEN "\nALL TESTS PASSED\n" TERM_RESET);
    return 0;
}
