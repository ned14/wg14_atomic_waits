#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// Progress markers on stderr: ctest only echoes them on failure, so a hang is
// localisable to the exact part of the test that blocked.
#define SECTION(name) fprintf(stderr, "atomic_wait_widths_test: " name "\n")

// B4: exercise the non-native widths (1- and 2-byte on every POSIX backend, and
// 8-byte on Linux) which route through the hash-table fallback, plus the native
// fast path, without relying on any sleep-only synchronisation.

#define DEFINE_WIDTH_TESTS(SUFFIX, AMTYPE, BASETYPE)                           \
  static WG14_ATOMIC_WAITS_ATOMIC_PREFIX AMTYPE g_val_##SUFFIX = 0;            \
  static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_park_##SUFFIX = 0;       \
  static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_woke_##SUFFIX = 0;       \
  static int waiter_##SUFFIX(void *arg)                                        \
  {                                                                            \
    (void) arg;                                                                \
    BASETYPE expected = 0;                                                     \
    atomic_fetch_add_explicit(&g_park_##SUFFIX, 1, memory_order_release);      \
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&g_val_##SUFFIX, (int) expected);    \
    atomic_fetch_add_explicit(&g_woke_##SUFFIX, 1, memory_order_relaxed);      \
    return 0;                                                                  \
  }                                                                            \
  static int roundtrip_##SUFFIX(void)                                          \
  {                                                                            \
    int ret = 0;                                                               \
    thrd_t thrs[2];                                                            \
    g_val_##SUFFIX = 0;                                                        \
    g_park_##SUFFIX = 0;                                                       \
    g_woke_##SUFFIX = 0;                                                       \
    for(int i = 0; i < 2; i++)                                                 \
      CHECK(thrd_create(&thrs[i], waiter_##SUFFIX, NULL) == thrd_success);     \
    test_wait_until("g_park_" #SUFFIX, &g_park_##SUFFIX, 2);                   \
    atomic_store_explicit(&g_val_##SUFFIX, (BASETYPE) 1,                       \
                          memory_order_seq_cst);                               \
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&g_val_##SUFFIX);              \
    int wr;                                                                    \
    for(int i = 0; i < 2; i++)                                                 \
      CHECK(thrd_join(thrs[i], &wr) == thrd_success);                          \
    CHECK(atomic_load_explicit(&g_woke_##SUFFIX, memory_order_acquire) == 2);  \
    return ret;                                                                \
  }

DEFINE_WIDTH_TESTS(w1, atomic_uint_least8_t, uint_least8_t)
DEFINE_WIDTH_TESTS(w2, atomic_uint_least16_t, uint_least16_t)

// 8-byte: native on macOS/FreeBSD/Windows, hash-table fallback on Linux.
DEFINE_WIDTH_TESTS(w8, atomic_uint_least64_t, uint_least64_t)

// --- Object isolation: waiting on A is not woken by notify on B ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t iso_a = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t iso_b = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int iso_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int iso_returned = 0;

static int iso_waiter(void *arg)
{
  (void) arg;
  uint_least8_t expected = 0;
  atomic_store_explicit(&iso_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&iso_a, (int) expected);
  atomic_store_explicit(&iso_returned, 1, memory_order_release);
  return 0;
}

static int isolation_test(void)
{
  int ret = 0;
  iso_a = 0;
  iso_b = 0;
  iso_parked = 0;
  iso_returned = 0;
  thrd_t thr;
  CHECK(thrd_create(&thr, iso_waiter, NULL) == thrd_success);
  test_wait_until("iso_parked", &iso_parked, 1);
  // Notify object B with B's value changed; A's waiter must stay parked.
  atomic_store_explicit(&iso_b, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&iso_b);
  thrd_sleep_ms(30);
  CHECK(atomic_load_explicit(&iso_returned, memory_order_acquire) == 0);

  // Now signal object A properly.
  atomic_store_explicit(&iso_a, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&iso_a);
  int wr;
  CHECK(thrd_join(thr, &wr) == thrd_success);
  CHECK(atomic_load_explicit(&iso_returned, memory_order_acquire) == 1);
  return ret;
}

// --- Spurious wake in the hash-table path (explicit): re-park until a real ---
// --- value change + notify arrives. ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t hs_val = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int hs_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int hs_returned = 0;

static int hs_waiter(void *arg)
{
  (void) arg;
  uint_least8_t expected = 0;
  atomic_store_explicit(&hs_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit)(&hs_val, (int) expected,
                                                 memory_order_acquire);
  atomic_store_explicit(&hs_returned, 1, memory_order_release);
  return 0;
}

static int hash_spurious_test(void)
{
  int ret = 0;
  hs_val = 0;
  hs_parked = 0;
  hs_returned = 0;
  thrd_t thr;
  CHECK(thrd_create(&thr, hs_waiter, NULL) == thrd_success);
  test_wait_until("hs_parked", &hs_parked, 1);
  // Dummy notify while parked with the value unchanged: must re-park, not
  // return.
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&hs_val);
  thrd_sleep_ms(30);
  CHECK(atomic_load_explicit(&hs_returned, memory_order_acquire) == 0);
  // Real value change + notify releases it.
  atomic_store_explicit(&hs_val, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&hs_val);
  int wr;
  CHECK(thrd_join(thr, &wr) == thrd_success);
  CHECK(atomic_load_explicit(&hs_returned, memory_order_acquire) == 1);
  return ret;
}

int atomic_wait_widths_test_main(void)
{
  int ret = 0;
  SECTION("round-trip 1-byte (hash-table fallback)");
  ret += roundtrip_w1();
  SECTION("round-trip 2-byte (hash-table fallback)");
  ret += roundtrip_w2();
  SECTION("round-trip 8-byte (native or hash-table fallback)");
  ret += roundtrip_w8();
  SECTION("object isolation: wait on A is not woken by notify on B");
  ret += isolation_test();
  SECTION("spurious wake in hash-table path (explicit)");
  ret += hash_spurious_test();
  return ret;
}

int main(void)
{
  return atomic_wait_widths_test_main();
}
