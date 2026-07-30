#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>
#include <string.h>

static _Atomic(int) g_value = 0;
static _Atomic(int) g_start = 0;

static int consumer_func(void *arg)
{
  (void)arg;
  while(!atomic_load_explicit(&g_start, memory_order_acquire))
    ;
  int expected = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_value, expected);
  return *(&g_value);
}

int benchmark_atomic_wait_test(void)
{
  (void)consumer_func;
  return 0;
}
