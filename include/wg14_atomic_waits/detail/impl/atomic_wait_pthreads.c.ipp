/* Proposed WG14 atomic wait/notify support
(C) 2026 Niall Douglas <http://www.nedproductions.biz/>
File Created: Jul 2026


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef WG14_ATOMIC_WAITS_PTHREADS_IMPL_GUARD
#define WG14_ATOMIC_WAITS_PTHREADS_IMPL_GUARD

#ifndef __linux__
#ifndef __FreeBSD__
#ifndef __APPLE__
#error                                                                         \
"atomic_wait_pthreads.c.ipp must only be included on non-Linux non-FreeBSD non-Apple POSIX"
#endif
#endif
#endif

#include "../../atomic_wait.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // Use CLOCK_MONOTONIC for timed waits where available and where the
  // pthread_condattr_setclock() declaration is exposed by the feature-test
  // macros used to build this library (_POSIX_C_SOURCE=200809L). Apple hides
  // that declaration under the same feature-test macros, but the Apple backend
  // never reaches this file, so falling back to the default (realtime) clock
  // there is harmless.
#if defined(CLOCK_MONOTONIC) && !defined(__APPLE__)
#define WG14_ATOMIC_WAITS_PTHREADS_MONOTONIC 1
#else
#define WG14_ATOMIC_WAITS_PTHREADS_MONOTONIC 0
#endif

  // Per-object wait/notify state. Each proxy holds a mutex, a condition
  // variable and a count of outstanding "wake tokens". Serialising the wait
  // decision and the wake on the *proxy's own* mutex (rather than a per-thread
  // mutex) prevents the classic condvar lost wake: a notify that occurs between
  // the waiter's re-check and its park is recorded as a pending token, so the
  // waiter observes it on the next park attempt instead of sleeping forever.
  typedef struct WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_s)
  {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int pending;  // number of queued wake tokens not yet consumed
    int waiting;  // number of threads currently parked on this proxy
  } WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_t);

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(
  pthread_proxy_init)(WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_t) * p)
  {
    int save_errno = errno;
    int ret = pthread_mutex_init(&p->mutex, WG14_ATOMIC_WAITS_NULLPTR);
    if(ret != 0)
    {
      errno = save_errno;
      return -ret;
    }
#if WG14_ATOMIC_WAITS_PTHREADS_MONOTONIC
    pthread_condattr_t attr;
    ret = pthread_condattr_init(&attr);
    if(ret == 0)
    {
      pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
      ret = pthread_cond_init(&p->cond, &attr);
      pthread_condattr_destroy(&attr);
    }
    if(ret != 0)
    {
      pthread_mutex_destroy(&p->mutex);
      errno = save_errno;
      return -ret;
    }
#else
  ret = pthread_cond_init(&p->cond, WG14_ATOMIC_WAITS_NULLPTR);
  if(ret != 0)
  {
    pthread_mutex_destroy(&p->mutex);
    errno = save_errno;
    return -ret;
  }
#endif
    p->pending = 0;
    p->waiting = 0;
    errno = save_errno;
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(
  pthread_proxy_destroy)(WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_t) * p)
  {
    int save_errno = errno;
    const int ret1 = pthread_cond_destroy(&p->cond);
    const int ret2 = pthread_mutex_destroy(&p->mutex);
    errno = save_errno;
    return (ret1 != 0) ? ret1 : ret2;
  }

  // Returns 0 on success (woken or a pending token was consumed), -ETIMEDOUT on
  // timeout, or a negative error on failure.
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wait)(
  WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_t) * p,
  const struct timespec *timeout)  // relative timeout, or NULL for infinite
  {
    int save_errno = errno;
    int ret = pthread_mutex_lock(&p->mutex);
    if(ret != 0)
    {
      errno = save_errno;
      return -ret;
    }
    // A notify fired before we parked: consume the pending token and return
    // immediately so the caller re-checks the object value.
    if(p->pending > 0)
    {
      p->pending--;
      (void) pthread_mutex_unlock(&p->mutex);
      errno = save_errno;
      return 0;
    }
    struct timespec abstime;
    if(timeout != WG14_ATOMIC_WAITS_NULLPTR)
    {
#if WG14_ATOMIC_WAITS_PTHREADS_MONOTONIC
      (void) clock_gettime(CLOCK_MONOTONIC, &abstime);
#else
    (void) clock_gettime(CLOCK_REALTIME, &abstime);
#endif
      abstime.tv_sec += timeout->tv_sec;
      abstime.tv_nsec += timeout->tv_nsec;
      if(abstime.tv_nsec >= 1000000000L)
      {
        abstime.tv_sec++;
        abstime.tv_nsec -= 1000000000L;
      }
    }
    p->waiting++;
    for(;;)
    {
      if(timeout != WG14_ATOMIC_WAITS_NULLPTR)
      {
        ret = pthread_cond_timedwait(&p->cond, &p->mutex, &abstime);
      }
      else
      {
        ret = pthread_cond_wait(&p->cond, &p->mutex);
      }
      if(ret == 0)
      {
        if(p->pending > 0)
        {
          // We were woken by a notify (or a spurious wake collided with a
          // pending token); consume one token and return.
          p->pending--;
          p->waiting--;
          (void) pthread_mutex_unlock(&p->mutex);
          errno = save_errno;
          return 0;
        }
        // Spurious wake with no token: loop and keep waiting.
        continue;
      }
      if(ret == ETIMEDOUT)
      {
        p->waiting--;
        (void) pthread_mutex_unlock(&p->mutex);
        errno = save_errno;
        return -ETIMEDOUT;
      }
      // Genuine error.
      p->waiting--;
      (void) pthread_mutex_unlock(&p->mutex);
      errno = save_errno;
      return -ret;
    }
  }

  // Wakes up to `max_threads_to_wake` parked threads on this proxy.
  // Returns a positive count on success, or a negative error on failure.
  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(
  pthread_proxy_wake)(WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_t) * p,
                      unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    int ret = pthread_mutex_lock(&p->mutex);
    if(ret != 0)
    {
      errno = save_errno;
      return -ret;
    }
    if(max_threads_to_wake == (unsigned) -1)
    {
      p->pending = INT_MAX;
    }
    else
    {
      const long np = (long) p->pending + (long) max_threads_to_wake;
      p->pending = (np > INT_MAX) ? INT_MAX : (int) np;
    }
    ret = (max_threads_to_wake == 1) ? pthread_cond_signal(&p->cond) :
                                       pthread_cond_broadcast(&p->cond);
    int woken;
    if(max_threads_to_wake == (unsigned) -1)
    {
      woken = p->waiting;
    }
    else if(p->waiting < (int) max_threads_to_wake)
    {
      woken = p->waiting;
    }
    else
    {
      woken = (int) max_threads_to_wake;
    }
    if(woken < 1)
    {
      woken = 1;  // keep the return strictly positive on success
    }
    (void) pthread_mutex_unlock(&p->mutex);
    if(ret != 0)
    {
      errno = save_errno;
      return -ret;
    }
    errno = save_errno;
    return woken;
  }

  // Embed the proxy state as the object-level wait/notify store.
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE                           \
  WG14_ATOMIC_WAITS_PREFIX(wg14_pthreads_proxy_t)
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_INIT(x)                   \
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_init)(&(x)->atomic)
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_DESTROY(x)                \
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_destroy)(&(x)->atomic)
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAIT_GET_COUNTER(x) 0
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAIT(x, counter, timeout) \
  (                                                                            \
  (void) (counter),                                                            \
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wait)(&(x)->atomic, (timeout)))
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAKE(x,                   \
                                                          max_threads_to_wake) \
  WG14_ATOMIC_WAITS_PREFIX(pthread_proxy_wake)(&(x)->atomic,                   \
                                               (max_threads_to_wake))

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 0
#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 0
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 0
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 0

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_PTHREADS_IMPL_GUARD */
