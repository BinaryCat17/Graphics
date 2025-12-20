#include "test_framework.h"
#include "features/math_engine/math_graph.h"
#include "features/math_engine/internal/transpiler.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_transpiler_simple_add(void) {
    MathGraph graph;
    // Arena no longer needed for MathGraph initialization
    // MemoryArena arena;
    // arena_init(&arena, 1024 * 1024);
    
    math_graph_init(&graph, NULL); // Pass NULL as arena is unused
    
    // Create nodes: 3.0 + 5.0
    MathNodeId id1 = math_graph_add_node(&graph, MATH_NODE_VALUE);
    math_graph_set_value(&graph, id1, 3.0f);
    
    MathNodeId id2 = math_graph_add_node(&graph, MATH_NODE_VALUE);
    math_graph_set_value(&graph, id2, 5.0f);
    
    MathNodeId id_add = math_graph_add_node(&graph, MATH_NODE_ADD);
    
    math_graph_connect(&graph, id_add, 0, id1);
    math_graph_connect(&graph, id_add, 1, id2);
    
    // Transpile
    char* glsl = math_graph_transpile(&graph, TRANSPILE_MODE_BUFFER_1D, SHADER_TARGET_GLSL_VULKAN);
    TEST_ASSERT(glsl != NULL);
    
    printf("\n--- Generated GLSL ---\n%s\n----------------------\n", glsl);
    
    // Basic checks
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
    math_graph_clear(&graph); // Clean up pool
    // arena_destroy(&arena);
    return 1;
}

int main(void) {
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
