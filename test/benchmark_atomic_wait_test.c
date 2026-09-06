#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wg14_atomic_waits/atomic_wait.h>

// Progress markers on stderr: ctest only echoes them on failure, so a hang is
// localisable to the exact part of the test that blocked.
#define SECTION(name) fprintf(stderr, "benchmark_atomic_wait_test: " name "\n")

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
// proxy generation starts fresh and the park really suspends), the producer
// busy-settles so the park has reached the kernel before it stores a new value
// and notifies, and the consumer times each wake from the producer's notify
// timestamp to its own return from atomic_wait (a ns-resolution clock; the
// settle itself is never part of the measurement).
//
// Why a settle at all: the wait primitives give no user-space way to observe
// that a park has reached the kernel, so without one a notify can land in the
// consumer's check-to-park window, the wait then returns immediately without
// suspending, and the benchmark would report a blend of genuine kernel wakes
// and value-change detects. A fixed settle is not portable though: the time
// from the consumer's park signal to its kernel sleep is machine- and
// load-dependent, so the settle is calibrated per path at run time (see
// calibrate_settle_us) by raising it until at least 99.9% of notifies catch a
// genuinely parked consumer (suspend fraction = consumer wakes / producer
// rounds). An uncalibrated settle that under-settles does not merely add
// noise: producer and consumer fall into phase-locked orbits where one notify
// in three finds the consumer mid-cycle and the reported "mean wake latency"
// swings wildly with the run duration, so no fixed small settle is safe.
//
// The calibration starts at 8 us, never lower: with shorter parks the woken
// CPU sits on the idle-state entry boundary, where a microsecond either way
// changes the wake latency by ~0.5 us and the numbers stop being repeatable.
// Both paths are then measured at the same settle (the larger of the two
// calibrated values): the wake latency depends on the park duration (idle
// states), so measuring the paths at different settles would bias one against
// the other.
//
// The measurement phase then records every wake latency in ns and reports
// percentiles (p50/p90/p99/p99.9) rather than just the mean: wake latencies
// are strongly skewed (CPU idle-state exit, scheduler and frequency effects
// pile onto the right tail), so the mean of a few runs is dominated by
// whatever the kernel happened to do, whereas the percentiles characterise
// the whole distribution repeatably.

#define CAL_SETTLE_US_STEPS {8, 10, 12, 16, 20, 24, 32, 40, 48, 64}
#define CAL_EPOCH_MS 200
// A notify that finds the consumer parked produces exactly one wake, so
// wakes/rounds is the fraction of measured events that genuinely suspended.
// Calibrate to 99.9%: the residual sub-0.1% instant returns sit at tiny
// latencies and cannot move any reported percentile.
#define MIN_SUSPEND_RATIO 0.999
#define MAX_SETTLE_US 64
#define TARGET_EVENTS 500000
#define MAX_PHASE_MS 8000
#define DRAIN_MAX_ITERATIONS 1000000
#define PARK_TIMEOUT_NS 2000000000LL

// Wake latencies longer than this cannot be a park caught by a notify within
// the same cycle (the producer paces at the calibrated settle, at most tens
// of microseconds); they are shutdown artifacts (a wake from the stop drain,
// or a stale notify timestamp) and are discarded.
#define SANE_DELTA_NS 100000000LL

static long long monotonic_ns(void)
{
#if defined(_WIN32) || defined(_WIN64)
  // QueryPerformanceFrequency returns a constant non-zero value for the life
  // of the process, so cache it (zero also serves as the not-yet-initialised
  // sentinel). The benchmark is a single TU, so exactly one cache exists.
  static LARGE_INTEGER freq;
  if(freq.QuadPart == 0)
  {
    QueryPerformanceFrequency(&freq);
  }
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  // Divide before multiplying: count can be the raw TSC (~10^9-10^10 ticks per
  // second on modern x64), so count * 10^9 would overflow 64-bit within
  // seconds of system uptime.
  return (long long) ((count.QuadPart / freq.QuadPart) * 1000000000LL +
                      ((count.QuadPart % freq.QuadPart) * 1000000000LL) /
                      freq.QuadPart);
#else
  struct timespec ts;
  (void) clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long) ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

// Busy-wait (not nanosleep: sub-millisecond sleeps on macOS round up to the
// timer granularity, which would swamp the measurement). The settle is only
// long enough for the consumer's park to reach the kernel; it is not part of
// the measured wake latency.
static void busy_wait_ns(long long ns)
{
  const long long end = monotonic_ns() + ns;
  while(monotonic_ns() < end)
  {
  }
}

