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

int test_transpiler_vec2_uv(void) {
    MemoryArena arena;
    arena_init(&arena, 1024 * 1024);
    
    MathGraph* graph = math_graph_create(&arena);
    
    // Create UV -> Output
    MathNodeId uv = math_graph_add_node(graph, MATH_NODE_UV);
    MathNodeId output = math_graph_add_node(graph, MATH_NODE_OUTPUT);
    math_graph_connect(graph, output, 0, uv);
    
    // Transpile
    char* glsl = math_graph_transpile(graph, TRANSPILE_MODE_BUFFER_1D, SHADER_TARGET_GLSL_VULKAN);
    TEST_ASSERT(glsl != NULL);
    
    printf("\n--- Generated GLSL (Vec2 UV) ---\n%s\n--------------------------------\n", glsl);
    
    // Checks:
    // 1. Output buffer should use vec2
    TEST_ASSERT(strstr(glsl, "vec2 result;") != NULL);
    
    // 2. UV variable should be vec2
    char buf[128];
    snprintf(buf, 128, "vec2 v_%d = uv;", uv);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    // 3. Result assignment
    snprintf(buf, 128, "b_out.result = v_%d;", uv);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    free(glsl);
    math_graph_destroy(graph);
    arena_destroy(&arena);
    return 1;
}

int test_transpiler_mouse(void) {
    MemoryArena arena;
    arena_init(&arena, 1024 * 1024);
    
    MathGraph* graph = math_graph_create(&arena);
    
    // Create Mouse -> Output
    MathNodeId mouse = math_graph_add_node(graph, MATH_NODE_MOUSE);
    MathNodeId output = math_graph_add_node(graph, MATH_NODE_OUTPUT);
    math_graph_connect(graph, output, 0, mouse);
    
    // Transpile
    char* glsl = math_graph_transpile(graph, TRANSPILE_MODE_BUFFER_1D, SHADER_TARGET_GLSL_VULKAN);
    TEST_ASSERT(glsl != NULL);
    
    printf("\n--- Generated GLSL (Mouse) ---\n%s\n------------------------------\n", glsl);
    
    // Checks:
    // 1. Output buffer should use vec4
    TEST_ASSERT(strstr(glsl, "vec4 result;") != NULL);
    
    // 2. Mouse variable should be vec4 and come from params.mouse
    char buf[128];
    snprintf(buf, 128, "vec4 v_%d = params.mouse;", mouse);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    // 3. Params struct should have mouse
    TEST_ASSERT(strstr(glsl, "vec4 mouse;") != NULL);
    
    free(glsl);
    math_graph_destroy(graph);
    arena_destroy(&arena);
    return 1;
}

int test_transpiler_texture_sample(void) {
    MemoryArena arena;
    arena_init(&arena, 1024 * 1024);
    
    MathGraph* graph = math_graph_create(&arena);
    
    // Create UV Node
    MathNodeId uv = math_graph_add_node(graph, MATH_NODE_UV);
    
    // Create Texture Param
    MathNodeId tex_param = math_graph_add_node(graph, MATH_NODE_TEXTURE_PARAM);
    math_graph_set_name(graph, tex_param, "MyTexture");
    
    // Create Sample Node
    MathNodeId sample = math_graph_add_node(graph, MATH_NODE_TEXTURE_SAMPLE);
    math_graph_connect(graph, sample, 0, tex_param); // Input 0: Texture
    math_graph_connect(graph, sample, 1, uv);        // Input 1: UV
    
    // Create Output
    MathNodeId output = math_graph_add_node(graph, MATH_NODE_OUTPUT);
    math_graph_connect(graph, output, 0, sample);
    
    // Transpile
    char* glsl = math_graph_transpile(graph, TRANSPILE_MODE_BUFFER_1D, SHADER_TARGET_GLSL_VULKAN);
    TEST_ASSERT(glsl != NULL);
    
    printf("\n--- Generated GLSL (Texture Sample) ---\n%s\n---------------------------------------\n", glsl);
    
    // Checks:
    // 1. Uniform Declaration
    // The name "MyTexture" is not currently used in transpiler V1 for generation, 
    // it uses ID. e.g. "uniform sampler2D u_tex_%d"
    char buf[128];
    snprintf(buf, 128, "layout(set=0, binding=1) uniform sampler2D u_tex_%d;", tex_param);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    // 2. Texture Sampling
    snprintf(buf, 128, "vec4 v_%d = texture(u_tex_%d, v_%d);", sample, tex_param, uv);
    TEST_ASSERT(strstr(glsl, buf) != NULL);
    
    free(glsl);
    math_graph_destroy(graph);
    arena_destroy(&arena);
    return 1;
}

int test_transpiler_c_generation(void) {
    MemoryArena arena;
    arena_init(&arena, 1024 * 1024);
    
    MathGraph* graph = math_graph_create(&arena);
    
    // Create Nodes: 3.0 + 5.0
    MathNodeId id1 = math_graph_add_node(graph, MATH_NODE_VALUE);
    math_graph_set_value(graph, id1, 3.0f);
    
    MathNodeId id2 = math_graph_add_node(graph, MATH_NODE_VALUE);
    math_graph_set_value(graph, id2, 5.0f);
    
    MathNodeId id_add = math_graph_add_node(graph, MATH_NODE_ADD);
    math_graph_connect(graph, id_add, 0, id1);
    math_graph_connect(graph, id_add, 1, id2);

    MathNodeId output = math_graph_add_node(graph, MATH_NODE_OUTPUT);
    math_graph_connect(graph, output, 0, id_add);
    
    // Transpile to C
    char* c_code = math_graph_transpile(graph, TRANSPILE_MODE_BUFFER_1D, SHADER_TARGET_C);
    TEST_ASSERT(c_code != NULL);
    
    printf("\n--- Generated C Code ---\n%s\n----------------------\n", c_code);
    
    // Checks
    TEST_ASSERT(strstr(c_code, "void execute_graph(void* out_buffer, GraphParams params)") != NULL);
    TEST_ASSERT(strstr(c_code, "typedef struct { float x, y; } vec2;") != NULL);
    
    char buf[128];
    snprintf(buf, 128, "float v_%d = f_add(v_%d, v_%d);", id_add, id1, id2);
    TEST_ASSERT(strstr(c_code, buf) != NULL);
    
    free(c_code);
    
    math_graph_destroy(graph);
    arena_destroy(&arena);
    return 1;
}

int main(void) {
    printf("Running Transpiler Tests...\n");
    RUN_TEST(test_transpiler_simple_add);
    RUN_TEST(test_transpiler_with_output_node);
    RUN_TEST(test_transpiler_vec2_uv);
    RUN_TEST(test_transpiler_mouse);
    RUN_TEST(test_transpiler_texture_sample);
    RUN_TEST(test_transpiler_c_generation);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d tests failed!\n" TERM_RESET, g_tests_failed);
        return 1;
    } else {
        printf(TERM_GREEN "\nAll tests passed!\n" TERM_RESET);
        return 0;
    }
}
