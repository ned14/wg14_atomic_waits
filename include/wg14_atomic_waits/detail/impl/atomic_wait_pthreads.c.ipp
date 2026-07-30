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

#ifndef __linux__
#ifndef __FreeBSD__
#ifndef __APPLE__
#error                                                                         \
"atomic_wait_pthreads.c.ipp must only be included on non-Linux non-FreeBSD non-Apple POSIX"
#endif
#endif
#endif

#include "../../atomic_wait.h"
#include "atomic_wait_common.ipp.ipp"
#include <limits.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // --- All sizes: hash table fallback using pthread_cond_t ---

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

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, uint_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 4, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object, int_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 4, (uint64_t) (uint_least32_t) expected,
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

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object, uint_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 4, (uint64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object, int_least32_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
    (const volatile void *) object, 4, (uint64_t) (uint_least32_t) expected,
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

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 4,
                                                    1);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 4,
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

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 4,
                                                    UINT_MAX);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int_least32_t *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 4,
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

  // --- Native-width expected + notify via hash table ---

  int WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *restrict object,
  uint_least32_t *restrict expected, const struct timespec *restrict duration,
  memory_order success, memory_order failure)
  {
    return WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_generic)(
    (const volatile void *) object, 4, expected, duration, success, failure);
  }

  int WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *restrict object,
  uint_least32_t *restrict expected, uint_least32_t desired,
  unsigned max_threads_to_wake, memory_order success, memory_order failure)
  {
    uint_least32_t exp = *expected;
    if(!atomic_compare_exchange_strong_explicit(
       (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object, &exp, desired,
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX success,
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure))
    {
      *expected = exp;
      return 0;
    }
    *expected = desired;
    if(max_threads_to_wake == 0)
      return 1;
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)((volatile void *) object, 4,
                                                    max_threads_to_wake);
    return 1;
  }

#ifdef __cplusplus
}
#endif
