#include "test_common.h"
#include <time.h>
#include <wg14_atomic_waits/atomic_wait.h>

// Progress markers on stderr: ctest only echoes them on failure, so a hang is
// localisable to the exact part of the test that blocked.
#define SECTION(name) fprintf(stderr, "atomic_wait_race_test: " name "\n")

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

// B1, "Reveal the PRIMARY RACE (proxy flag never reset -> lost
// wake/busy-spin)".
//
// This drives the hash-table fallback path (a sub-native width such as
// atomic_uint_least8_t) through a wait -> spurious-notify (value still equal)
// -> re-park cycle on a single object, and asserts the waiter actually suspends
// while re-parked rather than busy-spinning at ~100% CPU.

static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t g_value = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked = 0;
static double g_cpu_burned = 0.0;

// CPU seconds consumed by the current thread since its last reset.
static double cpu_seconds(void)
{
#if defined(__APPLE__)
  thread_basic_info_data_t info;
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  const kern_return_t kr = thread_info(mach_thread_self(), THREAD_BASIC_INFO,
                                       (thread_info_t) &info, &count);
  if(kr != KERN_SUCCESS)
  {
    return -1.0;
  }
  return (double) info.user_time.seconds +
         (double) info.user_time.microseconds / 1e6 +
         (double) info.system_time.seconds +
         (double) info.system_time.microseconds / 1e6;
#elif defined(__linux__) && defined(CLOCK_THREAD_CPUTIME_ID)
  struct timespec ts;
  if(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
  {
    return -1.0;
  }
  return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
#else
  return -1.0;
#endif
}

static int waiter_func(void *arg)
{
  (void) arg;
  uint_least8_t expected = 0;
  atomic_store_explicit(&g_parked, 1, memory_order_release);
  const double cpu0 = cpu_seconds();
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_value, (int) expected);
  const double cpu1 = cpu_seconds();
  if(cpu0 >= 0.0 && cpu1 >= 0.0)
  {
    g_cpu_burned = cpu1 - cpu0;
  }
  return (int) atomic_load_explicit(&g_value, memory_order_acquire);
}

int atomic_wait_race_test_main(void)
{
  int ret = 0;
  g_value = 0;
  g_parked = 0;
  g_cpu_burned = -1.0;
  thrd_t thr;
  CHECK(thrd_create(&thr, waiter_func, NULL) == thrd_success);
  test_wait_until("g_parked", &g_parked, 1);
  g_parked = 0;

  // Spurious notify with the value unchanged: the waiter must re-park (and, per
  // the proposal, actually suspend again).
  SECTION("spurious notify while parked must re-park and suspend");
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value);

  // Give the (re-parked) waiter a real-time window. If it busy-spins instead of
  // suspending it burns ~100% CPU here; if it properly suspends it burns ~none.
  thrd_sleep_ms(300);

  SECTION("value change + notify releases the waiter");
  atomic_store_explicit(&g_value, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_value);
  int wr;
  CHECK(thrd_join(thr, &wr) == thrd_success);
  CHECK(wr == 1);

  if(g_cpu_burned > 0.1)
  {
    fprintf(stderr,
            "FATAL: waiter busy-spun %g s CPU in a 300 ms "
            "window instead of suspending\n",
            g_cpu_burned);
    return ret + 1;
  }
  return ret;
}

int main(void)
{
  return atomic_wait_race_test_main();
}
