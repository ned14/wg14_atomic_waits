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

#if !defined(__FreeBSD__) && !defined(__FreeBSD_kernel__)
#error "atomic_wait_freebsd.c.ipp must only be included on FreeBSD"
#endif
#if __FreeBSD_version < 1200000
#error "atomic_wait_freebsd requires FreeBSD 12+"
#endif

#include "../../atomic_wait.h"
#include "atomic_wait_common.ipp.ipp"
#include <errno.h>
#include <sys/umtx.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(umtx_wait_uint)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, uint_least32_t expected,
  const struct timespec *abs_timeout, int32_t *val)
  {
    int save_errno = errno;
    struct _umtx_time umtx_time;
    if(abs_timeout)
    {
      umtx_time._timeout = *abs_timeout;
      umtx_time._flags = UMTX_ABSTIME;
      umtx_time._clockid = UMTX_CLOCK_MONOTONIC;
    }
    else
    {
      memset(&umtx_time, 0, sizeof(umtx_time));
      umtx_time._flags = 0;
    }
    int ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAIT_UINT,
                       expected, (long) &umtx_time);
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

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(umtx_wake_uint)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, unsigned count)
  {
    int save_errno = errno;
    long ret =
    _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAKE_UINT, count, 0);
    if(ret < 0)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return (int) ret;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(umtx_wait)(const volatile void *object,
                                      unsigned long expected,
                                      const struct timespec *abs_timeout)
  {
    int save_errno = errno;
    struct _umtx_time umtx_time;
    if(abs_timeout)
    {
      umtx_time._timeout = *abs_timeout;
      umtx_time._flags = UMTX_ABSTIME;
      umtx_time._clockid = UMTX_CLOCK_MONOTONIC;
    }
    else
    {
      memset(&umtx_time, 0, sizeof(umtx_time));
    }

    int ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAIT,
                       (long) &umtx_time, expected);
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

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(umtx_wake)(const volatile void *object,
                                      unsigned count)
  {
    int save_errno = errno;
    long ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAKE,
                        (long) count, 0);
    if(ret < 0)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return (int) ret;
  }

  // --- 4-byte: UMTX_OP_WAIT_UINT bypass ---

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
      umtx_wait_uint(object, expected, NULL, (int32_t *) (uintptr_t) object);
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
    umtx_wake_uint((const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, 1);
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
    umtx_wake_uint((const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, INT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
    (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object);
  }

  // --- Sub-native-width: hash table fallback ---

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

  // --- Native-width expected + notify (4 byte) ---

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
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if(now.tv_sec > end.tv_sec ||
           (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
        {
          *expected = current;
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
          return 0;
        }

        struct _umtx_time umtx_time;
        umtx_time._timeout = end;
        umtx_time._flags = UMTX_ABSTIME;
        umtx_time._clockid = UMTX_CLOCK_MONOTONIC;

        int ret = _umtx_op((volatile void *) (uintptr_t) object,
                           UMTX_OP_WAIT_UINT, *expected, (long) &umtx_time);

        uint_least32_t current2 =
        atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
        *expected = current2;
        if(ret < 0 && errno == ETIMEDOUT)
        {
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
          return 0;
        }
        if(current2 != *expected)
        {
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
          return 1;
        }
        // Spurious wake, retry
      }
      else
      {
        int ret =
        umtx_wait_uint(object, *expected, NULL, (int32_t *) (uintptr_t) object);
        uint_least32_t current2 =
        atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
        *expected = current2;
        if(ret < 0 && errno == ETIMEDOUT)
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
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *restrict object,
  uint_least32_t *restrict expected, uint_least32_t desired,
  unsigned max_threads_to_wake, memory_order success, memory_order failure)
  {
    uint_least32_t original = *expected;
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
      int n =
      umtx_wake_uint((const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, 1);
      if(n <= 0)
        break;
      woke += n;
    }
    return 1 + woke;
  }

#ifdef __cplusplus
}
#endif
