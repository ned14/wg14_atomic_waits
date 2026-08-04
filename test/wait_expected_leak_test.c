#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// §1.3 (analysis): a timed-out / errored `atomic_wait_expected` on the
// hash-table fallback path must release its proxy node. The regression is
// observable through the public API: a notify on an address with no registered
// proxy node wakes nobody. How that surfaces in `atomic_notify`'s return
// differs by backend — the hash-table backend reports "1 + 0 woken" (1), the
// Linux futex backend reports the real woken count (1 + 0), a native backend
// whose kernel wake fails on an unwaited address (macOS) reports a negative
// no-waiter error, and a native backend whose kernel wake cannot report a
// woken count (Windows, FreeBSD) fabricates "1 woken" (2). A *retained* node
// instead makes the hash-table backend fabricate an extra wake. The
// discriminator is therefore `nr > baseline`, where `baseline` is the return
// of a notify on a never-waited control address: every leaked node makes a
// waited-on address report exactly one more woken thread than the control, and
// that comparison works on every backend.
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
  // Baseline: a notify on an address that was never waited on. Its return
  // captures the backend's convention for "no registered proxy node". Every
  // proxy node must have been released, so notifying each waited-on address
  // must report no more woken threads than this control. A retained (leaked)
  // node makes the hash-table backend fabricate an extra wake (nr > baseline).
  static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t
  control;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) base_expected = 0;
  const int baseline = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
  &control, &base_expected, 1, 1, memory_order_seq_cst, memory_order_seq_cst);
  for(int i = 0; i < LEAK_N; i++)
  {
    WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) en = 0;
    const int nr = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
    &objs[i], &en, 1, 1, memory_order_seq_cst, memory_order_seq_cst);
    if(nr > baseline)
    {
      fprintf(stderr,
              "FATAL: timed-out wait leaked a proxy node: atomic_notify on "
              "address %d returned %d after a timed-out wait (a leaked node "
              "fabricates a wake; baseline %d)\n",
              i, nr, baseline);
      return ret + 1;
    }
  }
  return ret;
}

int main(void)
{
  return wait_expected_leak_test_main();
}