// 4-byte native fast path.
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_value = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_llong g_notify_ns = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_rounds = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_wakes = 0;

// 1-byte hash-table fallback path.
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t g_value8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_llong g_notify_ns8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_rounds8 = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_wakes8 = 0;

// Shared per-phase state, written by the main thread before the gate opens
// (the gate release/acquire makes the writes visible to both workers) and
// read once by each worker after the acquire.
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_gate = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_stop = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_pushed = 0;
static long long g_settle_ns;
static long long g_deadline_ns;
static uint32_t *g_samples;
static size_t g_capacity;

static void wait_for_gate(void)
{
  while(atomic_load_explicit(&g_gate, memory_order_acquire) == 0)
  {
    thrd_sleep_ms(1);
  }
}

static void wait_parked_or_timeout(int use_8bit)
{
  const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *const parked =
  use_8bit ? &g_parked8 : &g_parked;
  const long long deadline = monotonic_ns() + PARK_TIMEOUT_NS;
  while(atomic_load_explicit(parked, memory_order_acquire) == 0)
  {
    if(monotonic_ns() > deadline)
    {
      fprintf(stderr,
              "FATAL: timeout waiting for the consumer to park after 2000 "
              "ms; aborting\n");
      abort();
    }
  }
}

static int producer_func(void *arg)
{
  (void) arg;
  const long long settle_ns = g_settle_ns;
  wait_for_gate();
  // Do not start pacing until the consumer's first park exists: the very
  // first notify would otherwise fire before the consumer has even entered
  // its wait, guaranteeing a non-suspend round.
  wait_parked_or_timeout(0);
  for(;;)
  {
    busy_wait_ns(settle_ns);
    if(atomic_load_explicit(&g_stop, memory_order_acquire))
    {
      break;
    }
    atomic_store_explicit(&g_notify_ns, monotonic_ns(), memory_order_release);
    atomic_fetch_add_explicit(&g_value, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value);
    atomic_fetch_add_explicit(&g_rounds, 1, memory_order_relaxed);
    if(atomic_load_explicit(&g_pushed, memory_order_relaxed) >= g_capacity ||
       monotonic_ns() > g_deadline_ns)
    {
      atomic_store_explicit(&g_stop, 1, memory_order_release);
      break;
    }
  }
  return 0;
}

static int consumer_func(void *arg)
{
  (void) arg;
  wait_for_gate();
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
    if(atomic_load_explicit(&g_stop, memory_order_acquire))
    {
      // Stopped while parked: this return was produced by the shutdown
      // drain's notify_all, not by a paced notify, so do not sample it.
      atomic_store_explicit(&g_parked, 0, memory_order_release);
      break;
    }
    const long long now = monotonic_ns();
    const long long notify_ns =
    atomic_load_explicit(&g_notify_ns, memory_order_acquire);
    const long long delta = now - notify_ns;
    if(delta >= 0 && delta < SANE_DELTA_NS)
    {
      const unsigned long long idx =
      atomic_fetch_add_explicit(&g_pushed, 1, memory_order_relaxed);
      if(idx < g_capacity)
      {
        if(g_samples != NULL)
        {
          g_samples[idx] = (uint32_t) delta;
        }
      }
      else
      {
        atomic_store_explicit(&g_stop, 1, memory_order_release);
      }
    }
    atomic_fetch_add_explicit(&g_wakes, 1, memory_order_relaxed);
    expected = atomic_load_explicit(&g_value, memory_order_acquire);
    atomic_store_explicit(&g_parked, 0, memory_order_release);
  }
  return 0;
}

static int producer_func8(void *arg)
{
  (void) arg;
  const long long settle_ns = g_settle_ns;
  wait_for_gate();
  wait_parked_or_timeout(1);
  for(;;)
  {
    busy_wait_ns(settle_ns);
    if(atomic_load_explicit(&g_stop, memory_order_acquire))
    {
      break;
    }
    atomic_store_explicit(&g_notify_ns8, monotonic_ns(), memory_order_release);
    atomic_fetch_add_explicit(&g_value8, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value8);
    atomic_fetch_add_explicit(&g_rounds8, 1, memory_order_relaxed);
    if(atomic_load_explicit(&g_pushed, memory_order_relaxed) >= g_capacity ||
       monotonic_ns() > g_deadline_ns)
    {
      atomic_store_explicit(&g_stop, 1, memory_order_release);
      break;
    }
  }
  return 0;
}

