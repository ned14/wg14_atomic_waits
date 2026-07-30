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
#include "atomic_wait_common.ipp.ipp"
#include <bsd/sys/ulock.h>
#include <errno.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(ulock_wait32)(
  const volatile _Atomic(uint_least32_t) *object, uint_least32_t expected,
  uint32_t *timeout_ns)
  {
    int ret = __ulock_wait(UL_COMPARE_AND_WAIT, (uint32_t *) (uintptr_t) object,
                           (uint64_t) expected, timeout_ns ? *timeout_ns : 0);
    if(ret == 0 || ret == EINTR)
      return 0;
    if(ret == ETIMEDOUT)
      return -1;
    return -1;
  }

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(ulock_wait64)(
  const volatile _Atomic(uint_least64_t) *object, uint_least64_t expected,
  uint32_t *timeout_ns)
  {
    int ret =
    __ulock_wait(UL_COMPARE_AND_WAIT64, (uint64_t *) (uintptr_t) object,
                 (uint64_t) expected, timeout_ns ? *timeout_ns : 0);
    if(ret == 0 || ret == EINTR)
      return 0;
    if(ret == ETIMEDOUT)
      return -1;
    return -1;
  }

  static WG14_ATOMIC_WAITS_INLINE unsigned WG14_ATOMIC_WAITS_PREFIX(
  ulock_wake32)(const volatile _Atomic(uint_least32_t) *object, unsigned count)
  {
    unsigned woke = 0;
    while(count-- > 0)
    {
      int ret =
      __ulock_wake(UL_COMPARE_AND_WAIT, (uint32_t *) (uintptr_t) object, 0);
      if(ret == 0 || ret == ENOENT)
        break;
      woke++;
    }
    return woke;
  }

  static WG14_ATOMIC_WAITS_INLINE unsigned WG14_ATOMIC_WAITS_PREFIX(
  ulock_wake64)(const volatile _Atomic(uint_least64_t) *object, unsigned count)
  {
    unsigned woke = 0;
    while(count-- > 0)
    {
      int ret =
      __ulock_wake(UL_COMPARE_AND_WAIT64, (uint64_t *) (uintptr_t) object, 0);
      if(ret == 0 || ret == ENOENT)
        break;
      woke++;
    }
    return woke;
  }

  // --- 4-byte: UL_COMPARE_AND_WAIT bypass ---

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
  const volatile _Atomic(uint_least32_t) *object, uint_least32_t expected,
  memory_order order)
  {
    uint32_t timeout_ns = 0;  // Infinite wait
    for(;;)
    {
      uint_least32_t current =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
      if(current != expected)
        return;
      int r = __ulock_wait(UL_COMPARE_AND_WAIT, (uint32_t *) (uintptr_t) object,
                           (uint64_t) expected, &timeout_ns);
      if(r == ETIMEDOUT)
        return;
    }
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i4)(
  const volatile _Atomic(int_least32_t) *object, int_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
    (const volatile _Atomic(uint_least32_t) *) object,
    (uint_least32_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_4)(
  const volatile _Atomic(uint_least32_t) *object, uint_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(object, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i4)(
  const volatile _Atomic(int_least32_t) *object, int_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
    (const volatile _Atomic(uint_least32_t) *) object,
    (uint_least32_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
  volatile _Atomic(uint_least32_t) *object)
  {
    __ulock_wake(UL_COMPARE_AND_WAIT, (uint32_t *) (uintptr_t) object, 0);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i4)(
  volatile _Atomic(int_least32_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
    (volatile _Atomic(uint_least32_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
  volatile _Atomic(uint_least32_t) *object)
  {
    __ulock_wake(UL_COMPARE_AND_WAIT, (uint32_t *) (uintptr_t) object,
                 UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i4)(
  volatile _Atomic(int_least32_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
    (volatile _Atomic(uint_least32_t) *) object);
  }

  // --- Sub-native-width: hash table fallback ---

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(
  const volatile _Atomic(uint_least8_t) *object, uint_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i1)(
  const volatile _Atomic(int_least8_t) *object, int_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) (uint_least8_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(
  const volatile _Atomic(uint_least16_t) *object, uint_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i2)(
  const volatile _Atomic(int_least16_t) *object, int_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) (uint_least16_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_1)(
  const volatile _Atomic(uint_least8_t) *object, uint_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i1)(
  const volatile _Atomic(int_least8_t) *object, int_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) (uint_least8_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_2)(
  const volatile _Atomic(uint_least16_t) *object, uint_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i2)(
  const volatile _Atomic(int_least16_t) *object, int_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) (uint_least16_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(
  volatile _Atomic(uint_least8_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i1)(
  volatile _Atomic(int_least8_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(
  volatile _Atomic(uint_least16_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i2)(
  volatile _Atomic(int_least16_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(
  volatile _Atomic(uint_least8_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i1)(
  volatile _Atomic(int_least8_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(
  volatile _Atomic(uint_least16_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i2)(
  volatile _Atomic(int_least16_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    UINT_MAX);
  }

  // --- Native-width expected + notify (4 byte) ---

  int WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(
  const volatile _Atomic(uint_least32_t) *restrict object,
  uint_least32_t *restrict expected, const struct timespec *restrict duration,
  memory_order success, memory_order failure)
  {
    struct timespec end;

    if(duration)
    {
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      end.tv_sec = now.tv_sec + duration->tv_sec;
      end.tv_nsec = now.tv_nsec + duration->tv_nsec;
      if(end.tv_nsec >= 1000000000)
      {
        end.tv_sec++;
        end.tv_nsec -= 1000000000;
      }
    }

    for(;;)
    {
      uint_least32_t current =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
      if(current != *expected)
      {
        *expected = current;
        atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
        return 0;
      }
      if(duration)
      {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if(now.tv_sec > end.tv_sec ||
           (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
        {
          *expected = current;
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
          return 0;
        }

        uint32_t timeout_ns;
        if(end.tv_sec == 0 && end.tv_nsec < UINT32_MAX)
          timeout_ns = (uint32_t) end.tv_nsec;
        else if(end.tv_sec >= 4)
          timeout_ns = UINT32_MAX;
        else
        {
          uint64_t total =
          (uint64_t) end.tv_sec * 1000000000ull + (uint64_t) end.tv_nsec;
          timeout_ns = (uint32_t) (total > UINT32_MAX ? UINT32_MAX : total);
        }

        uint_least32_t current2;
        int r = ulock_wait32(object, *expected, &timeout_ns);
        current2 =
        atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
        *expected = current2;
        if(r < 0)
        {
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
          return 0;
        }
        if(current2 != *expected)
        {
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
          return 1;
        }
        // Timeout but still equal, continue outer loop
      }
      else
      {
        uint_least32_t current2;
        int r = ulock_wait32(object, *expected, NULL);
        current2 =
        atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
        *expected = current2;
        if(r < 0)
        {
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
          return 0;
        }
        if(current2 != *expected)
        {
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
          return 1;
        }
      }
    }
  }

  int WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(
  volatile _Atomic(uint_least32_t) *restrict object,
  uint_least32_t *restrict expected, uint_least32_t desired,
  unsigned max_threads_to_wake, memory_order success, memory_order failure)
  {
    uint_least32_t original = *expected;
    if(!atomic_compare_exchange_strong_explicit(
       (volatile _Atomic(uint_least32_t) *) object, expected, desired,
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX success,
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure))
    {
      return 0;
    }
    if(max_threads_to_wake == 0)
      return 1;
    unsigned woke = WG14_ATOMIC_WAITS_PREFIX(ulock_wake32)(
    (const volatile _Atomic(uint_least32_t) *) object, max_threads_to_wake);
    return 1 + woke;
  }

#ifdef __cplusplus
}
#endif
