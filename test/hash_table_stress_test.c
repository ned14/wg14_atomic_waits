#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// §1.11 (analysis): hash-table rehash and removal-compaction robustness under
// high occupancy. Each waiter parks on its own 8-bit object (the hash-table
// fallback path on every backend), holding a proxy in the table; with
// HASH_STRESS_WAITERS far above the initial 1024-bucket table's load-factor
// threshold the table is rehashed under load, and every waiter's return runs
// the removal-compaction probe chain. Every waiter must be findable by its
// notify and must return exactly once. The old quadratic probing visited only
// half the slots and hash_table_grow could silently drop an item on a failed
// rehash probe; this stress would lose a waiter if either regressed.
#define HASH_STRESS_WAITERS 900

static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t
g_vals[HASH_STRESS_WAITERS];
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_returned = 0;

static int hash_stress_waiter(void *arg)
{
  const long i = (long) (intptr_t) arg;
  atomic_fetch_add_explicit(&g_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_vals[i], 0);
  atomic_fetch_add_explicit(&g_returned, 1, memory_order_release);
  return 0;
}

int hash_table_stress_test_main(void)
{
  int ret = 0;
  thrd_t thrs[HASH_STRESS_WAITERS];
  for(int round = 0; round < 3; round++)
  {
    for(int i = 0; i < HASH_STRESS_WAITERS; i++)
    {
      g_vals[i] = 0;
    }
    atomic_store_explicit(&g_parked, 0, memory_order_release);
    atomic_store_explicit(&g_returned, 0, memory_order_release);
    for(int i = 0; i < HASH_STRESS_WAITERS; i++)
    {
      CHECK(thrd_create(&thrs[i], hash_stress_waiter,
                        (void *) (intptr_t) (long) i) == thrd_success);
    }
    test_wait_until("hash stress parked", &g_parked, HASH_STRESS_WAITERS);
    for(int i = 0; i < HASH_STRESS_WAITERS; i++)
    {
      atomic_store_explicit(&g_vals[i], 1, memory_order_seq_cst);
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_vals[i]);
    }
    int wr;
    for(int i = 0; i < HASH_STRESS_WAITERS; i++)
    {
      CHECK(thrd_join(thrs[i], &wr) == thrd_success);
    }
    CHECK(atomic_load_explicit(&g_returned, memory_order_acquire) ==
          HASH_STRESS_WAITERS);
  }
  return ret;
}

int main(void)
{
  return hash_table_stress_test_main();
}
