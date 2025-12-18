#include "test_framework.h"
#include "features/graph_editor/math_graph.h"
#include "features/graph_editor/transpiler.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_transpiler_simple_add() {
    MathGraph graph;
    MemoryArena arena;
    arena_init(&arena, 1024 * 1024);
    math_graph_init(&graph, &arena);
    
    // Create nodes: 3.0 + 5.0
    MathNodeId id1 = math_graph_add_node(&graph, MATH_NODE_VALUE);
    math_graph_set_value(&graph, id1, 3.0f);
    
    MathNodeId id2 = math_graph_add_node(&graph, MATH_NODE_VALUE);
    math_graph_set_value(&graph, id2, 5.0f);
    
    MathNodeId id_add = math_graph_add_node(&graph, MATH_NODE_ADD);
    
    math_graph_connect(&graph, id_add, 0, id1);
    math_graph_connect(&graph, id_add, 1, id2);
    
    char* glsl = math_graph_transpile_glsl(&graph, TRANSPILE_MODE_BUFFER_1D);
    TEST_ASSERT(glsl != NULL);
    
    printf("\n--- Generated GLSL ---\n%s\n----------------------\n", glsl);
    
    // Basic checks
    // Note: IDs might vary, but since it's the first test in a fresh arena, they should be 0, 1, 2.
    // However, transpiler output depends on node ID.
    char buf[128];
    snprintf(buf, 128, "float v_%d = 3.000000;", id1);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    snprintf(buf, 128, "float v_%d = 5.000000;", id2);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    snprintf(buf, 128, "float v_%d = v_%d + v_%d;", id_add, id1, id2);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    snprintf(buf, 128, "b_out.result = v_%d;", id_add);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    free(glsl);
    arena_destroy(&arena);
    return 1;
}

int main() {
    printf("Running Transpiler Tests...\n");
    RUN_TEST(test_transpiler_simple_add);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d tests failed!\n" TERM_RESET, g_tests_failed);
        return 1;
    } else {
        printf(TERM_GREEN "\nAll tests passed!\n" TERM_RESET);
        return 0;
    }
}