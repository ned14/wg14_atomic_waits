#include "test_common.h"
#include <stdio.h>
#include <time.h>
#include <wg14_atomic_waits/atomic_wait.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// Ping-pong suspend-wake benchmark. One producer and one consumer exchange a
// shared counter: the consumer parks in atomic_wait (a genuine kernel suspend
// on both the native fast path and the hash-table fallback -- on the fallback
// the single consumer's proxy is removed and recreated between cycles, so its
// wake flag is re-armed and the park really suspends). The producer waits for
// the consumer to signal it is about to park, settles briefly so the park has
// reached the kernel (otherwise the store lands before the park syscall and
// the wait returns immediately without suspending), then stores a new value
// and notifies.
//
// Two numbers are reported per path:
//  - the mean suspend-wake latency, timed on the consumer from the producer's
//    notify timestamp to its own return from atomic_wait (this excludes the
//    settle), and
//  - the achieved round-trip rate (completed park -> wake -> re-park cycles).

#define RUNTIME_MS 300
#define SETTLE_US 3

static long long monotonic_us(void)
{
#if defined(_WIN32)
  LARGE_INTEGER freq, cnt;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&cnt);
  return (long long) (cnt.QuadPart * 1000000LL / freq.QuadPart);
#else
  struct timespec ts;
  (void) clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long) ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
#endif
}

// Busy-wait (not nanosleep: sub-millisecond sleeps on macOS round up to the
// timer granularity, which would swamp the measurement). The settle is only
// long enough for the consumer's park to reach the kernel; it is not part of
// the measured wake latency.
static void busy_wait_us(long us)
{
  const long long end = monotonic_us() + us;
  while(monotonic_us() < end)
  {
  }
}

// 4-byte native fast path.
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_value = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_llong g_notify_us = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_llong g_latency_us = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_rounds = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_wakes = 0;

// 1-byte hash-table fallback path.
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t g_value8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_llong g_notify_us8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_llong g_latency_us8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_rounds8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_wakes8 = 0;

static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_gate = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_stop = 0;

static int producer_func(void *arg)
{
  (void) arg;
  while(atomic_load_explicit(&g_gate, memory_order_acquire) == 0)
  {
    thrd_sleep_ms(1);
  }
  while(atomic_load_explicit(&g_stop, memory_order_acquire) == 0)
  {
    // Spin until the consumer signals it is about to park.
    while(atomic_load_explicit(&g_parked, memory_order_acquire) == 0)
    {
    }
    busy_wait_us(SETTLE_US);
    atomic_store_explicit(&g_notify_us, monotonic_us(), memory_order_release);
    atomic_fetch_add_explicit(&g_value, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value);
    atomic_fetch_add_explicit(&g_rounds, 1, memory_order_relaxed);
  }
  return 0;
}

static int consumer_func(void *arg)
{
  (void) arg;
  while(atomic_load_explicit(&g_gate, memory_order_acquire) == 0)
  {
    thrd_sleep_ms(1);
  }
  int expected = 0;
  for(;;)
  {
    atomic_store_explicit(&g_parked, 1, memory_order_release);
    if(atomic_load_explicit(&g_stop, memory_order_acquire))
    {
      atomic_store_explicit(&g_parked, 0, memory_order_release);
      break;
    }
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_value, expected);
    const long long now = monotonic_us();
    const long long notify_us =
    atomic_load_explicit(&g_notify_us, memory_order_acquire);
    atomic_fetch_add_explicit(&g_latency_us, now - notify_us,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&g_wakes, 1, memory_order_relaxed);
    expected = atomic_load_explicit(&g_value, memory_order_acquire);
    atomic_store_explicit(&g_parked, 0, memory_order_release);
  }
  return 0;
}

static int producer_func8(void *arg)
{
  (void) arg;
  while(atomic_load_explicit(&g_gate, memory_order_acquire) == 0)
  {
    thrd_sleep_ms(1);
  }
  while(atomic_load_explicit(&g_stop, memory_order_acquire) == 0)
  {
    while(atomic_load_explicit(&g_parked8, memory_order_acquire) == 0)
    {
    }
    busy_wait_us(SETTLE_US);
    atomic_store_explicit(&g_notify_us8, monotonic_us(), memory_order_release);
    atomic_fetch_add_explicit(&g_value8, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value8);
    atomic_fetch_add_explicit(&g_rounds8, 1, memory_order_relaxed);
  }
  return 0;
}

