#include "test_common.h"
#include <stdio.h>
#include <time.h>
#include <wg14_atomic_waits/atomic_wait.h>

#define NPRODUCERS 4
#define NCONSUMERS 4
#define RUNTIME_MS 300

static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_value = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_gate = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_stop = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t g_ops = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_consumers_done = 0;

// Producer: repeatedly advance the monotonic counter and wake a consumer.
static int producer_func(void *arg)
{
  (void) arg;
  while(atomic_load_explicit(&g_gate, memory_order_acquire) == 0)
  {
    thrd_sleep_ms(1);
  }
  unsigned long local = 0;
  while(atomic_load_explicit(&g_stop, memory_order_acquire) == 0)
  {
    atomic_fetch_add_explicit(&g_value, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_value);
    local++;
  }
  atomic_fetch_add_explicit(&g_ops, local, memory_order_relaxed);
  return 0;
}

// Consumer: wait for the counter to advance past the value it last observed.
static int consumer_func(void *arg)
{
  (void) arg;
  while(atomic_load_explicit(&g_gate, memory_order_acquire) == 0)
  {
    thrd_sleep_ms(1);
  }
  int last = atomic_load_explicit(&g_value, memory_order_acquire);
  while(atomic_load_explicit(&g_stop, memory_order_acquire) == 0)
  {
    int current = atomic_load_explicit(&g_value, memory_order_acquire);
    if(current != last)
    {
      last = current;
      continue;
    }
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_value, last);
    last = atomic_load_explicit(&g_value, memory_order_acquire);
  }
  atomic_fetch_add_explicit(&g_consumers_done, 1, memory_order_release);
  return 0;
}

int benchmark_atomic_wait_test(void)
{
  thrd_t producers[NPRODUCERS];
  thrd_t consumers[NCONSUMERS];
  g_value = 0;
  g_gate = 0;
  g_stop = 0;
  g_ops = 0;
  g_consumers_done = 0;

  for(int i = 0; i < NPRODUCERS; i++)
  {
    thrd_create(&producers[i], producer_func, NULL);
  }
  for(int i = 0; i < NCONSUMERS; i++)
  {
    thrd_create(&consumers[i], consumer_func, NULL);
  }

  atomic_store_explicit(&g_gate, 1, memory_order_release);
  thrd_sleep_ms(RUNTIME_MS);
  atomic_store_explicit(&g_stop, 1, memory_order_release);

  // Consumers may be re-parked inside atomic_wait with an unchanged value and
  // miss a single final notify; keep bumping + notifying until they all exit so
  // shutdown never hangs and producers never run again (they have stopped).
  for(unsigned i = 0;
      i < 1000000 && atomic_load_explicit(&g_consumers_done,
                                          memory_order_acquire) < NCONSUMERS;
      i++)
  {
    atomic_fetch_add_explicit(&g_value, 1, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_value);
    thrd_sleep_ms(1);
  }

  int wr;
  for(int i = 0; i < NPRODUCERS; i++)
  {
    thrd_join(producers[i], &wr);
  }
  for(int i = 0; i < NCONSUMERS; i++)
  {
    thrd_join(consumers[i], &wr);
  }

  const unsigned long long ops =
  (unsigned long long) atomic_load_explicit(&g_ops, memory_order_relaxed);
  fprintf(stderr, "benchmark: %llu notify/ops in %u ms across %d producers\n",
          ops, RUNTIME_MS, NPRODUCERS);
  return 0;
}

int main(void)
{
  return benchmark_atomic_wait_test();
}