static int consumer_func8(void *arg)
{
  (void) arg;
  wait_for_gate();
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
    if(atomic_load_explicit(&g_stop, memory_order_acquire))
    {
      atomic_store_explicit(&g_parked8, 0, memory_order_release);
      break;
    }
    const long long now = monotonic_ns();
    const long long notify_ns =
    atomic_load_explicit(&g_notify_ns8, memory_order_acquire);
    const long long delta = now - notify_ns;
    if(delta >= 0 && delta < SANE_DELTA_NS)
    {
      const unsigned long long idx =
      atomic_fetch_add_explicit(&g_pushed, 1, memory_order_relaxed);
      if(idx < g_capacity)
      {
        if(g_samples != NULL)
        {
          g_samples[idx] = (uint32_t) delta;
        }
      }
      else
      {
        atomic_store_explicit(&g_stop, 1, memory_order_release);
      }
    }
    atomic_fetch_add_explicit(&g_wakes8, 1, memory_order_relaxed);
    expected = atomic_load_explicit(&g_value8, memory_order_acquire);
    atomic_store_explicit(&g_parked8, 0, memory_order_release);
  }
  return 0;
}

static void reset_state(int use_8bit)
{
  if(use_8bit)
  {
    atomic_store_explicit(&g_value8, 0, memory_order_relaxed);
    atomic_store_explicit(&g_parked8, 0, memory_order_relaxed);
    atomic_store_explicit(&g_notify_ns8, 0, memory_order_relaxed);
    atomic_store_explicit(&g_rounds8, 0, memory_order_relaxed);
    atomic_store_explicit(&g_wakes8, 0, memory_order_relaxed);
  }
  else
  {
    atomic_store_explicit(&g_value, 0, memory_order_relaxed);
    atomic_store_explicit(&g_parked, 0, memory_order_relaxed);
    atomic_store_explicit(&g_notify_ns, 0, memory_order_relaxed);
    atomic_store_explicit(&g_rounds, 0, memory_order_relaxed);
    atomic_store_explicit(&g_wakes, 0, memory_order_relaxed);
  }
  atomic_store_explicit(&g_pushed, 0, memory_order_relaxed);
  atomic_store_explicit(&g_gate, 0, memory_order_relaxed);
  atomic_store_explicit(&g_stop, 0, memory_order_relaxed);
}

static void spawn_workers(int use_8bit, thrd_t *producer, thrd_t *consumer)
{
  thrd_create(producer, use_8bit ? producer_func8 : producer_func, NULL);
  thrd_create(consumer, use_8bit ? consumer_func8 : consumer_func, NULL);
  atomic_store_explicit(&g_gate, 1, memory_order_release);
}

// The consumer may be parked when g_stop is set; keep bumping + notifying
// until it has woken and cleared the park flag, so shutdown never hangs.
static void drain_consumer(int use_8bit)
{
  const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *const parked =
  use_8bit ? &g_parked8 : &g_parked;
  for(unsigned i = 0; i < DRAIN_MAX_ITERATIONS; i++)
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
    if(atomic_load_explicit(parked, memory_order_acquire) == 0)
    {
      break;
    }
    thrd_sleep_ms(1);
  }
}

static void join_workers(thrd_t producer, thrd_t consumer)
{
  int wr;
  thrd_join(producer, &wr);
  thrd_join(consumer, &wr);
}

static unsigned long long load_wakes(int use_8bit)
{
  return (unsigned long long) atomic_load_explicit(
  use_8bit ? &g_wakes8 : &g_wakes, memory_order_relaxed);
}

static unsigned long long load_rounds(int use_8bit)
{
  return (unsigned long long) atomic_load_explicit(
  use_8bit ? &g_rounds8 : &g_rounds, memory_order_relaxed);
}

// Runs the ping-pong for the given settle for `ms` milliseconds and returns
// the suspend fraction (consumer wakes / producer rounds). Spawning a fresh
// worker pair per candidate keeps the phases independent.
static double run_epoch(int use_8bit, long long settle_ns, int ms)
{
  reset_state(use_8bit);
  g_settle_ns = settle_ns;
  // The epoch is timed by the main thread (it sets g_stop after `ms`
  // milliseconds); give the producer a generous deadline and no sample cap so
  // only the main thread can end the epoch.
  g_deadline_ns = monotonic_ns() + ((long long) ms + 1000) * 1000000;
  g_capacity = (size_t) -1;
  g_samples = NULL;
  thrd_t producer;
  thrd_t consumer;
  spawn_workers(use_8bit, &producer, &consumer);
  thrd_sleep_ms(ms);
  atomic_store_explicit(&g_stop, 1, memory_order_release);
  drain_consumer(use_8bit);
  join_workers(producer, consumer);
  const unsigned long long rounds = load_rounds(use_8bit);
  const unsigned long long wakes = load_wakes(use_8bit);
  if(rounds == 0)
  {
    return 0.0;
  }
  return (double) wakes / (double) rounds;
}

