#include "test_framework.h"
#include "domains/math_model/math_scene.h"
#include "domains/math_model/math_mesh_builder.h"

// Test 1: Generate a simple plane z = x + y
int test_mesh_generation() {
    MathScene* scene = math_scene_create();
    
    // Variables x, y
    MathNode* var_x = math_scene_add_node(scene, MATH_NODE_VARIABLE, "x");
    var_x->data.variable.name = strdup("x");
    
    MathNode* var_y = math_scene_add_node(scene, MATH_NODE_VARIABLE, "y");
    var_y->data.variable.name = strdup("y");
    
    // Op: Add
    MathNode* op_add = math_scene_add_node(scene, MATH_NODE_OPERATOR, "Add");
    op_add->data.op.op_symbol = '+';
    
    math_scene_connect(var_x, op_add, 0);
    math_scene_connect(var_y, op_add, 1);
    
    // Visualizer
    MathNode* visual = math_scene_add_node(scene, MATH_NODE_VISUALIZER, "Plot");
    math_scene_connect(op_add, visual, 0);
    
    // Config
    MathMeshConfig config = {
        .grid_resolution_x = 2,
        .grid_resolution_y = 2,
        .range_x_min = 0.0f,
        .range_x_max = 2.0f,
        .range_y_min = 0.0f,
        .range_y_max = 2.0f
    };
    
    Mesh mesh = {0};
    bool success = math_mesh_build_surface(scene, visual, &config, &mesh);
    
    TEST_ASSERT(success);
    
    // Resolution 2x2 means 3x3 vertices = 9 vertices
    TEST_ASSERT_INT_EQ(9, mesh.position_count);
    
    // Check vertex at (0, 0). x=0, y=0 => z=0. Pos: 0, 0, 0
    // Index 0.
    TEST_ASSERT_FLOAT_EQ(0.0f, mesh.positions[0], 0.001f); // x
    TEST_ASSERT_FLOAT_EQ(0.0f, mesh.positions[1], 0.001f); // y (our z)
    TEST_ASSERT_FLOAT_EQ(0.0f, mesh.positions[2], 0.001f); // z (our y) -- wait, I mapped Z to [1] and Y to [2]
    // In builder: 
    // positions[1] = z
    // positions[2] = y
    
    // Check vertex at (2, 2). x=2, y=2 => z=4. 
    // Last vertex (index 8).
    // positions[8*3 + 0] = 2.0
    // positions[8*3 + 1] = 4.0
    // positions[8*3 + 2] = 2.0
    int last = 8 * 3;
    TEST_ASSERT_FLOAT_EQ(2.0f, mesh.positions[last + 0], 0.001f);
    TEST_ASSERT_FLOAT_EQ(4.0f, mesh.positions[last + 1], 0.001f);
    TEST_ASSERT_FLOAT_EQ(2.0f, mesh.positions[last + 2], 0.001f);

    // Cleanup
    free(mesh.positions);
    free(mesh.indices);
    math_scene_dispose(scene);
    
    return 1;
}

int main() {
    printf("Running Math Mesh Tests...\n");
    RUN_TEST(test_mesh_generation);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d tests failed!\n" TERM_RESET, g_tests_failed);
        return 1;
    } else {
        printf(TERM_GREEN "\nAll tests passed!\n" TERM_RESET);
        return 0;
    }
}
