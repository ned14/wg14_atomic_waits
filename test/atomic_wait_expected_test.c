#include "test_common.h"
#include <string.h>
#include <wg14_atomic_waits/atomic_wait.h>

int atomic_wait_expected_test_main(void)
{
  int ret = 0;
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t value32 = 0;
  uint_least32_t expected32 = 0;

  // Immediate return - value != expected
  expected32 = 1;
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value32, &expected32, WG14_ATOMIC_WAITS_NULLPTR, memory_order_seq_cst,
  memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected32 == 0);

  // Wait with zero timeout, should always time out
  struct timespec dur = {.tv_sec = 0, .tv_nsec = 0};
  expected32 = 0;
  r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value32, &expected32, &dur, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(r == 0);  // timed out, no suspension
  CHECK(expected32 == 0);

  // Wait with a real (non-zero) duration and no notify: must suspend, wait for
  // the full duration (ceiling), then return 0 on timeout with *expected
  // unchanged. This exercises the timed kernel wait path (Linux/FreeBSD/
  // macOS/pthreads) which previously mis-reported a clean timeout as an error.
  {
    const unsigned timeout_ms = 50;
    struct timespec start, fin, d = {.tv_sec = 0, .tv_nsec = 0};
    d.tv_nsec = (long) timeout_ms * 1000000L;
    expected32 = 0;
    timespec_get(&start, TIME_UTC);
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value32, &expected32, &d, memory_order_seq_cst, memory_order_seq_cst);
    timespec_get(&fin, TIME_UTC);
    CHECK(r == 0);  // clean timeout, not an error
    CHECK(expected32 == 0);
    const long elapsed_ms =
    (long) ((fin.tv_sec - start.tv_sec) * 1000L +
            (fin.tv_nsec - start.tv_nsec) / 1000000L);
    // Must not return before the requested duration has elapsed.
    CHECK(elapsed_ms >= (long) timeout_ms);
  }

  // CAS failure returns 0
  atomic_store_explicit(&value32, 5, memory_order_seq_cst);
  expected32 = 10;
  uint_least32_t desired = 20;
  int nr = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
  &value32, &expected32, desired, 1, memory_order_seq_cst,
  memory_order_seq_cst);
  CHECK(nr == 0);
  CHECK(expected32 == 5);

  return ret;
}

int main(void)
{
  return atomic_wait_expected_test_main();
}
