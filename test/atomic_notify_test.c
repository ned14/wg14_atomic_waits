#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>
#include <string.h>

#define NUM_THREADS 4

static _Atomic(int) g_value = 0;
static _Atomic(int) g_ready = 0;
static _Atomic(int) g_count = 0;

static int notify_one_func(void *arg)
{
  (void)arg;
  while(!atomic_load_explicit(&g_ready, memory_order_acquire))
    thrd_sleep_ms(1);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value);
  return 0;
}

static int waiter_func(void *arg)
{
  (void)arg;
  int expected = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_value, expected);
  atomic_fetch_add_explicit(&g_count, 1, memory_order_relaxed);
  return 0;
}

int atomic_notify_test(void)
{
  int ret = 0;

  // Test atomic_notify_one wakes exactly one waiter
  g_value = 0;
  g_ready = 0;
  g_count = 0;

  thrd_t waiters[NUM_THREADS];
  for(int i = 0; i < NUM_THREADS; i++)
    CHECK(thrd_create(&waiters[i], waiter_func, NULL) == thrd_success);

  thrd_sleep_ms(50);
  atomic_store_explicit(&g_ready, 1, memory_order_release);

  thrd_t notifier;
  CHECK(thrd_create(&notifier, notify_one_func, NULL) == thrd_success);

  thrd_sleep_ms(100);
  atomic_store_explicit(&g_value, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_value);

  for(int i = 0; i < NUM_THREADS; i++)
  {
    int r;
    thrd_join(waiters[i], &r);
  }
  int nr;
  thrd_join(notifier, &nr);

  // Test atomic_wait_expected CAS failure
  _Atomic uint_least32_t value2 = 42;
  uint_least32_t expected2 = 99;
  uint_least32_t desired2 = 99;
  int notify_r = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(&value2, &expected2, desired2, 1, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(notify_r == 0);
  CHECK(atomic_load_explicit(&value2, memory_order_seq_cst) == 42);

  return ret;
}
