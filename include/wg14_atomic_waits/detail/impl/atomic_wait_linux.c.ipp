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

#if !defined(__linux__)
#error "atomic_wait_linux.c.ipp must only be included on Linux"
#endif

#include "../../atomic_wait.h"
#include "atomic_wait_common.ipp.ipp"
#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(futex_wait)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, uint_least32_t expected,
  const struct timespec *abs_timeout)
  {
    int save_errno = errno;
    int ret = (int) syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAIT,
                            expected, abs_timeout, NULL, 0);
    if(ret == 0)
    {
      errno = save_errno;
      return 0;
    }
    if(errno == EAGAIN || errno == EINTR)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return -1;
  }

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(futex_wake)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, unsigned count)
  {
    int save_errno = errno;
    long ret = syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAKE, count,
                       NULL, NULL, 0);
    if(ret < 0)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return (int) ret;
  }

  // --- 4-byte: futex bypass ---

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, uint_least32_t expected,
  memory_order order)
  {
    for(;;)
    {
      uint_least32_t current =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
      if(current != expected)
        return;
      futex_wait(object, expected, NULL);
    }
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object, int_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
    (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object,
    (uint_least32_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, uint_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(object, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object, int_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
    (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object,
    (uint_least32_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object)
  {
    futex_wake((const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, 1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
    (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object)
  {
    futex_wake((const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, INT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
    (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object);
  }

  // --- 4-byte native-width expected + notify ---

  int WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *restrict object,
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
        for(;;)
        {
          struct timespec now;
          clock_gettime(CLOCK_MONOTONIC, &now);
          if(now.tv_sec > end.tv_sec ||
             (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
          {
            *expected = atomic_load_explicit(
            object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
            atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
            return 0;
          }
          int r = futex_wait(object, *expected, &end);
          if(r < 0 && errno == ETIMEDOUT)
          {
            *expected = atomic_load_explicit(
            object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
            atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
            return 0;
          }
          // Woke up - check for value change
          uint_least32_t current2 =
          atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
          if(current2 != *expected)
          {
            *expected = current2;
            atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
            return 1;
          }
          // Spurious wake, retry
        }
      }
      else
      {
        for(;;)
        {
          int r = futex_wait(object, *expected, NULL);
          uint_least32_t current2 =
          atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
          if(current2 != *expected)
          {
            *expected = current2;
            atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
            return 1;
          }
          if(r < 0 && errno == ETIMEDOUT)
          {
            *expected = current2;
            atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
            return 0;
          }
        }
      }
    }
  }

  int WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *restrict object,
  uint_least32_t *restrict expected, uint_least32_t desired,
  unsigned max_threads_to_wake, memory_order success, memory_order failure)
  {
    (void) failure;
    uint_least32_t exp = *expected;
    (void) exp;
    if(!atomic_compare_exchange_strong_explicit(
       (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, expected, desired,
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX success,
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure))
    {
      return 0;
    }
    if(max_threads_to_wake == 0)
      return 1;
    int woke = 0;
    for(unsigned i = 0; i < max_threads_to_wake; i++)
    {
      int n = futex_wake((const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, 1);
      if(n <= 0)
        break;
      woke += n;
    }
    return 1 + woke;
  }

  // --- Sub-native-width and 8-byte: hash table fallback ---

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object, uint_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i1)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least8_t *object, int_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) (uint_least8_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object, uint_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i2)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least16_t *object, int_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) (uint_least16_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object, uint_least64_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 8, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least64_t *object, int_least64_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 8, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_1)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object, uint_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i1)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least8_t *object, int_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 1, (uint64_t) (uint_least8_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_2)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object, uint_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i2)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least16_t *object, int_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 2, (uint64_t) (uint_least16_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object, uint_least64_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 8, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least64_t *object, int_least64_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 8, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least8_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least16_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 8,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least64_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 8,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least8_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 1,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least16_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 2,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 8,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least64_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 8,
                                                    UINT_MAX);
  }

#ifdef __cplusplus
}
#endif
