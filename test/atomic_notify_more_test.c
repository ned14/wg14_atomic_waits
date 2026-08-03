#include "test_common.h"
#include <time.h>
#include <wg14_atomic_waits/atomic_wait.h>

#define CAP_WAITERS 6
#define NOTIFIERS 4

static void mwait_flag(const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *flag,
                       int goal)
{
  struct timespec start;
  timespec_get(&start, TIME_UTC);
  while(atomic_load_explicit(flag, memory_order_acquire) < goal)
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

// --- atomic_notify success path & max_threads_to_wake == 0 ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t ns_val =
0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int ns_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int ns_returned = 0;

static int ns_waiter(void *arg)
{
  (void) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_store_explicit(&ns_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&ns_val, (int) expected);
  atomic_store_explicit(&ns_returned, 1, memory_order_release);
  return 0;
}

static int notify_success_test(void)
{
  int ret = 0;
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t value = 0;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) desired = 5;

  // max_threads_to_wake == 0: returns 0, no side effects, value unchanged.
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) e0 = 0;
  int nr = atomic_notify(&value, &e0, desired, 0, memory_order_seq_cst,
                         memory_order_seq_cst);
  CHECK(nr == 0);
  CHECK(atomic_load_explicit(&value, memory_order_seq_cst) == 0);

  // Success: CAS 0->5 succeeds, wakes the parked waiter, returns positive, and
  // the value becomes desired.
  ns_val = 0;
  ns_parked = 0;
  ns_returned = 0;
  thrd_t thr;
  CHECK(thrd_create(&thr, ns_waiter, NULL) == thrd_success);
  mwait_flag(&ns_parked, 1);
  CHECK(ns_parked == 1);
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) e1 = 0;
  nr = atomic_notify(&ns_val, &e1, desired, 1, memory_order_seq_cst,
                     memory_order_seq_cst);
  CHECK(nr > 0);
  CHECK(atomic_load_explicit(&ns_val, memory_order_seq_cst) == desired);
  // On success *expected on return is the value when the compare-exchange was
  // performed, i.e. the pre-exchange value (proposal §7.17.7.11); it is
  // unchanged.
  CHECK(e1 == 0);
  int wr;
  CHECK(thrd_join(thr, &wr) == thrd_success);
  CHECK(atomic_load_explicit(&ns_returned, memory_order_acquire) == 1);
  return ret;
}

// --- Cap: never wake more than max_threads_to_wake parked waiters ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t
cap_val = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int cap_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int cap_woke = 0;

static int cap_waiter(void *arg)
{
  (void) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_fetch_add_explicit(&cap_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&cap_val, (int) expected);
  atomic_fetch_add_explicit(&cap_woke, 1, memory_order_relaxed);
  return 0;
}

static int cap_test(void)
{
  int ret = 0;
  thrd_t thrs[CAP_WAITERS];
  // k = 1 is the only partial cap that every backend honours deterministically
  // (on macOS a wake with max > 1 degenerates to wake-all; a store never wakes
  // a parked waiter). With max_threads_to_wake = 1 exactly one of the parked
  // waiters observes the wake, never more.
  const unsigned k = 1;
  cap_val = 0;
  cap_parked = 0;
  cap_woke = 0;
  for(int i = 0; i < CAP_WAITERS; i++)
  {
    CHECK(thrd_create(&thrs[i], cap_waiter, NULL) == thrd_success);
  }
  mwait_flag(&cap_parked, CAP_WAITERS);

  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  const WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) desired = 1;
  int nr = atomic_notify(&cap_val, &expected, desired, k, memory_order_seq_cst,
                         memory_order_seq_cst);
  CHECK(nr > 0);
  CHECK(atomic_load_explicit(&cap_val, memory_order_seq_cst) == desired);
  thrd_sleep_ms(50);
  CHECK(atomic_load_explicit(&cap_woke, memory_order_acquire) == 1);

  // Release the remaining parked waiters so all threads exit cleanly.
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&cap_val);
  int wr;
  for(int i = 0; i < CAP_WAITERS; i++)
  {
    thrd_join(thrs[i], &wr);
  }
  CHECK(atomic_load_explicit(&cap_woke, memory_order_acquire) == CAP_WAITERS);
  return ret;
}

// --- Concurrent notifies from multiple threads ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t
conc_val = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int conc_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int conc_woke = 0;

static int conc_waiter(void *arg)
{
  (void) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_fetch_add_explicit(&conc_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&conc_val, (int) expected);
  atomic_fetch_add_explicit(&conc_woke, 1, memory_order_relaxed);
  return 0;
}

static int conc_notifier(void *arg)
{
  (void) arg;
  for(int i = 0; i < 100; i++)
  {
    WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
    const WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) desired = 1;
    atomic_notify(&conc_val, &expected, desired, 1, memory_order_seq_cst,
                  memory_order_seq_cst);
    if(atomic_load_explicit(&conc_val, memory_order_acquire) == desired)
    {
      break;
    }
  }
  return 0;
}

static int concurrent_notify_test(void)
{
  int ret = 0;
  thrd_t waiters[CAP_WAITERS];
  thrd_t notifiers[NOTIFIERS];
  conc_val = 0;
  conc_parked = 0;
  conc_woke = 0;
  for(int i = 0; i < CAP_WAITERS; i++)
  {
    CHECK(thrd_create(&waiters[i], conc_waiter, NULL) == thrd_success);
  }
  mwait_flag(&conc_parked, CAP_WAITERS);
  for(int i = 0; i < NOTIFIERS; i++)
  {
    CHECK(thrd_create(&notifiers[i], conc_notifier, NULL) == thrd_success);
  }
  int wr;
  for(int i = 0; i < NOTIFIERS; i++)
  {
    thrd_join(notifiers[i], &wr);
  }
  // Ensure every waiter is released regardless of backend wake semantics.
  atomic_store_explicit(&conc_val, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&conc_val);
  for(int i = 0; i < CAP_WAITERS; i++)
  {
    thrd_join(waiters[i], &wr);
  }
  CHECK(atomic_load_explicit(&conc_woke, memory_order_acquire) == CAP_WAITERS);
  return ret;
}

int atomic_notify_more_test_main(void)
{
  int ret = 0;
  ret += notify_success_test();
  ret += cap_test();
  ret += concurrent_notify_test();
  return ret;
}

int main(void)
{
  return atomic_notify_more_test_main();
}
