#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>
#include <string.h>

static int waiter_func(void *arg)
{
  _Atomic(int) *value = (_Atomic(int) *)arg;
  int expected = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(value, expected);
  return *value;
}

static int waiter_explicit_func(void *arg)
{
  _Atomic(int) *value = (_Atomic(int) *)arg;
  int expected = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit)(value, expected, memory_order_seq_cst);
  return *value;
}

int atomic_wait_test(void)
{
  int ret = 0;
  _Atomic int value = 0;

  thrd_t thr;
  CHECK(thrd_create(&thr, waiter_func, &value) == thrd_success);

  thrd_sleep_ms(50);
  atomic_store_explicit(&value, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);

  int result = 0;
  CHECK(thrd_join(thr, &result) == thrd_success);
  CHECK(result == 1);

  thrd_sleep_ms(50);
  atomic_store_explicit(&value, 0, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);

  thrd_t thr2;
  CHECK(thrd_create(&thr2, waiter_explicit_func, &value) == thrd_success);

  thrd_sleep_ms(50);
  atomic_store_explicit(&value, 42, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);

  int result2 = 0;
  CHECK(thrd_join(thr2, &result2) == thrd_success);
  CHECK(result2 == 42);

  return ret;
}
