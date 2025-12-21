#include "test_framework.h"
#include "foundation/config/config_system.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/memory/arena.h"
#include "foundation/meta/reflection.h"
#include "foundation/math/math_types.h"
#include "foundation/string/string_id.h"

// --- Mock Data ---

typedef struct TestNode {
    int id;
    float value;
    char* name;
    struct TestNode** children;
    size_t child_count;
} TestNode;

typedef struct TestColor {
    Vec4 color;
} TestColor;

static MetaField test_node_fields[] = {
    { "id", META_TYPE_INT, offsetof(TestNode, id), "int" },
    { "value", META_TYPE_FLOAT, offsetof(TestNode, value), "float" },
    { "name", META_TYPE_STRING, offsetof(TestNode, name), "string" },
    { "children", META_TYPE_POINTER_ARRAY, offsetof(TestNode, children), "TestNode" },
    { "child_count", META_TYPE_INT, offsetof(TestNode, child_count), "int" },
};

static MetaStruct test_node_meta = {
    "TestNode",
    sizeof(TestNode),
    test_node_fields,
    5
};

static MetaField test_color_fields[] = {
    { "color", META_TYPE_VEC4, offsetof(TestColor, color), "Vec4" },
};

static MetaStruct test_color_meta = {
    "TestColor",
    sizeof(TestColor),
    test_color_fields,
    1
};

// --- Mock Registry ---

const MetaStruct* meta_get_struct(const char* name) {
    if (strcmp(name, "TestNode") == 0) return &test_node_meta;
    if (strcmp(name, "TestColor") == 0) return &test_color_meta;
    return NULL;
}

const MetaEnum* meta_get_enum(const char* name) {
    (void)name;
    return NULL;
}

bool meta_enum_get_value(const MetaEnum* meta_enum, const char* name_str, int* out_value) {
    (void)meta_enum;
    (void)name_str;
    (void)out_value;
    return false;
}

// --- Tests ---

int test_simple_struct(void) {
    MemoryArena arena;
    arena_init(&arena, 1024);

    const char* yaml = 
        "id: 42\n"
        "value: 3.14\n"
        "name: \"Hello\"\n";
    
    ConfigNode* root;
    ConfigError err;
    int res = simple_yaml_parse(&arena, yaml, &root, &err);
    TEST_ASSERT(res);

    TestNode node = {0};
    bool ok = config_load_struct(root, &test_node_meta, &node, &arena);
    
    TEST_ASSERT(ok);
    ASSERT_EQ_INT(42, node.id);
    ASSERT_EQ_FLOAT(3.14f, node.value, 0.001f);
    ASSERT_STR_EQ("Hello", node.name);

    arena_destroy(&arena);
    return 1;
}

int test_nested_array(void) {
    MemoryArena arena;
    arena_init(&arena, 4096);

    const char* yaml = 
        "id: 1\n"
        "name: \"Root\"\n"
        "children:\n"
        "  - id: 2\n"
        "    name: \"Child A\"\n"
        "  - id: 3\n"
        "    name: \"Child B\"\n";
    
    ConfigNode* root;
    ConfigError err;
    int res = simple_yaml_parse(&arena, yaml, &root, &err);
    TEST_ASSERT(res);

    TestNode node = {0};
    bool ok = config_load_struct(root, &test_node_meta, &node, &arena);
    
    TEST_ASSERT(ok);
    ASSERT_EQ_INT(1, node.id);
    ASSERT_STR_EQ("Root", node.name);
    
    TEST_ASSERT(node.child_count == 2);
    TEST_ASSERT(node.children != NULL);
    
    TestNode* c1 = node.children[0];
    TestNode* c2 = node.children[1];
    
    TEST_ASSERT(c1 != NULL);
    TEST_ASSERT(c2 != NULL);
    
    ASSERT_EQ_INT(2, c1->id);
    ASSERT_STR_EQ("Child A", c1->name);
    
    ASSERT_EQ_INT(3, c2->id);
    ASSERT_STR_EQ("Child B", c2->name);

    arena_destroy(&arena);
    return 1;
}

int test_hex_color(void) {
    MemoryArena arena;
    arena_init(&arena, 1024);

    const char* yaml = "color: \"#FF0000FF\"\n"; // Red fully opaque
    
    ConfigNode* root;
    ConfigError err;
    int res = simple_yaml_parse(&arena, yaml, &root, &err);
    TEST_ASSERT(res);

    TestColor obj = {0};
    bool ok = config_load_struct(root, &test_color_meta, &obj, &arena);
    
    TEST_ASSERT(ok);
    // x=r, y=g, z=b, w=a
    ASSERT_EQ_FLOAT(1.0f, obj.color.x, 0.001f);
    ASSERT_EQ_FLOAT(0.0f, obj.color.y, 0.001f);
    ASSERT_EQ_FLOAT(0.0f, obj.color.z, 0.001f);
    ASSERT_EQ_FLOAT(1.0f, obj.color.w, 0.001f);

    arena_destroy(&arena);
    return 1;
}

int main(void) {
    TEST_INIT("Config Deserializer");
    TEST_RUN(test_simple_struct);
    TEST_RUN(test_nested_array);
    TEST_RUN(test_hex_color);
    TEST_REPORT();
}