static int consumer_func8(void *arg)
{
  (void) arg;
  while(atomic_load_explicit(&g_gate, memory_order_acquire) == 0)
  {
    thrd_sleep_ms(1);
  }
  uint_least8_t expected = 0;
  for(;;)
  {
    atomic_store_explicit(&g_parked8, 1, memory_order_release);
    if(atomic_load_explicit(&g_stop, memory_order_acquire))
    {
      atomic_store_explicit(&g_parked8, 0, memory_order_release);
      break;
    }
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_value8, (int) expected);
    const long long now = monotonic_us();
    const long long notify_us =
    atomic_load_explicit(&g_notify_us8, memory_order_acquire);
    atomic_fetch_add_explicit(&g_latency_us8, now - notify_us,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&g_wakes8, 1, memory_order_relaxed);
    expected = atomic_load_explicit(&g_value8, memory_order_acquire);
    atomic_store_explicit(&g_parked8, 0, memory_order_release);
  }
  return 0;
}

static unsigned long long run_benchmark(int use_8bit)
{
  g_gate = 0;
  g_stop = 0;
  if(use_8bit)
  {
    g_value8 = 0;
    g_parked8 = 0;
    g_notify_us8 = 0;
    g_latency_us8 = 0;
    g_rounds8 = 0;
    g_wakes8 = 0;
  }
  else
  {
    g_value = 0;
    g_parked = 0;
    g_notify_us = 0;
    g_latency_us = 0;
    g_rounds = 0;
    g_wakes = 0;
  }
  thrd_t producer;
  thrd_t consumer;
  thrd_create(&producer, use_8bit ? producer_func8 : producer_func, NULL);
  thrd_create(&consumer, use_8bit ? consumer_func8 : consumer_func, NULL);

  atomic_store_explicit(&g_gate, 1, memory_order_release);
  thrd_sleep_ms(RUNTIME_MS);
  atomic_store_explicit(&g_stop, 1, memory_order_release);

  // The consumer may be parked when g_stop is set; keep bumping + notifying
  // until it has woken and cleared the park flag, so shutdown never hangs.
  for(unsigned i = 0; i < 1000000; i++)
  {
    if(use_8bit)
    {
      atomic_fetch_add_explicit(&g_value8, 1, memory_order_seq_cst);
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_value8);
    }
    else
    {
      atomic_fetch_add_explicit(&g_value, 1, memory_order_seq_cst);
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_value);
    }
    const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *const parked =
    use_8bit ? &g_parked8 : &g_parked;
    if(atomic_load_explicit(parked, memory_order_acquire) == 0)
    {
      break;
    }
    thrd_sleep_ms(1);
  }

  int wr;
  thrd_join(producer, &wr);
  thrd_join(consumer, &wr);

  return use_8bit ?
         (unsigned long long) atomic_load_explicit(&g_rounds8,
                                                   memory_order_relaxed) :
         (unsigned long long) atomic_load_explicit(&g_rounds,
                                                   memory_order_relaxed);
}

static void report_result(const char *name, unsigned long long rounds,
                          int use_8bit)
{
  const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_llong *const latency =
  use_8bit ? &g_latency_us8 : &g_latency_us;
  const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *const wakes =
  use_8bit ? &g_wakes8 : &g_wakes;
  const long long latency_us =
  atomic_load_explicit(latency, memory_order_relaxed);
  const unsigned long long wake_count =
  atomic_load_explicit(wakes, memory_order_relaxed);
  fprintf(stderr,
          "benchmark[%s]: %llu suspend-wake round trips in %u ms "
          "(mean wake latency %.3f us)\n",
          name, rounds, RUNTIME_MS, (double) latency_us / (double) wake_count);
}

int benchmark_atomic_wait_test(void)
{
  const unsigned long long rounds4 = run_benchmark(0);
  report_result("4-byte native", rounds4, 0);
  const unsigned long long rounds8 = run_benchmark(1);
  report_result("1-byte hash-table", rounds8, 1);
  return 0;
}

// main must keep default visibility: with CMAKE_C_VISIBILITY_PRESET=hidden and
// -O2, Apple's ld64 can otherwise resolve the entry point to the Mach-O header
// (LC_MAIN entryoff 0) and the binary crashes at startup with
// EXC_BAD_INSTRUCTION.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((visibility("default")))
#endif
int main(void)
{
  if(benchmark_atomic_wait_test() != 0)
  {
    return 1;
  }
  return 0;
}
