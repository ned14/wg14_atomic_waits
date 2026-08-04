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

#ifndef WG14_ATOMIC_WAITS_MACOS_IMPL_GUARD
#define WG14_ATOMIC_WAITS_MACOS_IMPL_GUARD

#if !defined(__APPLE__)
#error "atomic_wait_macos.c.ipp must only be included on macOS/iOS"
#endif

#include "../../atomic_wait.h"

#include <errno.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // From <bsd/sys/ulock.h> which isn't in the public SDK headers
  //
  // New Mac OS versions have os_sync_wait_on_address_with_timeout() et al, but
  // they're too new at the time of writing this code.
  extern int
  __ulock_wait(uint32_t operation, void *addr, uint64_t value,
               uint32_t timeout); /* timeout is specified in microseconds */
  extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);
#define WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT 1
#define WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT64 5
#define WG14_ATOMIC_WAITS_ULF_WAKE_ALL 0x00000100
#define WG14_ATOMIC_WAITS_ULF_WAKE_THREAD 0x00000200


#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, const struct timespec *ts)
  {
    int save_errno = errno;
    uint32_t timeout_us = 0;
    if(ts != WG14_ATOMIC_WAITS_NULLPTR)
    {
      uint64_t us = ((uint64_t) ts->tv_sec) * 1000000ull +
                    (((uint64_t) ts->tv_nsec) + 999ull) / 1000ull;
      timeout_us = (us > UINT32_MAX) ? UINT32_MAX : (uint32_t) us;
      if(timeout_us == 0)
      {
        timeout_us = 1;
      }
    }
    const int ret = __ulock_wait(WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT,
                                 (uint32_t *) (uintptr_t) object,
                                 (uint64_t) expected, timeout_us);
    if(ret == 0)
    {
      errno = save_errno;
      return 0;
    }
    // __ulock_wait returns -1 and sets errno; report it as -errno like the
    // other backends so callers can distinguish retryable conditions.
    const int e = errno;
    errno = save_errno;
    return -e;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address64)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, const struct timespec *ts)
  {
    int save_errno = errno;
    uint32_t timeout_us = 0;
    if(ts != WG14_ATOMIC_WAITS_NULLPTR)
    {
      uint64_t us = ((uint64_t) ts->tv_sec) * 1000000ull +
                    (((uint64_t) ts->tv_nsec) + 999ull) / 1000ull;
      timeout_us = (us > UINT32_MAX) ? UINT32_MAX : (uint32_t) us;
      if(timeout_us == 0)
      {
        timeout_us = 1;
      }
    }
    const int ret = __ulock_wait(WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT64,
                                 (uint64_t *) (uintptr_t) object,
                                 (uint64_t) expected, timeout_us);
    if(ret == 0)
    {
      errno = save_errno;
      return 0;
    }
    // __ulock_wait returns -1 and sets errno; report it as -errno like the
    // other backends so callers can distinguish retryable conditions.
    const int e = errno;
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
    const int ret = __ulock_wake(
    WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT |
    ((max_threads_to_wake == 1) ? 0 : WG14_ATOMIC_WAITS_ULF_WAKE_ALL),
    (uint32_t *) (uintptr_t) object, 0);
    if(ret != 0 && errno != ENOENT)
    {
      // Genuine wake failure: report it as -errno like the other backends.
      const int e = errno;
      errno = save_errno;
      return -e;
    }
    // Success, or ENOENT because no thread is parked on the address — a normal
    // "nothing to wake" outcome, the analogue of a futex wake reporting zero.
    // Treat both as a successful wake so atomic_notify stays positive on a
    // successful exchange, consistent with the Windows/FreeBSD backends, and
    // restore errno so the suppressed ENOENT does not leak to the caller.
    errno = save_errno;
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
    const int ret = __ulock_wake(
    WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT64 |
    ((max_threads_to_wake == 1) ? 0 : WG14_ATOMIC_WAITS_ULF_WAKE_ALL),
    (uint64_t *) (uintptr_t) object, 0);
    if(ret != 0 && errno != ENOENT)
    {
      // Genuine wake failure: report it as -errno like the other backends.
      const int e = errno;
      errno = save_errno;
      return -e;
    }
    // ENOENT means no thread is parked on the address (see wake_by_address32);
    // treat it as a successful wake with nothing woken, not an error.
    errno = save_errno;
    return (max_threads_to_wake == 1) ? 1 : (INT_MAX - 1);
  }

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_MACOS_IMPL_GUARD */
