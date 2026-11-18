/* Proposed WG14 atomic wait/notify support
(C) 2026 Niall Douglas <http://www.nedproductions.biz/>
File Created: Nov 2026


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

#ifndef WG14_ATOMIC_WAITS_SPIN_IMPL_GUARD
#define WG14_ATOMIC_WAITS_SPIN_IMPL_GUARD

#include "../../atomic_wait.h"

#if !WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#include <errno.h>
#endif

#if WG14_ATOMIC_WAITS_SPIN_USE_YIELD
#ifdef __cplusplus
#include <thread>
#else
#if defined(__STDC_NO_THREADS__)
#error "WG14_ATOMIC_WAITS_SPIN_USE_YIELD requires C11 threads support"
#endif
#include <threads.h>
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if !WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT
  static WG14_ATOMIC_WAITS_INLINE
  uint_least64_t WG14_ATOMIC_WAITS_PREFIX(monotonic_now_ns)(void);
#endif

  // This backend has no kernel primitive: waiting is a spin on the atomic
  // object using only <stdatomic.h>. A notify is a no-op because a spinning
  // waiter already observes value changes directly. If finite timeouts are
  // enabled, the spin loop uses the shared monotonic clock to honor them;
  // otherwise it never reads a clock. Setting WG14_ATOMIC_WAITS_SPIN_USE_YIELD
  // additionally yields to other threads between polls when <threads.h> (C) or
  // <thread> (C++) is available.

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_8 1
#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_16 1
#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 1
#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 1
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_8 1
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_16 1
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 1

  static WG14_ATOMIC_WAITS_INLINE void
  WG14_ATOMIC_WAITS_PREFIX(spin_pause)(void)
  {
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_signal_fence(
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);
#if WG14_ATOMIC_WAITS_SPIN_USE_YIELD
#ifdef __cplusplus
    std::this_thread::yield();
#else
    (void) thrd_yield();
#endif
#endif
  }

  static WG14_ATOMIC_WAITS_INLINE uint_least64_t
  WG14_ATOMIC_WAITS_PREFIX(spin_wait_start)(const uint_least64_t *timeout_ns)
  {
#if WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT
    (void) timeout_ns;
    return 0;
#else
  return (timeout_ns == WG14_ATOMIC_WAITS_NULLPTR) ?
         0 :
         WG14_ATOMIC_WAITS_PREFIX(monotonic_now_ns)();
#endif
  }

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(
  spin_wait_timed_out)(const uint_least64_t *timeout_ns, uint_least64_t start)
  {
#if WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT
    (void) timeout_ns;
    (void) start;
    return 0;
#else
  if(timeout_ns == WG14_ATOMIC_WAITS_NULLPTR)
  {
    return 0;
  }
  return (uint_least64_t) (WG14_ATOMIC_WAITS_PREFIX(monotonic_now_ns)() -
                           start) >= *timeout_ns;
#endif
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out_result)(void)
  {
#if WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT
    return 0;
#else
  return -ETIMEDOUT;
#endif
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object,
  uint_least8_t expected, const uint_least64_t *timeout_ns)
  {
    const uint_least64_t start =
    WG14_ATOMIC_WAITS_PREFIX(spin_wait_start)(timeout_ns);
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
          object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_relaxed) ==
          expected)
    {
      if(WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out)(timeout_ns, start))
      {
        return WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out_result)();
      }
      WG14_ATOMIC_WAITS_PREFIX(spin_pause)();
    }
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address16)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object,
  uint_least16_t expected, const uint_least64_t *timeout_ns)
  {
    const uint_least64_t start =
    WG14_ATOMIC_WAITS_PREFIX(spin_wait_start)(timeout_ns);
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
          object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_relaxed) ==
          expected)
    {
      if(WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out)(timeout_ns, start))
      {
        return WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out_result)();
      }
      WG14_ATOMIC_WAITS_PREFIX(spin_pause)();
    }
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, const uint_least64_t *timeout_ns)
  {
    const uint_least64_t start =
    WG14_ATOMIC_WAITS_PREFIX(spin_wait_start)(timeout_ns);
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
          object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_relaxed) ==
          expected)
    {
      if(WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out)(timeout_ns, start))
      {
        return WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out_result)();
      }
      WG14_ATOMIC_WAITS_PREFIX(spin_pause)();
    }
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address64)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, const uint_least64_t *timeout_ns)
  {
    const uint_least64_t start =
    WG14_ATOMIC_WAITS_PREFIX(spin_wait_start)(timeout_ns);
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
          object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_relaxed) ==
          expected)
    {
      if(WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out)(timeout_ns, start))
      {
        return WG14_ATOMIC_WAITS_PREFIX(spin_wait_timed_out_result)();
      }
      WG14_ATOMIC_WAITS_PREFIX(spin_pause)();
    }
    return 0;
  }

  // A spinning waiter does not need a notification to observe a value change.
  // The generic notify paths still require a wake helper, so provide a no-op
  // which reports that no parked waiters were counted.
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object,
  unsigned max_threads_to_wake)
  {
    (void) object;
    (void) max_threads_to_wake;
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address16)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object,
  unsigned max_threads_to_wake)
  {
    (void) object;
    (void) max_threads_to_wake;
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    (void) object;
    (void) max_threads_to_wake;
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  unsigned max_threads_to_wake)
  {
    (void) object;
    (void) max_threads_to_wake;
    return 0;
  }

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_SPIN_IMPL_GUARD */