// Picks the smallest calibrated settle (in us) for which at least 99.9% of
// notifies catch a genuinely parked consumer. See the file header comment for
// why this must be calibrated rather than fixed.
static int calibrate_settle_us(int use_8bit)
{
  static const int steps[] = CAL_SETTLE_US_STEPS;
  for(size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
  {
    const double ratio =
    run_epoch(use_8bit, (long long) steps[i] * 1000, CAL_EPOCH_MS);
    fprintf(stderr, "calibration[%s]: settle %d us -> suspend fraction %.4f\n",
            use_8bit ? "1-byte hash-table" : "4-byte native", steps[i], ratio);
    if(ratio >= MIN_SUSPEND_RATIO)
    {
      return steps[i];
    }
  }
  return MAX_SETTLE_US;
}

static int cmp_u32(const void *a, const void *b)
{
  const uint32_t ua = *(const uint32_t *) a;
  const uint32_t ub = *(const uint32_t *) b;
  return (ua > ub) - (ua < ub);
}

static size_t measure_path(int use_8bit, long long settle_ns)
{
  g_capacity = TARGET_EVENTS;
  g_samples = (uint32_t *) calloc(g_capacity, sizeof(uint32_t));
  if(g_samples == NULL)
  {
    fprintf(stderr, "FATAL: out of memory allocating sample buffer\n");
    abort();
  }
  reset_state(use_8bit);
  g_settle_ns = settle_ns;
  g_deadline_ns = monotonic_ns() + (long long) MAX_PHASE_MS * 1000000;
  const long long start_ns = monotonic_ns();
  thrd_t producer;
  thrd_t consumer;
  spawn_workers(use_8bit, &producer, &consumer);
  int wr;
  thrd_join(producer, &wr);
  drain_consumer(use_8bit);
  thrd_join(consumer, &wr);
  const long long elapsed_ms = (monotonic_ns() - start_ns) / 1000000;
  const size_t pushed =
  (size_t) atomic_load_explicit(&g_pushed, memory_order_relaxed);
  const unsigned long long rounds = load_rounds(use_8bit);
  const unsigned long long wakes = load_wakes(use_8bit);
  const double suspend = (rounds != 0) ? (double) wakes / (double) rounds : 0.0;
  qsort(g_samples, pushed, sizeof(uint32_t), cmp_u32);
  static const double percentiles[] = {0.50, 0.90, 0.99, 0.999};
  fprintf(stderr,
          "benchmark[%s]: %zu events in %llu ms, settle %lld us, "
          "suspend fraction %.4f",
          use_8bit ? "1-byte hash-table" : "4-byte native", pushed,
          (unsigned long long) elapsed_ms, settle_ns / 1000, suspend);
  for(size_t i = 0; i < sizeof(percentiles) / sizeof(percentiles[0]); i++)
  {
    if(pushed == 0)
    {
      fprintf(stderr, ", p%g us n/a", percentiles[i] * 100.0);
      continue;
    }
    // Nearest-rank percentile.
    size_t k = (size_t) (percentiles[i] * (double) pushed);
    if(k == 0)
    {
      k = 1;
    }
    fprintf(stderr, ", p%g %.3f", percentiles[i] * 100.0,
            (double) g_samples[k - 1] / 1000.0);
  }
  fprintf(stderr, " us\n");
  free(g_samples);
  g_samples = NULL;
  return pushed;
}

int benchmark_atomic_wait_test(void)
{
  SECTION("4-byte native fast path");
  const int settle4 = calibrate_settle_us(0);
  SECTION("1-byte hash-table fallback path");
  const int settle8 = calibrate_settle_us(1);
  // Measure both paths at the same settle (the larger of the two calibrated
  // values): the wake latency depends on the park duration through CPU idle
  // states, so differing settles would bias one path against the other.
  const int settle_us = (settle4 > settle8) ? settle4 : settle8;
  fprintf(stderr, "benchmark: measuring both paths at settle %d us\n",
          settle_us);
  SECTION("4-byte native fast path");
  measure_path(0, (long long) settle_us * 1000);
  SECTION("1-byte hash-table fallback path");
  measure_path(1, (long long) settle_us * 1000);
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
