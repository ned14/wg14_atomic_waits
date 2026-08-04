// White-box regression test for the pthreads backend's lost-wake race that
// intermittently hung `atomic_wait_widths_test` on the Linux pthread-backed
// CI. A wake-all capped its pending-token pile at the number of *parked*
// waiters, so a waiter that had re-checked the object value under the
// hash-table lock but had not yet parked (registered, but not counted in
// `waiting`) consumed no token after the parked waiters drained the pile and
// parked forever despite the value having changed. The backend .ipp is
// included directly and pthread_proxy_wait / pthread_proxy_wake are driven
// single-threaded, so the exact interleaving is deterministic on every
// platform — no reliance on thread scheduling, unlike the two-waiter public-API
// round-trip that exposed it only under CI load.
//
// The feature-test macros below mirror the compiled library's build
// (CMakeLists.txt) so clock_gettime etc. are declared on glibc.

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// The library target propagates WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY in the
// header-only build; drop it here so atomic_wait.h (via test_common.h) only
// declares the API and does not auto-include a platform backend, leaving the
// pthreads .ipp below as the sole backend in this TU.
#undef WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY

#include "test_common.h"
#include <wg14_atomic_waits/detail/impl/atomic_wait_pthreads.c.ipp>

// POSIX only: the included backend is a no-op on Windows (no <pthread.h>).
int pthreads_proxy_whitebox_test_main(void)
{
  int ret = 0;
  WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_t) proxy;
  struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000};  // 100 ms

  // 1. Wake-all must create one token per *registered* waiter, not per parked
  //    waiter. One waiter is parked (waiting == 1); a second has registered
  //    (registered == 2) but is still racing the park. The parked waiter
  //    consumes a token and exits; the in-flight waiter must still find one,
  //    otherwise it parks forever (the regression). A short timeout makes the
  //    pre-fix failure fail fast instead of hanging.
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_init)(&proxy) == 0);
  proxy.waiting = 1;
  proxy.pending = 0;
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wake)(&proxy, (unsigned) -1, 2);
  CHECK(proxy.pending == 2);
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wait)(
        &proxy, WG14_ATOMIC_WAITS_NULLPTR) == 0);
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wait)(&proxy, &ts) == 0);
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_destroy)(&proxy);

  // 2. Repeated wake-alls must keep the pile covering the registered count
  //    (the in-flight waiter must never be dropped) without ever exceeding it
  //    (the spurious-notify busy-spin protection). Pre-fix the pile sat at 1,
  //    the parked count, so the in-flight waiter was covered by none of them.
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_init)(&proxy) == 0);
  proxy.waiting = 1;
  proxy.pending = 0;
  for(int i = 0; i < 10; i++)
  {
    WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wake)(&proxy, (unsigned) -1, 2);
    CHECK(proxy.pending == 2);
    CHECK(proxy.pending <= 2);
  }
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_destroy)(&proxy);

  // 3. Repeated notify_ones accumulate tokens but stay bounded by the
  //    registered count, and every registered waiter is still served.
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_init)(&proxy) == 0);
  proxy.waiting = 2;
  proxy.pending = 0;
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wake)(&proxy, 1, 2);
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wake)(&proxy, 1, 2);
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wake)(&proxy, 1, 2);
  CHECK(proxy.pending == 2);
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wait)(
        &proxy, WG14_ATOMIC_WAITS_NULLPTR) == 0);
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wait)(&proxy, &ts) == 0);
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_destroy)(&proxy);

  // 4. A wake-all on a proxy with no registered waiter still leaves one token
  //    for a waiter racing the park (the cap's floor-of-one fallback).
  CHECK(WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_init)(&proxy) == 0);
  proxy.waiting = 0;
  proxy.pending = 0;
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wake)(&proxy, (unsigned) -1, 0);
  CHECK(proxy.pending == 1);
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_destroy)(&proxy);

  return ret;
}

int main(void)
{
  return pthreads_proxy_whitebox_test_main();
}
