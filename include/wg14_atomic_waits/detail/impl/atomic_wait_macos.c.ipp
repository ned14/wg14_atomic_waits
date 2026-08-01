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
    uint32_t timeout_us = 0;
    if(ts != WG14_ATOMIC_WAITS_NULLPTR)
    {
      timeout_us = ts->tv_sec * 1000000U + ts->tv_nsec / 1000U;
      if(timeout_us == 0)
      {
        timeout_us = 1;
      }
    }
    const int ret = __ulock_wait(WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT,
                                 (uint32_t *) (uintptr_t) object,
                                 (uint64_t) expected, timeout_us);
    return ret;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address64)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, const struct timespec *ts)
  {
    uint32_t timeout_us = 0;
    if(ts != WG14_ATOMIC_WAITS_NULLPTR)
    {
      timeout_us = ts->tv_sec * 1000000U + ts->tv_nsec / 1000U;
      if(timeout_us == 0)
      {
        timeout_us = 1;
      }
    }
    const int ret = __ulock_wait(WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT64,
                                 (uint64_t *) (uintptr_t) object,
                                 (uint64_t) expected, timeout_us);
    return ret;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    const int ret = __ulock_wake(
    WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT |
    ((max_threads_to_wake == 1) ? WG14_ATOMIC_WAITS_ULF_WAKE_THREAD :
                                  WG14_ATOMIC_WAITS_ULF_WAKE_ALL),
    (uint32_t *) (uintptr_t) object,
    atomic_load_explicit(object,
                         WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_relaxed));
    return (ret != 0) ? ret : ((max_threads_to_wake == 1) ? 1 : (INT_MAX - 1));
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  unsigned max_threads_to_wake)
  {
    const int ret = __ulock_wake(
    WG14_ATOMIC_WAITS_UL_COMPARE_AND_WAIT64 |
    ((max_threads_to_wake == 1) ? WG14_ATOMIC_WAITS_ULF_WAKE_THREAD :
                                  WG14_ATOMIC_WAITS_ULF_WAKE_ALL),
    (uint64_t *) (uintptr_t) object,
    atomic_load_explicit(object,
                         WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_relaxed));
    return (ret != 0) ? ret : ((max_threads_to_wake == 1) ? 1 : (INT_MAX - 1));
  }

#include "atomic_wait_common.ipp.ipp"
