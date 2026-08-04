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
// __FreeBSD__ is the compiler-defined major release (e.g. 15 on FreeBSD 15).
#if defined(__FreeBSD__) && __FreeBSD__ < 12
#error "atomic_wait_freebsd requires FreeBSD 12+"
#endif

#include "../../atomic_wait.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
// _umtx_op() in <sys/umtx.h> is prototyped with u_long, which <sys/types.h>
// provides only when __BSD_VISIBLE is set. Strict POSIX feature-test macros
// (the CI passes -D_POSIX_C_SOURCE / -D_XOPEN_SOURCE) clear __BSD_VISIBLE, so
// supply u_long ourselves. Redefining a typedef to the same type is permitted
// by C11 (and C++11), so this is a no-op where it is already defined.
typedef unsigned long u_long;
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
      // The shared implementation passes a relative remaining duration. Leave
      // the UMTX_ABSTIME flag clear so the kernel converts _timeout to an
      // absolute deadline itself, and set _clockid to CLOCK_MONOTONIC so that
      // conversion is measured against the same monotonic clock the shared
      // code uses for its own deadline tracking (the kernel defaults the
      // clock to CLOCK_REALTIME when it is not supplied, which would disagree
      // with that tracking).
      umtx_time._clockid = CLOCK_MONOTONIC;
      umtx_time._flags = 0;
      umtx_time._timeout = *duration;
      // For the WAIT ops the kernel uses uaddr1 (the 4th argument) as the size
      // of the struct pointed to by uaddr2 (the 5th argument): a size no
      // larger than struct timespec copies just a relative timespec, a larger
      // size copies the full _umtx_time including _clockid/_flags. uaddr2 ==
      // NULL means an infinite wait, so it must be non-NULL when a timeout is
      // supplied.
      ret = _umtx_op((void *) (uintptr_t) object, UMTX_OP_WAIT_UINT, expected,
                     (void *) (uintptr_t) sizeof(umtx_time), &umtx_time);
    }
    else
    {
      // A NULL uaddr2 means an infinite wait (passing a zeroed _umtx_time
      // would instead mean "relative timeout of zero" and return immediately).
      ret =
      _umtx_op((void *) (uintptr_t) object, UMTX_OP_WAIT_UINT, expected, 0, 0);
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
      // Relative remaining duration on CLOCK_MONOTONIC; see wait_on_address32
      // for the UMTX_ABSTIME/_clockid reasoning and the uaddr1/uaddr2 layout.
      umtx_time._clockid = CLOCK_MONOTONIC;
      umtx_time._flags = 0;
      umtx_time._timeout = *duration;
      ret = _umtx_op((void *) (uintptr_t) object, UMTX_OP_WAIT, (long) expected,
                     (void *) (uintptr_t) sizeof(umtx_time), &umtx_time);
    }
    else
    {
      // A NULL uaddr2 means an infinite wait; see wait_on_address32.
      ret = _umtx_op((void *) (uintptr_t) object, UMTX_OP_WAIT, (long) expected,
                     0, 0);
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
    // UMTX_OP_WAKE takes a long count and the kernel's umtxq_wake loop breaks
    // after (++count >= nr_wake) wake-ups. Callers pass (unsigned)-1 to mean
    // "wake all"; clamp to a positive int so wake-all really wakes every
    // waiter (a negative count on 32-bit long would stop after one waiter).
    const long nr_wake = (max_threads_to_wake > (unsigned) INT_MAX) ?
                         (long) INT_MAX :
                         (long) max_threads_to_wake;
    // Modern FreeBSD has no UMTX_OP_WAKE_UINT: the 32-bit wake queue is
    // selected by ORing the UMTX_OP__32BIT flag into UMTX_OP_WAKE.
    long ret = _umtx_op((void *) (uintptr_t) object,
                        UMTX_OP_WAKE | UMTX_OP__32BIT, nr_wake, 0, 0);
    if(ret < 0)
    {
      int e = errno;
      errno = save_errno;
      return -e;
    }
    errno = save_errno;
    // The kernel's WAKE never reports how many threads were woken, so mirror
    // the macOS/Windows success convention: 1 for a single wake, a large
    // count for a wake-all.
    return (max_threads_to_wake == 1) ? 1 : (INT_MAX - 1);
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
    _umtx_op((void *) (uintptr_t) object, UMTX_OP_WAKE, nr_wake, 0, 0);
    if(ret < 0)
    {
      int e = errno;
      errno = save_errno;
      return -e;
    }
    errno = save_errno;
    // The kernel's WAKE never reports how many threads were woken; see
    // wake_by_address32 for the fabricated success count.
    return (max_threads_to_wake == 1) ? 1 : (INT_MAX - 1);
  }

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_FREEBSD_IMPL_GUARD */
