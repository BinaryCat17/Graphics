#include "test_framework.h"
#include "engine/ui/ui_def.h"
#include "engine/ui/ui_layout.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Mock Measurement Function ---
static float mock_measure_text(const char* text, void* user_data) {
    (void)user_data; 
    if (!text) return 0.0f;
    return strlen(text) * 10.0f;
}

// --- Helper: Build Def Tree first ---
static UiDef* create_def(MemoryArena* arena, UiNodeType type, UiLayoutType layout, float w, float h, const char* id) {
    UiDef* def = ui_def_create(arena, type);
    if (!def) return NULL;
    def->layout = layout;
    def->width = w;
    def->height = h;
    def->id = arena_push_string(arena, id ? id : "node");
    return def;
}

static void add_child_def(MemoryArena* arena, UiDef* parent, UiDef* child) {
    parent->child_count++;
    // Realloc via arena? 
    // Arena doesn't support realloc.
    // For tests, we can just alloc a new array and copy.
    UiDef** new_children = (UiDef**)arena_alloc(arena, parent->child_count * sizeof(UiDef*));
    if (parent->children) {
        memcpy(new_children, parent->children, (parent->child_count - 1) * sizeof(UiDef*));
    }
    new_children[parent->child_count - 1] = child;
    parent->children = new_children;
}

// --- TESTS ---

int test_column_layout() {
    MemoryArena arena;
    arena_init(&arena, 4096);

    UiDef* root_def = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_COLUMN, 100.0f, 200.0f, "root");
    root_def->spacing = 10.0f;
    root_def->padding = 5.0f;
    // Set root arena ownership manually since we created it outside loader
    root_def->arena = arena; 

    UiDef* c1 = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c1");
    UiDef* c2 = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c2");
    
    add_child_def(&arena, root_def, c1);
    add_child_def(&arena, root_def, c2);

    UiView* view = ui_view_create(root_def, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);

    TEST_ASSERT_FLOAT_EQ(view->rect.w, 100.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(view->rect.h, 200.0f, 0.1f);

    UiView* v1 = view->children[0];
    UiView* v2 = view->children[1];

    TEST_ASSERT_FLOAT_EQ(v1->rect.x, 5.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v1->rect.y, 5.0f, 0.1f);
    
    TEST_ASSERT_FLOAT_EQ(v2->rect.x, 5.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v2->rect.y, 65.0f, 0.1f);

    ui_view_free(view);
    
    // Cleaning up root cleans up arena
    ui_def_free(root_def);
    return 1;
}

int test_row_layout_auto_width() {
    MemoryArena arena;
    arena_init(&arena, 4096);

    ui_layout_set_measure_func(mock_measure_text, NULL);

    UiDef* root_def = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_ROW, -1.0f, 50.0f, "root");
    root_def->spacing = 5.0f;
    root_def->padding = 2.0f;
    root_def->arena = arena;

    UiDef* l1 = create_def(&arena, UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "l1");
    l1->text = arena_push_string(&arena, "ABC");

    UiDef* l2 = create_def(&arena, UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "l2");
    l2->text = arena_push_string(&arena, "DE");

    add_child_def(&arena, root_def, l1);
    add_child_def(&arena, root_def, l2);

    UiView* view = ui_view_create(root_def, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);

    UiView* v1 = view->children[0];
    UiView* v2 = view->children[1];

    TEST_ASSERT_FLOAT_EQ(v1->rect.w, 30.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v2->rect.w, 20.0f, 0.1f);

    TEST_ASSERT_FLOAT_EQ(v1->rect.x, 2.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v2->rect.x, 37.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(view->rect.w, 800.0f, 0.1f);

    ui_view_free(view);
    ui_def_free(root_def);
    return 1;
}

int test_nested_auto_size() {
    MemoryArena arena;
    arena_init(&arena, 4096);

    ui_layout_set_measure_func(mock_measure_text, NULL);
    
    UiDef* root = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_COLUMN, 200.0f, 200.0f, "root");
    root->arena = arena;
    
    UiDef* row = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_ROW, -1.0f, -1.0f, "row");
    UiDef* lbl = create_def(&arena, UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "lbl");
    lbl->text = arena_push_string(&arena, "A"); 

    add_child_def(&arena, row, lbl);
    add_child_def(&arena, root, row);

    UiView* view = ui_view_create(root, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);
    
    UiView* v_row = view->children[0];
    UiView* v_lbl = v_row->children[0];

    TEST_ASSERT_FLOAT_EQ(v_lbl->rect.w, 10.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v_row->rect.w, 200.0f, 0.1f);
    TEST_ASSERT_FLOAT_EQ(v_row->rect.h, 30.0f, 0.1f);

    ui_view_free(view);
    ui_def_free(root);
    return 1;
}

int test_overlay_layout() {
    MemoryArena arena;
    arena_init(&arena, 4096);

    UiDef* root = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_OVERLAY, 100.0f, 100.0f, "root");
    root->padding = 10.0f;
    root->arena = arena;
    
    UiDef* c1 = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c1");
    UiDef* c2 = create_def(&arena, UI_NODE_PANEL, UI_LAYOUT_COLUMN, 50.0f, 50.0f, "c2");
    
    add_child_def(&arena, root, c1);
    add_child_def(&arena, root, c2);

    UiView* view = ui_view_create(root, NULL, NULL);
    ui_layout_root(view, 800, 600, 0, false);

    UiView* v1 = view->children[0];
    UiView* v2 = view->children[1];

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
    MemoryArena arena;
    arena_init(&arena, 4096);

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
    UiDef* list_def = create_def(&arena, UI_NODE_LIST, UI_LAYOUT_COLUMN, 200.0f, -1.0f, "list");
    list_def->data_source = arena_push_string(&arena, "nodes");
    list_def->count_source = arena_push_string(&arena, "node_count");
    list_def->arena = arena;
    
    // Item Template (Label)
    UiDef* item_def = create_def(&arena, UI_NODE_LABEL, UI_LAYOUT_COLUMN, -1.0f, -1.0f, "item");
    item_def->text = arena_push_string(&arena, "Node {id}"); 
    list_def->item_template = item_def;

    // 3. Create View
    const MetaStruct* graph_meta = meta_get_struct("MathGraph");
    TEST_ASSERT(graph_meta != NULL);
    
    UiView* view = ui_view_create(list_def, &graph, graph_meta);
    TEST_ASSERT(view != NULL);
    
    ui_view_update(view);

    TEST_ASSERT_INT_EQ(3, view->child_count);
    
    UiView* child0 = view->children[0];
    TEST_ASSERT(child0 != NULL);
    
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
