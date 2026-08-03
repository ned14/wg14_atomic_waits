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

#ifndef WG14_ATOMIC_WAITS_FREEBSD_IMPL_GUARD
#define WG14_ATOMIC_WAITS_FREEBSD_IMPL_GUARD

#if !defined(__FreeBSD__) && !defined(__FreeBSD_kernel__)
#error "atomic_wait_freebsd.c.ipp must only be included on FreeBSD"
#endif
#if __FreeBSD_version < 1200000
#error "atomic_wait_freebsd requires FreeBSD 12+"
#endif

#include "../../atomic_wait.h"

#include <errno.h>
#include <limits.h>
#include <sys/umtx.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, const struct timespec *duration)
  {
    int save_errno = errno;
    int ret;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      struct _umtx_time umtx_time;
      memset(&umtx_time, 0, sizeof(umtx_time));
      // The shared implementation passes a relative remaining duration. The
      // UMTX_ABSTIME flag is deliberately left clear: without it the kernel
      // treats _timeout as a relative interval and converts it to an absolute
      // deadline itself, matching the relative contract used by the other
      // backends. (The _clockid member is only honoured when UMTX_ABSTIME is
      // set, so it stays zero.)
      umtx_time._timeout = *duration;
      ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAIT_UINT,
                     expected, (long) &umtx_time);
    }
    else
    {
      // A NULL timeout means an infinite wait; passing a zeroed _umtx_time
      // would instead mean "relative timeout of zero" and return immediately.
      ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAIT_UINT,
                     expected, 0);
    }
    if(ret == 0)
    {
      errno = save_errno;
      return 0;
    }
    int e = errno;
    errno = save_errno;
    return -e;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address64)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, const struct timespec *duration)
  {
    int save_errno = errno;
    int ret;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      struct _umtx_time umtx_time;
      memset(&umtx_time, 0, sizeof(umtx_time));
      // Relative remaining duration; see wait_on_address32 for why the
      // UMTX_ABSTIME flag is deliberately left clear.
      umtx_time._timeout = *duration;
      ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAIT,
                     (long) expected, (long) &umtx_time);
    }
    else
    {
      // A NULL timeout means an infinite wait; see wait_on_address32.
      ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAIT,
                     (long) expected, 0);
    }
    if(ret == 0)
    {
      errno = save_errno;
      return 0;
    }
    int e = errno;
    errno = save_errno;
    return -e;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    // UMTX_OP_WAKE_UINT takes a long count and the kernel's umtxq_wake loop
    // breaks after (++count >= nr_wake) wake-ups. Callers pass (unsigned)-1 to
    // mean "wake all"; on 32-bit that casts to a negative long and the loop
    // stops after waking just one waiter. Clamp to a positive int so wake-all
    // really wakes every waiter.
    const long nr_wake = (max_threads_to_wake > (unsigned) INT_MAX) ?
                         (long) INT_MAX :
                         (long) max_threads_to_wake;
    long ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAKE_UINT,
                        nr_wake, 0);
    if(ret < 0)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return (int) ret;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    // See wake_by_address32 for the wake-all count clamping rationale.
    const long nr_wake = (max_threads_to_wake > (unsigned) INT_MAX) ?
                         (long) INT_MAX :
                         (long) max_threads_to_wake;
    long ret =
    _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAKE, nr_wake, 0);
    if(ret < 0)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return (int) ret;
  }

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_FREEBSD_IMPL_GUARD */
