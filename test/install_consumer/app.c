// Install-consumer smoke test: verifies that a find_package consumer of the
// *installed* wg14_atomic_waits package can compile, link and run against the
// public API. It deliberately avoids threads: its job is to catch install /
// export regressions (missing headers, broken Config.cmake, missing version
// file, wrong INTERFACE_* properties), not to test the library's concurrency
// (that is covered by the in-tree tests).

// Mirror the feature-test macros the library itself is built with so
// clock_gettime etc. are declared on glibc when this header-only package is
// consumed directly (see hash_table_whitebox_test.c).
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <wg14_atomic_waits/atomic_wait.h>

#include <stdio.h>

static int checks = 0;

#define CHECK(x)                                                               \
  do                                                                           \
  {                                                                            \
    if(!(x))                                                                   \
    {                                                                          \
      fprintf(stderr, "install consumer: CHECK(" #x ") failed at line %d\n",   \
              __LINE__);                                                       \
      return 1;                                                                \
    }                                                                          \
    checks++;                                                                  \
  } while(0)

int main(void)
{
  // atomic_wait / atomic_wait_explicit: *object != expected returns
  // immediately.
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int v = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&v, 1);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit)(&v, 1, memory_order_acquire);

  // atomic_notify_one / atomic_notify_all on a 4-byte object.
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&v);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(&v);

  // atomic_wait_expected: immediate mismatch returns 0 and reloads *expected.
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t nv = 0;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 5;
  const int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &nv, &expected, WG14_ATOMIC_WAITS_NULLPTR, memory_order_seq_cst,
  memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected == 0);

  // atomic_notify: successful exchange 0 -> 3 returns positive and stores 3.
  expected = 0;
  const WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) desired = 3;
  const int nr = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
  &nv, &expected, desired, 1, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(nr > 0);
  CHECK(atomic_load_explicit(&nv, memory_order_seq_cst) == 3);

  // atomic_notify CAS failure returns 0 and updates *expected.
  expected = 9;
  const int nf = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
  &nv, &expected, 7, 1, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(nf == 0);
  CHECK(expected == 3);

  fprintf(stderr, "install consumer: %d checks passed\n", checks);
  return 0;
}
