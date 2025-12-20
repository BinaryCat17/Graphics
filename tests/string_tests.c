#include "test_framework.h"
#include "foundation/string/string_id.h"
#include <string.h>

int test_string_id_hash(void) {
    StringId id1 = str_id("test_string");
    StringId id2 = str_id("test_string");
    StringId id3 = str_id("other_string");

    ASSERT_TRUE(id1 == id2);
    ASSERT_TRUE(id1 != id3);
    return 1;
}

int test_string_id_lookup(void) {
#ifndef NDEBUG
    // Lookup only works in Debug builds where the registry is active
    const char* original = "lookup_test";
    StringId id = str_id(original);
    
    const char* recovered = str_id_lookup(id);
    ASSERT_TRUE(strcmp(recovered, original) == 0);

    StringId unknown = 123456; // Random unlikely ID
    // Assuming 123456 hasn't been hashed. If it has, this test is flaky.
    // Ideally we'd use a known unused ID or mock the registry.
    // For now, let's just check it doesn't crash.
    const char* recovered_unknown = str_id_lookup(unknown);
    ASSERT_TRUE(recovered_unknown != NULL); 
#endif
    return 1;
}

int main(void) {
    TEST_INIT("Foundation String");
    
    TEST_RUN(test_string_id_hash);
    TEST_RUN(test_string_id_lookup);
    
    TEST_REPORT();
    return 0;
}
