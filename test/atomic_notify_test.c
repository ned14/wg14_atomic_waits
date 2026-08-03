#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// Progress markers on stderr: ctest only echoes them on failure, so a hang is
// localisable to the exact part of the test that blocked.
#define SECTION(name) fprintf(stderr, "atomic_notify_test: " name "\n")

#define NUM_THREADS 4

static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t g_value8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_pcount = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_woke = 0;

static int waiter_func(void *arg)
{
  (void) arg;
  uint_least8_t expected = 0;
  atomic_fetch_add_explicit(&g_pcount, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_value8, (int) expected);
  atomic_fetch_add_explicit(&g_woke, 1, memory_order_relaxed);
  return 0;
}

int atomic_notify_test(void)
{
  int ret = 0;
  thrd_t waiters[NUM_THREADS];

  // Park all waiters, then wake at least one with atomic_notify_one.
  SECTION("wake one waiter via atomic_notify_one");
  g_value8 = 0;
  g_pcount = 0;
  g_woke = 0;
  for(int i = 0; i < NUM_THREADS; i++)
  {
    CHECK(thrd_create(&waiters[i], waiter_func, NULL) == thrd_success);
  }
  test_wait_until("g_pcount", &g_pcount, NUM_THREADS);

  // Change the value so that waiters woken by a notify actually return from
  // atomic_wait (otherwise a woken waiter re-parks with the value still equal).
  //
  // We deliberately do NOT assert that a bare store leaves every parked waiter
  // asleep. atomic_wait may return on ANY backend after the value changes: a
  // waiter can still be inside its load/re-check/park window when the store
  // lands (so it observes the new value), or can be woken by a spurious wake.
  // The proposal imposes no "store does not wake" guarantee, so no negative
  // store-wake assertion is made.
  atomic_store_explicit(&g_value8, 1, memory_order_seq_cst);

  // atomic_notify_one must wake at least one parked waiter. (A portable test
  // cannot assert "exactly one": the store above may already have released a
  // not-yet-parked waiter, and spurious wakes are permitted.)
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value8);
  test_wait_until("g_woke", &g_woke, 1);

  // atomic_notify_all must wake every remaining parked waiter.
  SECTION("atomic_notify_all wakes every remaining waiter");
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_value8);
  test_wait_until("g_woke", &g_woke, NUM_THREADS);
  CHECK(atomic_load_explicit(&g_woke, memory_order_acquire) == NUM_THREADS);

  for(int i = 0; i < NUM_THREADS; i++)
  {
    int r;
    CHECK(thrd_join(waiters[i], &r) == thrd_success);
    CHECK(r == 0);
  }

  // atomic_wait_expected CAS failure
  SECTION("atomic_notify CAS failure");
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t value2 = 42;
  uint_least32_t expected2 = 99;
  uint_least32_t desired2 = 99;
  int notify_r =
  atomic_notify(&value2, &expected2, desired2, 1,
                WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst,
                WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);
  CHECK(notify_r == 0);
  CHECK(atomic_load_explicit(&value2, memory_order_seq_cst) == 42);

  return ret;
}

int main(void)
{
  return atomic_notify_test();
}
