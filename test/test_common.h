#pragma once

#include "wg14_atomic_waits/config.h"

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

#if __has_include(<threads.h>)
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

static inline void thrd_sleep_ms(unsigned ms)
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long) (ms % 1000) * 1000000L;
  thrd_sleep(&ts, WG14_ATOMIC_WAITS_NULLPTR);
}
