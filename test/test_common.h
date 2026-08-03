#pragma once

#include "wg14_atomic_waits/config.h"
#include <wg14_atomic_waits/atomic_wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STRINGISE2(x) #x
#define STRINGISE(x) STRINGISE2(x)
#define CHECK(x)                                                               \
  if(!(x))                                                                     \
  {                                                                            \
    fprintf(stderr, "CHECK(" STRINGISE(x) ") failed at " __FILE__              \
                                          ":" STRINGISE(__LINE__) "\n");       \
    ret++;                                                                     \
  }

// TSAN detection: GCC defines __SANITIZE_THREAD__; Clang exposes
// __has_feature(thread_sanitizer). The __has_feature probe is nested so that
// compilers without __has_feature support (which would report a preprocessor
// parse error if it appeared inside an #if expression) skip it entirely.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define WG14_ATOMIC_WAITS_TEST_TSAN 1
#endif
#endif
#if !defined(WG14_ATOMIC_WAITS_TEST_TSAN) && defined(__SANITIZE_THREAD__)
#define WG14_ATOMIC_WAITS_TEST_TSAN 1
#endif

// Prefer the real C11 <threads.h> API when available. Exception: glibc's
// thrd_create() calls pthread_create() from inside libc, which bypasses TSan's
// pthread_create interceptor. The spawned thread is then never registered with
// TSan and crashes the moment it runs instrumented code (SEGV at thr+0x18
// inside __tsan_func_entry). On glibc under TSan only, use the pthread-based
// fallback below so thread creation goes through the interposable
// pthread_create() symbol in this TU, which TSan does intercept. Other libcs
// (macOS, musl, FreeBSD, ...) do not have this problem.
#if __has_include(<threads.h>) && \
    !(defined(__GLIBC__) && defined(WG14_ATOMIC_WAITS_TEST_TSAN))
#include <threads.h>
#else

#include <pthread.h>

#define thrd_success 0

typedef int (*thrd_start_t)(void *);
typedef struct thrd_s
{
  void *arg;
  int res;
  thrd_start_t func;
  pthread_t thread;
} *thrd_t;

static inline void *thrd_runner(void *arg)
{
  thrd_t thr = (thrd_t) arg;
  thr->res = thr->func(thr->arg);
  return WG14_ATOMIC_WAITS_NULLPTR;
}

static inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
  thrd_t ret = (thrd_t) calloc(1, sizeof(struct thrd_s));
  ret->arg = arg;
  ret->res = 0;
  ret->func = func;
  *thr = ret;
  return pthread_create(&ret->thread, WG14_ATOMIC_WAITS_NULLPTR, thrd_runner,
                        ret);
}

static inline int thrd_join(thrd_t thr, int *res)
{
  int ret = pthread_join(thr->thread, WG14_ATOMIC_WAITS_NULLPTR);
  if(ret != -1)
  {
    *res = thr->res;
  }
  free(thr);
  return ret;
}

static inline int thrd_sleep(const struct timespec *duration,
                             struct timespec *remaining)
{
  return nanosleep(duration, remaining);
}

#endif

#ifdef WG14_ATOMIC_WAITS_TEST_TSAN
#undef WG14_ATOMIC_WAITS_TEST_TSAN
#endif

static inline void thrd_sleep_ms(unsigned ms)
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long) (ms % 1000) * 1000000L;
  thrd_sleep(&ts, WG14_ATOMIC_WAITS_NULLPTR);
}

// Bounded spin on a handshake flag or counter (AGENTS.md rule 5: sleeps only
// inside the proper spin synchronisation). Waits until `*value >= goal` (use
// goal 1 for a flag), then returns. If the goal is not reached within 2000 ms
// the process is aborted with a diagnostic, because continuing would otherwise
// hang on joins of threads that never parked or woke.
static inline void
test_wait_until(const char *what,
                const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int *value,
                int goal)
{
  struct timespec start;
  timespec_get(&start, TIME_UTC);
  while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
        value, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_acquire) < goal)
  {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    const long ms = (long) ((now.tv_sec - start.tv_sec) * 1000L +
                            (now.tv_nsec - start.tv_nsec) / 1000000L);
    if(ms > 2000)
    {
      fprintf(stderr,
              "FATAL: timeout waiting for %s to reach %d after "
              "2000 ms; aborting\n",
              what, goal);
      abort();
    }
    thrd_sleep_ms(1);
  }
}
