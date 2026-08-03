#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// Progress markers on stderr: ctest only echoes them on failure, so a hang is
// localisable to the exact part of the test that blocked.
#define SECTION(name) fprintf(stderr, "atomic_wait_test: " name "\n")

static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked = 0;

static int waiter_func(void *arg)
{
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *value =
  (WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *) arg;
  int expected = 0;
  atomic_store_explicit(&g_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(value, expected);
  return atomic_load_explicit(value, memory_order_acquire);
}

static int waiter_explicit_func(void *arg)
{
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *value =
  (WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *) arg;
  int expected = 0;
  atomic_store_explicit(&g_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit)(value, expected,
                                                 memory_order_acquire);
  return atomic_load_explicit(value, memory_order_acquire);
}

int atomic_wait_test(void)
{
  int ret = 0;
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int value = 0;

  // -- Waiter observes a store + notify without any sleep-based synchronisation
  SECTION("waiter observes store + notify");
  g_parked = 0;
  thrd_t thr;
  CHECK(thrd_create(&thr, waiter_func, &value) == thrd_success);
  test_wait_until("g_parked", &g_parked, 1);
  atomic_store_explicit(&value, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);

  int result = 0;
  CHECK(thrd_join(thr, &result) == thrd_success);
  CHECK(result == 1);

  // -- Same, via atomic_wait_explicit with a non-seq_cst order
  SECTION("waiter explicit with non-seq_cst order");
  g_parked = 0;
  atomic_store_explicit(&value, 0, memory_order_seq_cst);
  thrd_t thr2;
  CHECK(thrd_create(&thr2, waiter_explicit_func, &value) == thrd_success);
  test_wait_until("g_parked", &g_parked, 1);
  atomic_store_explicit(&value, 42, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);

  int result2 = 0;
  CHECK(thrd_join(thr2, &result2) == thrd_success);
  CHECK(result2 == 42);

  return ret;
}

int main(void)
{
  return atomic_wait_test();
}
