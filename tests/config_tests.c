#include "test_framework.h"
#include "foundation/config/config_system.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/memory/arena.h"
#include "foundation/meta/reflection.h"
#include "foundation/string/string_id.h"

// --- Mock Data ---

typedef struct TestNode {
    int id;
    float value;
    char* name;
    struct TestNode** children;
    size_t child_count;
} TestNode;

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

// --- Mock Registry ---

const MetaStruct* meta_get_struct(const char* name) {
    if (strcmp(name, "TestNode") == 0) return &test_node_meta;
    return NULL;
}

const MetaEnum* meta_get_enum(const char* name) {
    (void)name;
    return NULL;
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

int main(void) {
    TEST_INIT("Config Deserializer");
    TEST_RUN(test_simple_struct);
    TEST_RUN(test_nested_array);
    TEST_REPORT();
}
