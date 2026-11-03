#include <unity.h>
#include "detect/schedule.h"
void setUp() {}
void tearDown() {}

// With focus on, half the visits go to the focus sweep; the normal rotation
// must still reach every slot and every rotation index.
static void test_focus_does_not_starve_rotations() {
  Scheduler s;
  bool seen[kSchedSlots][4] = {};
  int focus = 0;
  for (int i = 0; i < 400; i++) {
    SchedStep st = s.next(true);
    if (st.focus) { focus++; continue; }
    seen[st.slot][st.rot % 4] = true;
  }
  TEST_ASSERT_INT_WITHIN(2, 200, focus);
  for (int slot = 0; slot < kSchedSlots; slot++)
    for (int r = 0; r < 4; r++) TEST_ASSERT_TRUE_MESSAGE(seen[slot][r], "slot/rot never visited");
}
static void test_no_focus_cycles_all_slots_in_order() {
  Scheduler s;
  for (int i = 0; i < kSchedSlots * 3; i++) {
    SchedStep st = s.next(false);
    TEST_ASSERT_FALSE(st.focus);
    TEST_ASSERT_EQUAL(i % kSchedSlots, st.slot);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(i / kSchedSlots), st.rot);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_focus_does_not_starve_rotations);
  RUN_TEST(test_no_focus_cycles_all_slots_in_order);
  return UNITY_END();
}
