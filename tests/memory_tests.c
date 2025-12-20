#include "test_framework.h"
#include "foundation/memory/arena.h"
#include <string.h>

int test_arena_init_destroy(void) {
    MemoryArena arena;
    bool result = arena_init(&arena, 1024);
    ASSERT_TRUE(result);
    ASSERT_TRUE(arena.base != NULL);
    ASSERT_EQ_INT(arena.size, 1024);
    ASSERT_EQ_INT(arena.offset, 0);
    
    arena_destroy(&arena);
    ASSERT_TRUE(arena.base == NULL);
    ASSERT_EQ_INT(arena.size, 0);
    return 1;
}

int test_arena_alloc(void) {
    MemoryArena arena;
    arena_init(&arena, 128);

    void* p1 = arena_alloc(&arena, 64);
    ASSERT_TRUE(p1 != NULL);
    ASSERT_EQ_INT(arena.offset, 64);

    void* p2 = arena_alloc(&arena, 64);
    ASSERT_TRUE(p2 != NULL);
    ASSERT_EQ_INT(arena.offset, 128);

    // Should fail (OOM)
    void* p3 = arena_alloc(&arena, 1);
    ASSERT_TRUE(p3 == NULL);
    ASSERT_EQ_INT(arena.offset, 128);

    arena_destroy(&arena);
    return 1;
}

int test_arena_alloc_zero(void) {
    MemoryArena arena;
    arena_init(&arena, 128);

    int* ints = (int*)arena_alloc_zero(&arena, sizeof(int) * 10);
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ_INT(ints[i], 0);
    }

    arena_destroy(&arena);
    return 1;
}

int test_arena_reset(void) {
    MemoryArena arena;
    arena_init(&arena, 128);

    arena_alloc(&arena, 64);
    ASSERT_EQ_INT(arena.offset, 64);

    arena_reset(&arena);
    ASSERT_EQ_INT(arena.offset, 0);
    
    // Memory is still valid to alloc again
    void* p = arena_alloc(&arena, 128);
    ASSERT_TRUE(p != NULL);

    arena_destroy(&arena);
    return 1;
}

int test_arena_strings(void) {
    MemoryArena arena;
    arena_init(&arena, 256);

    const char* str = "Hello World";
    char* pushed = arena_push_string(&arena, str);
    ASSERT_TRUE(strcmp(pushed, str) == 0);
    ASSERT_EQ_INT(arena.offset, strlen(str) + 1);

    char* formatted = arena_sprintf(&arena, "Val: %d", 42);
    ASSERT_TRUE(strcmp(formatted, "Val: 42") == 0);

    arena_destroy(&arena);
    return 1;
}

int main(void) {
    TEST_INIT("Foundation Memory");
    
    TEST_RUN(test_arena_init_destroy);
    TEST_RUN(test_arena_alloc);
    TEST_RUN(test_arena_alloc_zero);
    TEST_RUN(test_arena_reset);
    TEST_RUN(test_arena_strings);
    
    TEST_REPORT();
    return 0;
}
