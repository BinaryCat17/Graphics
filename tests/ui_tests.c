#include "test_framework.h"
#include "engine/ui/ui_def.h"
#include "engine/ui/ui_layout.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Mock Measurement Function ---
// Simulates a monospaced font where each character is 10px wide.
static float mock_measure_text(const char* text, void* user_data) {
    (void)user_data; // Unused
    if (!text) return 0.0f;
    return strlen(text) * 10.0f;
}

// --- Helper: Build Def Tree first ---
static UiDef* create_def(UiNodeType type, UiLayoutType layout, float w, float h, const char* id) {
    UiDef* def = ui_def_create(type);
    def->layout = layout;
    def->width = w;
    def->height = h;
    def->id = strdup(id ? id : "node");
    return def;
}

static void add_child_def(UiDef* parent, UiDef* child) {
    parent->child_count++;
    parent->children = (UiDef**)realloc(parent->children, parent->child_count * sizeof(UiDef*));
    parent->children[parent->child_count - 1] = child;
}

// --- TESTS ---

int test_column_layout() {
    // [Column 100x200]
    //   - Child1 (Fixed 50x50)
    //   - Child2 (Fixed 50x50)
    
    UiDef* root_def = create_def(UI_NODE_PANEL, UI_LAYOUT_COLUMN, 100.0f, 200.0f, "root");
    root_def->spacing = 10.0f;
    root_def->padding = 5.0f;

    UiDef* c1 = create_def(UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c1");
    UiDef* c2 = create_def(UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c2");
    
    add_child_def(root_def, c1);
    add_child_def(root_def, c2);

    UiView* view = ui_view_create(root_def, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);

    // Verify Root
    // x=0, y=0, w=100, h=200
    TEST_ASSERT_FLOAT_EQ(view->rect.w, 100.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(view->rect.h, 200.0f, 0.1f);

    // Verify Children
    UiView* v1 = view->children[0];
    UiView* v2 = view->children[1];

    // Child 1: x = root.x + pad = 5. y = root.y + pad = 5.
    TEST_ASSERT_FLOAT_EQ(v1->rect.x, 5.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v1->rect.y, 5.0f, 0.1f);
    
    // Child 2: y = v1.y + v1.h + spacing = 5 + 50 + 10 = 65.
    TEST_ASSERT_FLOAT_EQ(v2->rect.x, 5.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v2->rect.y, 65.0f, 0.1f);

    ui_view_free(view);
    ui_def_free(root_def);
    return 1;
}

int test_row_layout_auto_width() {
    // [Row Auto x 50]
    //   - Label1 ("ABC") -> 30px
    //   - Label2 ("DE")  -> 20px
    //   - Spacing 5, Padding 2
    
    ui_layout_set_measure_func(mock_measure_text, NULL);

    UiDef* root_def = create_def(UI_NODE_PANEL, UI_LAYOUT_ROW, -1.0f, 50.0f, "root");
    root_def->spacing = 5.0f;
    root_def->padding = 2.0f;

    UiDef* l1 = create_def(UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "l1");
    l1->text = strdup("ABC");

    UiDef* l2 = create_def(UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "l2");
    l2->text = strdup("DE");

    add_child_def(root_def, l1);
    add_child_def(root_def, l2);

    UiView* view = ui_view_create(root_def, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);

    UiView* v1 = view->children[0];
    UiView* v2 = view->children[1];

    // Check sizes (Mock measure: 10px per char)
    // L1: 3 chars * 10 = 30.
    // L1 width in layout is text_w + padding*2.
    // Def padding is 0 by default. So 30.
    TEST_ASSERT_FLOAT_EQ(v1->rect.w, 30.0f, 0.1f);
    // L2: 2 chars * 10 = 20.
    TEST_ASSERT_FLOAT_EQ(v2->rect.w, 20.0f, 0.1f);

    // Check Positions
    // Root Pad = 2.
    // V1 x = 2.
    TEST_ASSERT_FLOAT_EQ(v1->rect.x, 2.0f, 0.1f);
    
    // V2 x = v1.x + v1.w + spacing = 2 + 30 + 5 = 37.
    TEST_ASSERT_FLOAT_EQ(v2->rect.x, 37.0f, 0.1f);

    // Root Width (Auto)
    // If w < 0:
    //   if (parent_is_row || type==LABEL...) w = measured...
    //   else w = available.w;
    // Here root is PANEL and has no parent. So it will take available.w (800).
    TEST_ASSERT_FLOAT_EQ(view->rect.w, 800.0f, 0.1f);

    ui_view_free(view);
    ui_def_free(root_def);
    return 1;
}

int test_nested_auto_size() {
    // [Column 200x200]
    //   - [Row Auto x Auto]
    //       - Label "A"
    
    ui_layout_set_measure_func(mock_measure_text, NULL);
    
    UiDef* root = create_def(UI_NODE_PANEL, UI_LAYOUT_COLUMN, 200.0f, 200.0f, "root");
    UiDef* row = create_def(UI_NODE_PANEL, UI_LAYOUT_ROW, -1.0f, -1.0f, "row");
    UiDef* lbl = create_def(UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "lbl");
    lbl->text = strdup("A"); // 10px

    add_child_def(row, lbl);
    add_child_def(root, row);

    UiView* view = ui_view_create(root, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);
    
    UiView* v_row = view->children[0];
    UiView* v_lbl = v_row->children[0];

    // Label: 10px wide.
    TEST_ASSERT_FLOAT_EQ(v_lbl->rect.w, 10.0f, 0.1f);

    // Row: 
    // Logic: w = available.w.
    // So Row should be 200px wide (inherited from root content width).
    TEST_ASSERT_FLOAT_EQ(v_row->rect.w, 200.0f, 0.1f);
    
    // Row Height (Auto):
    // Columns calculate height based on children, but Rows default to 30.0f if auto.
    TEST_ASSERT_FLOAT_EQ(v_row->rect.h, 30.0f, 0.1f);

    ui_view_free(view);
    ui_def_free(root);
    return 1;
}

int test_overlay_layout() {
    // [Overlay 100x100]
    //   - Child1 50x50
    //   - Child2 50x50
    // Both should be at (0,0) relative to content area.
    
    UiDef* root = create_def(UI_NODE_PANEL, UI_LAYOUT_OVERLAY, 100.0f, 100.0f, "root");
    root->padding = 10.0f;
    
    UiDef* c1 = create_def(UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c1");
    UiDef* c2 = create_def(UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c2");
    
    add_child_def(root, c1);
    add_child_def(root, c2);

    UiView* view = ui_view_create(root, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);

    UiView* v1 = view->children[0];
    UiView* v2 = view->children[1];

    // Root x=0, y=0. Padding=10.
    // Content starts at 10,10.
    TEST_ASSERT_FLOAT_EQ(v1->rect.x, 10.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v1->rect.y, 10.0f, 0.1f);
    
    TEST_ASSERT_FLOAT_EQ(v2->rect.x, 10.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v2->rect.y, 10.0f, 0.1f);

    ui_view_free(view);
    ui_def_free(root);
    return 1;
}

#include "features/graph_editor/math_graph.h"

int test_repeater() {
    // 1. Setup Data Model
    MathGraph graph = {0};
    graph.node_count = 3;
    graph.nodes = (MathNode**)calloc(3, sizeof(MathNode*));
    for(int i=0; i<3; ++i) {
        graph.nodes[i] = (MathNode*)calloc(1, sizeof(MathNode));
        graph.nodes[i]->id = i + 1;
        graph.nodes[i]->value = (float)i * 10.0f;
    }

    // 2. Setup UI Definition
    // Root (List)
    UiDef* list_def = create_def(UI_NODE_LIST, UI_LAYOUT_COLUMN, 200.0f, -1.0f, "list");
    list_def->data_source = strdup("nodes");
    list_def->count_source = strdup("node_count");
    
    // Item Template (Label)
    UiDef* item_def = create_def(UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "item");
    item_def->text = strdup("Node {id}"); // Bind to ID
    list_def->item_template = item_def;

    // 3. Create View
    const MetaStruct* graph_meta = meta_get_struct("MathGraph");
    TEST_ASSERT(graph_meta != NULL);
    
    UiView* view = ui_view_create(list_def, &graph, graph_meta);
    TEST_ASSERT(view != NULL);
    
    // 4. Update (Triggers repeater logic)
    ui_view_update(view);

    // 5. Verify
    TEST_ASSERT_INT_EQ(3, view->child_count);
    
    UiView* child0 = view->children[0];
    TEST_ASSERT(child0 != NULL);
    
    // Check binding resolution
    // Note: resolve_text_binding runs in ui_view_update.
    TEST_ASSERT_STR_EQ("Node 1", child0->cached_text);
    
    // Cleanup
    ui_view_free(view);
    ui_def_free(list_def); 
    
    for(int i=0; i<3; ++i) free(graph.nodes[i]);
    free(graph.nodes);
    
    return 1;
}

int main() {
    printf("--- Running UI Tests ---\n");

    RUN_TEST(test_column_layout);
    RUN_TEST(test_row_layout_auto_width);
    RUN_TEST(test_nested_auto_size);
    RUN_TEST(test_overlay_layout);
    RUN_TEST(test_repeater);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d TESTS FAILED\n" TERM_RESET, g_tests_failed);
        return 1;
    }
    printf(TERM_GREEN "\nALL TESTS PASSED\n" TERM_RESET);
    return 0;
}