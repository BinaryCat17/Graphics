#include "test_framework.h"
#include "services/scene/math_scene.h"

#define M_PI 3.14159265358979323846

// Test 1: Basic creation and destruction
int test_math_scene_lifecycle() {
    MathScene* scene = math_scene_create();
    TEST_ASSERT(scene != NULL);
    TEST_ASSERT_INT_EQ(0, scene->node_count);
    
    math_scene_dispose(scene);
    return 1;
}

// Test 2: Simple arithmetic (Constant + Constant)
int test_math_basic_arithmetic() {
    MathScene* scene = math_scene_create();
    
    // Create nodes: 5.0 + 3.0
    MathNode* n1 = math_scene_add_node(scene, MATH_NODE_CONSTANT, "C1");
    n1->data.constant.value = 5.0f;
    
    MathNode* n2 = math_scene_add_node(scene, MATH_NODE_CONSTANT, "C2");
    n2->data.constant.value = 3.0f;
    
    MathNode* op = math_scene_add_node(scene, MATH_NODE_OPERATOR, "Add");
    op->data.op.op_symbol = '+';
    
    // Connect: op input 0 <- n1, op input 1 <- n2
    math_scene_connect(n1, op, 0);
    math_scene_connect(n2, op, 1);
    
    float result = math_node_eval(op);
    TEST_ASSERT_FLOAT_EQ(8.0f, result, 0.0001f);
    
    math_scene_dispose(scene);
    return 1;
}

// Test 3: Function evaluation (sin(t)) with time update
int test_math_function_time() {
    MathScene* scene = math_scene_create();
    scene->is_playing = true;
    
    // Variable 't'
    MathNode* var_t = math_scene_add_node(scene, MATH_NODE_VARIABLE, "t");
    var_t->data.variable.name = strdup("t");
    var_t->data.variable.current_value = 0.0f;
    
    // Function 'sin'
    MathNode* func_sin = math_scene_add_node(scene, MATH_NODE_FUNCTION, "SinFunc");
    func_sin->data.func.func_name = strdup("sin");
    
    // Connect: sin(t)
    math_scene_connect(var_t, func_sin, 0);
    
    // Time = 0, sin(0) = 0
    math_scene_update(scene, 0.0f); 
    TEST_ASSERT_FLOAT_EQ(0.0f, math_node_eval(func_sin), 0.0001f);
    
    // Advance time by PI/2
    math_scene_update(scene, (float)(M_PI / 2.0));
    // t should be PI/2 now
    TEST_ASSERT_FLOAT_EQ((float)(M_PI / 2.0), var_t->data.variable.current_value, 0.001f);
    
    // sin(PI/2) should be 1.0
    TEST_ASSERT_FLOAT_EQ(1.0f, math_node_eval(func_sin), 0.0001f);
    
    math_scene_dispose(scene);
    return 1;
}

int main() {
    printf("Running Math Scene Tests...\n");
    
    RUN_TEST(test_math_scene_lifecycle);
    RUN_TEST(test_math_basic_arithmetic);
    RUN_TEST(test_math_function_time);
    
    if (g_tests_failed > 0) {
        printf(TERM_RED "\n%d tests failed!\n" TERM_RESET, g_tests_failed);
        return 1;
    } else {
        printf(TERM_GREEN "\nAll tests passed!\n" TERM_RESET);
        return 0;
    }
}
