#include <unity.h>
void setUp() {}
void tearDown() {}
static void test_sanity() { TEST_ASSERT_EQUAL(1, 1); }
int main() { UNITY_BEGIN(); RUN_TEST(test_sanity); return UNITY_END(); }
