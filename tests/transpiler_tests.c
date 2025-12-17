#include "test_framework.h"
#include "domains/math_model/math_graph.h"
#include "domains/math_model/transpiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_transpiler_simple_add() {
    MathGraph graph;
    math_graph_init(&graph);
    
    // Create nodes: 3.0 + 5.0
    MathNode* n1 = math_graph_add_node(&graph, MATH_NODE_VALUE);
    n1->value = 3.0f;
    n1->id = 1; // Force ID for deterministic output
    
    MathNode* n2 = math_graph_add_node(&graph, MATH_NODE_VALUE);
    n2->value = 5.0f;
    n2->id = 2;
    
    MathNode* n_add = math_graph_add_node(&graph, MATH_NODE_ADD);
    n_add->id = 3;
    
    math_graph_connect(n_add, 0, n1);
    math_graph_connect(n_add, 1, n2);
    
    char* glsl = math_graph_transpile_glsl(&graph, TRANSPILE_MODE_BUFFER_1D);
    TEST_ASSERT(glsl != NULL);
    
    printf("\n--- Generated GLSL ---\n%s\n----------------------\n", glsl);
    
    // Basic checks
    TEST_ASSERT(strstr(glsl, "float v_1 = 3.000000;") != NULL);
    TEST_ASSERT(strstr(glsl, "float v_2 = 5.000000;") != NULL);
    TEST_ASSERT(strstr(glsl, "float v_3 = v_1 + v_2;") != NULL);
    TEST_ASSERT(strstr(glsl, "b_out.result = v_3;") != NULL);
    
    free(glsl);
    math_graph_dispose(&graph);
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
