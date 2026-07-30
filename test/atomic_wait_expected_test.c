#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>
#include <string.h>

int atomic_wait_expected_test_main(void)
{
  int ret = 0;
  _Atomic uint_least32_t value32 = 0;
  uint_least32_t expected32 = 0;

  // Immediate return - value != expected
  expected32 = 1;
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(&value32, &expected32, WG14_ATOMIC_WAITS_NULLPTR, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected32 == 0);

  // Wait with tiny timeout, should time out
  struct timespec dur = {.tv_sec = 0, .tv_nsec = 1};
  expected32 = 0;
  r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(&value32, &expected32, &dur, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(r == 0); // timed out, no suspension
  CHECK(expected32 == 0);

  // CAS failure returns 0
  atomic_store_explicit(&value32, 5, memory_order_seq_cst);
  expected32 = 10;
  uint_least32_t desired = 20;
  int nr = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(&value32, &expected32, desired, 1, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(nr == 0);
  CHECK(expected32 == 5);

  return ret;
}
