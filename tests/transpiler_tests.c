#include "test_framework.h"
#include "features/math_engine/math_graph.h"
#include "features/math_engine/internal/transpiler.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_transpiler_simple_add(void) {
    MemoryArena arena;
    arena_init(&arena, 1024 * 1024);
    
    MathGraph* graph = math_graph_create(&arena);
    TEST_ASSERT(graph != NULL);
    
    // Create nodes: 3.0 + 5.0
    MathNodeId id1 = math_graph_add_node(graph, MATH_NODE_VALUE);
    math_graph_set_value(graph, id1, 3.0f);
    
    MathNodeId id2 = math_graph_add_node(graph, MATH_NODE_VALUE);
    math_graph_set_value(graph, id2, 5.0f);
    
    MathNodeId id_add = math_graph_add_node(graph, MATH_NODE_ADD);
    
    math_graph_connect(graph, id_add, 0, id1);
    math_graph_connect(graph, id_add, 1, id2);

    // Create Output Node
    MathNodeId output = math_graph_add_node(graph, MATH_NODE_OUTPUT);
    math_graph_connect(graph, output, 0, id_add);
    
    // Transpile
    char* glsl = math_graph_transpile(graph, TRANSPILE_MODE_BUFFER_1D, SHADER_TARGET_GLSL_VULKAN);
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
    
    math_graph_destroy(graph);
    arena_destroy(&arena);
    return 1;
}

int test_transpiler_with_output_node(void) {
    MemoryArena arena;
    arena_init(&arena, 1024 * 1024);
    
    MathGraph* graph = math_graph_create(&arena);
    TEST_ASSERT(graph != NULL);
    
    // 1. Create a "dead" node (unconnected)
    MathNodeId dead_node = math_graph_add_node(graph, MATH_NODE_VALUE);
    math_graph_set_value(graph, dead_node, 999.0f);
    
    // 2. Create a connected chain: 10.0 + 20.0 -> Output
    MathNodeId val1 = math_graph_add_node(graph, MATH_NODE_VALUE);
    math_graph_set_value(graph, val1, 10.0f);
    
    MathNodeId val2 = math_graph_add_node(graph, MATH_NODE_VALUE);
    math_graph_set_value(graph, val2, 20.0f);
    
    MathNodeId add = math_graph_add_node(graph, MATH_NODE_ADD);
    math_graph_connect(graph, add, 0, val1);
    math_graph_connect(graph, add, 1, val2);
    
    MathNodeId output = math_graph_add_node(graph, MATH_NODE_OUTPUT);
    math_graph_connect(graph, output, 0, add); // Connect add to Output
    
    // Transpile
    char* glsl = math_graph_transpile(graph, TRANSPILE_MODE_BUFFER_1D, SHADER_TARGET_GLSL_VULKAN);
    TEST_ASSERT(glsl != NULL);
    
    printf("\n--- Generated GLSL (With Output Node) ---\n%s\n--------------------------------------\n", glsl);
    
    // Checks:
    char buf[128];
    
    // Connected nodes should be present
    snprintf(buf, 128, "float v_%d = 10.000000;", val1);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    snprintf(buf, 128, "float v_%d = v_%d + v_%d;", add, val1, val2);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    // Result should point to the 'add' node (input of output node)
    snprintf(buf, 128, "b_out.result = v_%d;", add);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    // Dead node should NOT be present (Dead Code Elimination)
    snprintf(buf, 128, "float v_%d = 999.000000;", dead_node);
    TEST_ASSERT(strstr(glsl, buf) == NULL);
    
    free(glsl);
    math_graph_destroy(graph);
    arena_destroy(&arena);
    return 1;
}

int main(void) {
    printf("Running Transpiler Tests...\n");
    RUN_TEST(test_transpiler_simple_add);
    RUN_TEST(test_transpiler_with_output_node);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d tests failed!\n" TERM_RESET, g_tests_failed);
        return 1;
    } else {
        printf(TERM_GREEN "\nAll tests passed!\n" TERM_RESET);
        return 0;
    }
}
