#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// §1.3 (analysis): a timed-out / errored `atomic_wait_expected` on the
// hash-table fallback path must release its proxy node. The regression is
// observable through the public API: a notify on an address with no registered
// proxy node wakes nobody, so it reports "1 + 0 woken" (returns 1) — or a
// negative no-waiter error on a native backend whose kernel wake fails on an
// unwaited address. A *retained* node instead makes the hash-table backend
// fabricate a wake (returns 2 or more). So the discriminator is `ret >= 2`:
// only a leaked node can produce it, and it works on every backend.
#define LEAK_N 20000

int wait_expected_leak_test_main(void)
{
  int ret = 0;
  static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t
  objs[LEAK_N];
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  // Negative duration: `atomic_wait_expected` must create the proxy node and
  // then time out immediately (returning 0 with *expected unchanged).
  struct timespec d = {.tv_sec = -1, .tv_nsec = 0};
  for(int i = 0; i < LEAK_N; i++)
  {
    expected = 0;
    const int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &objs[i], &expected, &d, memory_order_seq_cst, memory_order_seq_cst);
    CHECK(r == 0);
    CHECK(expected == 0);
  }
  // Every proxy node must have been released. A retained (leaked) node makes
  // the hash-table backend report a fabricated wake (return >= 2).
  for(int i = 0; i < LEAK_N; i++)
  {
    WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) en = 0;
    const int nr = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
    &objs[i], &en, 1, 1, memory_order_seq_cst, memory_order_seq_cst);
    if(nr >= 2)
    {
      fprintf(stderr,
              "FATAL: timed-out wait leaked a proxy node: atomic_notify on "
              "address %d returned %d after a timed-out wait (a leaked node "
              "fabricates a wake; expected <= 1)\n",
              i, nr);
      return ret + 1;
    }
  }
  return ret;
}

int main(void)
{
  return wait_expected_leak_test_main();
}
