#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// --- Immediate return when *object != expected (no suspension) ---
static int immediate_return_test(void)
{
  int ret = 0;
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int value = 5;
  // *value (5) != expected (0): must return immediately without suspending.
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&value, 0);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit)(&value, 0,
                                                 memory_order_acquire);
  return ret;
}

// --- Spurious wake while parked (value unchanged) must re-park, not return ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int spur_val = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int spur_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int spur_returned = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int spur_observed = 0;

static int spur_waiter(void *arg)
{
  (void) arg;
  atomic_store_explicit(&spur_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&spur_val, 0);
  atomic_store_explicit(&spur_returned, 1, memory_order_release);
  atomic_store_explicit(&spur_observed,
                        atomic_load_explicit(&spur_val, memory_order_acquire),
                        memory_order_release);
  return 0;
}

static void wait_flag(const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *flag)
{
  struct timespec start;
  timespec_get(&start, TIME_UTC);
  while(atomic_load_explicit(flag, memory_order_acquire) == 0)
  {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    const long ms = (long) ((now.tv_sec - start.tv_sec) * 1000L +
                            (now.tv_nsec - start.tv_nsec) / 1000000L);
    if(ms > 2000)
    {
      return;
    }
    thrd_sleep_ms(1);
  }
}

// A notify that lands while the waiter is parked, but with the value unchanged,
// must wake it, make it re-compare, re-park, and only return once the value
// really differs.
static int spurious_wake_test(void)
{
  int ret = 0;
  spur_val = 0;
  spur_parked = 0;
  spur_returned = 0;
  spur_observed = 0;
  thrd_t thr;
  CHECK(thrd_create(&thr, spur_waiter, NULL) == thrd_success);
  wait_flag(&spur_parked);
  CHECK(spur_parked == 1);

  // Spurious notify: value still equals expected (0), so the waiter must
  // re-park.
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&spur_val);
  thrd_sleep_ms(30);
  CHECK(atomic_load_explicit(&spur_returned, memory_order_acquire) == 0);

  // Now the value really changes (store + notify pair).
  atomic_store_explicit(&spur_val, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&spur_val);
  int wr;
  CHECK(thrd_join(thr, &wr) == thrd_success);
  CHECK(atomic_load_explicit(&spur_returned, memory_order_acquire) == 1);
  CHECK(atomic_load_explicit(&spur_observed, memory_order_acquire) == 1);
  return ret;
}

// A notify that fires before the waiter parks (value unchanged) must not cause
// a premature return either: the waiter still blocks until the value differs.
static int notify_before_park_test(void)
{
  int ret = 0;
  spur_val = 0;
  spur_parked = 0;
  spur_returned = 0;
  spur_observed = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&spur_val);
  thrd_t thr;
  CHECK(thrd_create(&thr, spur_waiter, NULL) == thrd_success);
  wait_flag(&spur_parked);
  CHECK(spur_parked == 1);
  thrd_sleep_ms(30);
  CHECK(atomic_load_explicit(&spur_returned, memory_order_acquire) == 0);
  atomic_store_explicit(&spur_val, 2, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&spur_val);
  int wr;
  CHECK(thrd_join(thr, &wr) == thrd_success);
  CHECK(atomic_load_explicit(&spur_observed, memory_order_acquire) == 2);
  return ret;
}

// --- Notify-before-park robustness on the hash-table fallback path ---
// A notify that precedes the park never creates a proxy node (and so is lost by
// design), and a bare store never signals the proxy. The queue must still not
// be permanently wedged: a subsequent store + notify reliably unblocks the
// waiter, which then observes the final value. Bounded wall-clock, no
// sleep-only sync.
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t lw_val = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int lw_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int lw_woke = 0;

static int lw_waiter(void *arg)
{
  (void) arg;
  uint_least8_t expected = 0;
  atomic_store_explicit(&lw_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&lw_val, (int) expected);
  atomic_fetch_add_explicit(&lw_woke, 1, memory_order_relaxed);
  return 0;
}

static int lost_wake_stress_test(void)
{
  int ret = 0;
  for(int iter = 0; iter < 20; iter++)
  {
    lw_val = 0;
    lw_parked = 0;
    lw_woke = 0;
    // Notifier fires before the waiter parks and without changing the value
    // (may be lost by design). Then a store changes the value, followed by the
    // guaranteed notify that releases the waiter with the final value observed.
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&lw_val);
    thrd_t thr;
    CHECK(thrd_create(&thr, lw_waiter, NULL) == thrd_success);
    wait_flag(&lw_parked);
    CHECK(lw_parked == 1);
    atomic_store_explicit(&lw_val, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&lw_val);
    int wr;
    CHECK(thrd_join(thr, &wr) == thrd_success);
    CHECK(atomic_load_explicit(&lw_woke, memory_order_acquire) == 1);
  }
  return ret;
}

// --- Notify-before-park robustness on the native 4-byte fast path ---
// Same cycle as lost_wake_stress_test, but through the native 4-byte path where
// the waiter parks directly on the user's object rather than on a hash-table
// proxy, so the lost-wake interleaving differs. A notify that precedes the park
// is lost by design and a bare store never signals a parked waiter; the
// subsequent store + notify must still reliably unblock the waiter, which then
// observes the final value. Bounded wall-clock, no sleep-only sync.
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t
lw4_val = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int lw4_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int lw4_woke = 0;

static int lw4_waiter(void *arg)
{
  (void) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_store_explicit(&lw4_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&lw4_val, (int) expected);
  atomic_fetch_add_explicit(&lw4_woke, 1, memory_order_relaxed);
  return 0;
}

static int lost_wake_stress_native_test(void)
{
  int ret = 0;
  for(int iter = 0; iter < 20; iter++)
  {
    lw4_val = 0;
    lw4_parked = 0;
    lw4_woke = 0;
    // Notifier fires before the waiter parks and without changing the value
    // (may be lost by design). Then a store changes the value, followed by the
    // guaranteed notify that releases the waiter with the final value observed.
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&lw4_val);
    thrd_t thr;
    CHECK(thrd_create(&thr, lw4_waiter, NULL) == thrd_success);
    wait_flag(&lw4_parked);
    CHECK(lw4_parked == 1);
    atomic_store_explicit(&lw4_val, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&lw4_val);
    int wr;
    CHECK(thrd_join(thr, &wr) == thrd_success);
    CHECK(atomic_load_explicit(&lw4_woke, memory_order_acquire) == 1);
  }
  return ret;
}

int atomic_wait_more_test_main(void)
{
  int ret = 0;
  ret += immediate_return_test();
  ret += spurious_wake_test();
  ret += notify_before_park_test();
  ret += lost_wake_stress_test();
  ret += lost_wake_stress_native_test();
  return ret;
}

int main(void)
{
  return atomic_wait_more_test_main();
}
