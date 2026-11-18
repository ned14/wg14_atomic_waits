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

#ifndef WG14_ATOMIC_WAITS_LINUX_IMPL_GUARD
#define WG14_ATOMIC_WAITS_LINUX_IMPL_GUARD

#if !defined(__linux__)
#error "atomic_wait_linux.c.ipp must only be included on Linux"
#endif

#include "../../atomic_wait.h"

#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#if !WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT
#include <time.h>
#endif
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, const uint_least64_t *timeout_ns)
  {
    int save_errno = errno;
    // The shared implementation passes a relative remaining timeout (NULL for
    // an infinite wait), but the futex wait operations only accept an absolute
    // CLOCK_MONOTONIC deadline. Convert the relative duration to an absolute
    // one here, otherwise a small relative value would be interpreted as a
    // deadline in the distant past and the wait would return immediately
    // (busy-spinning until expiry).
    //
    // Use FUTEX_WAIT_BITSET rather than plain FUTEX_WAIT: some virtualised
    // kernels (e.g. Ubuntu 24.04/26.04 guests) fail to arm the wake timer for
    // plain FUTEX_WAIT timeouts, silently leaving the waiter asleep past its
    // deadline, whereas the BITSET variant (which glibc itself uses for
    // pthread_cond_timedwait) arms the hrtimer correctly. With
    // FUTEX_BITSET_MATCH_ANY it is semantically identical to FUTEX_WAIT.
    int ret;
#if WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT
    (void) timeout_ns;
    ret =
    (int) syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAIT_BITSET,
                  (int) expected, NULL, NULL, FUTEX_BITSET_MATCH_ANY);
#else
  if(timeout_ns != WG14_ATOMIC_WAITS_NULLPTR)
  {
    struct timespec now;
    struct timespec abstime;
    (void) clock_gettime(CLOCK_MONOTONIC, &now);
    uint_least64_t ns =
    (uint_least64_t) now.tv_sec * 1000000000ull + (uint_least64_t) now.tv_nsec;
    if(*timeout_ns > UINT_LEAST64_MAX - ns)
    {
      ns = UINT_LEAST64_MAX;
    }
    else
    {
      ns += *timeout_ns;
    }
    // Clamp to a conservative signed-32-bit time_t range so the conversion
    // is safe even on 32-bit targets. Longer waits simply re-park.
    uint_least64_t secs = ns / 1000000000ull;
    if(secs > 0x7FFFFFFFull)
    {
      secs = 0x7FFFFFFFull;
    }
    abstime.tv_sec = (time_t) secs;
    abstime.tv_nsec = (long) (ns % 1000000000ull);
    ret =
    (int) syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAIT_BITSET,
                  (int) expected, &abstime, NULL, FUTEX_BITSET_MATCH_ANY);
  }
  else
  {
    ret =
    (int) syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAIT_BITSET,
                  (int) expected, NULL, NULL, FUTEX_BITSET_MATCH_ANY);
  }
#endif
    if(ret == 0)
    {
      errno = save_errno;
      return 0;
    }
    int e = errno;
    errno = save_errno;
    return -e;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 0

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    // FUTEX_WAKE takes a signed int count. Callers pass (unsigned)-1 to mean
    // "wake all"; passed through unclamped that becomes -1, and the kernel's
    // futex_wake() wake loop breaks after waking just one waiter
    // (if (++ret >= nr_wake) break), so atomic_notify_all would leave every
    // other parked waiter asleep forever. Clamp to INT_MAX so that wake-all
    // really wakes every waiter parked on the futex.
    const int nr_wake = (max_threads_to_wake > (unsigned) INT_MAX) ?
                        INT_MAX :
                        (int) max_threads_to_wake;
    long ret = syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAKE,
                       nr_wake, NULL, NULL, 0);
    if(ret < 0)
    {
      int e = errno;
      errno = save_errno;
      return -e;
    }
    errno = save_errno;
    return (int) ret;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 0

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_LINUX_IMPL_GUARD */
