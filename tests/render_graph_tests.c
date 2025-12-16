#include "test_framework.h"
#include "services/render/render_graph/render_graph.h"

int test_rg_creation() {
    RgGraph* graph = rg_create();
    TEST_ASSERT(graph != NULL);
    rg_destroy(graph);
    return 1;
}

int test_rg_topology() {
    RgGraph* graph = rg_create();
    
    // Create resources
    RgResourceHandle tex1 = rg_create_texture(graph, "Tex1", 800, 600, RG_FORMAT_R8G8B8A8_UNORM);
    RgResourceHandle tex2 = rg_create_texture(graph, "Tex2", 800, 600, RG_FORMAT_R8G8B8A8_UNORM);
    
    TEST_ASSERT(tex1 != 0);
    TEST_ASSERT(tex2 != 0);
    TEST_ASSERT(tex1 != tex2);
    
    // Pass 1: Writes Tex1
    RgPassBuilder* p1 = rg_add_pass(graph, "Pass1", 0, NULL);
    TEST_ASSERT(p1 != NULL);
    rg_pass_write(p1, tex1, RG_LOAD_OP_CLEAR, RG_STORE_OP_STORE);
    
    // Pass 2: Reads Tex1, Writes Tex2
    RgPassBuilder* p2 = rg_add_pass(graph, "Pass2", 0, NULL);
    rg_pass_read(p2, tex1);
    rg_pass_write(p2, tex2, RG_LOAD_OP_DONT_CARE, RG_STORE_OP_STORE);
    
    // Compile
    bool ok = rg_compile(graph);
    TEST_ASSERT(ok);
    
    rg_destroy(graph);
    return 1;
}

int main() {
    printf("Running Render Graph Tests...\n");
    RUN_TEST(test_rg_creation);
    RUN_TEST(test_rg_topology);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d tests failed!\n" TERM_RESET, g_tests_failed);
        return 1;
    } else {
        printf(TERM_GREEN "\nAll tests passed!\n" TERM_RESET);
        return 0;
    }
}
