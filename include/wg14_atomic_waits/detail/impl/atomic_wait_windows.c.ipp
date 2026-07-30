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

#if !defined(_WIN32) && !defined(_WIN64)
#error "atomic_wait_windows.c.ipp must only be included on Windows"
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0602)
#error                                                                         \
"atomic_wait_windows requires Windows 8 or later (_WIN32_WINNT >= 0x0602)"
#endif

#include "../../atomic_wait.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdint.h>
#include <synchapi.h>
#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

  static WG14_ATOMIC_WAITS_INLINE BOOL WG14_ATOMIC_WAITS_PREFIX(win32_wait)(
  PVOID object, SIZE_T size, const void *expected, DWORD timeout_ms)
  {
    return WaitOnAddress(object, (PVOID) expected, (ULONG) size, timeout_ms);
  }

  static WG14_ATOMIC_WAITS_INLINE BOOL
  WG14_ATOMIC_WAITS_PREFIX(win32_wake_single)(PVOID object)
  {
    return WakeByAddressSingle(object);
  }

  static WG14_ATOMIC_WAITS_INLINE BOOL
  WG14_ATOMIC_WAITS_PREFIX(win32_wake_all)(PVOID object)
  {
    return WakeByAddressAll(object);
  }

  // --- All sizes: Windows WaitOnAddress bypass (no hash table needed) ---
  // The volatile qualifier is intentionally stripped when passing the object
  // address to WaitOnAddress/WakeByAddressSingle/WakeByAddressAll. As with
  // the hash-table path, the pointer is not dereferenced in C; the kernel
  // primitive provides its own atomic memory-ordering guarantees for the
  // comparison, so discarding volatile is well-defined (see plan Step 3,
  // Step 7).

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(
  const volatile _Atomic(uint_least8_t) *object, uint_least8_t expected,
  memory_order order)
  {
    for(;;)
    {
      uint_least8_t current =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
      if(current != expected)
        return;
      if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 1, INFINITE))
        continue;  // woken, re-check
      return;
    }
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i1)(
  const volatile _Atomic(int_least8_t) *object, int_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(
    (const volatile _Atomic(uint_least8_t) *) object, (uint_least8_t) expected,
    order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(
  const volatile _Atomic(uint_least16_t) *object, uint_least16_t expected,
  memory_order order)
  {
    for(;;)
    {
      uint_least16_t current =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
      if(current != expected)
        return;
      if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 2, INFINITE))
        continue;
      return;
    }
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i2)(
  const volatile _Atomic(int_least16_t) *object, int_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(
    (const volatile _Atomic(uint_least16_t) *) object,
    (uint_least16_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
  const volatile _Atomic(uint_least32_t) *object, uint_least32_t expected,
  memory_order order)
  {
    for(;;)
    {
      uint_least32_t current =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
      if(current != expected)
        return;
      if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 4, INFINITE))
        continue;
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

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(
  const volatile _Atomic(uint_least64_t) *object, uint_least64_t expected,
  memory_order order)
  {
    for(;;)
    {
      uint_least64_t current =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
      if(current != expected)
        return;
      if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 8, INFINITE))
        continue;
      return;
    }
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i8)(
  const volatile _Atomic(int_least64_t) *object, int_least64_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(
    (const volatile _Atomic(uint_least64_t) *) object,
    (uint_least64_t) expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_1)(
  const volatile _Atomic(uint_least8_t) *object, uint_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(object, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i1)(
  const volatile _Atomic(int_least8_t) *object, int_least8_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i1)(object, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_2)(
  const volatile _Atomic(uint_least16_t) *object, uint_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(object, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i2)(
  const volatile _Atomic(int_least16_t) *object, int_least16_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i2)(object, expected, order);
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
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i4)(object, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_8)(
  const volatile _Atomic(uint_least64_t) *object, uint_least64_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(object, expected, order);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_i8)(
  const volatile _Atomic(int_least64_t) *object, int_least64_t expected,
  memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_wait_i8)(object, expected, order);
  }

  // --- Notify (all sizes, WaitOnAddress bypass) ---

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(
  volatile _Atomic(uint_least8_t) *object)
  {
    WakeByAddressSingle((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i1)(
  volatile _Atomic(int_least8_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(
    (volatile _Atomic(uint_least8_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(
  volatile _Atomic(uint_least16_t) *object)
  {
    WakeByAddressSingle((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i2)(
  volatile _Atomic(int_least16_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(
    (volatile _Atomic(uint_least16_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
  volatile _Atomic(uint_least32_t) *object)
  {
    WakeByAddressSingle((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i4)(
  volatile _Atomic(int_least32_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
    (volatile _Atomic(uint_least32_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(
  volatile _Atomic(uint_least64_t) *object)
  {
    WakeByAddressSingle((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_i8)(
  volatile _Atomic(int_least64_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(
    (volatile _Atomic(uint_least64_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(
  volatile _Atomic(uint_least8_t) *object)
  {
    WakeByAddressAll((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i1)(
  volatile _Atomic(int_least8_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(
    (volatile _Atomic(uint_least8_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(
  volatile _Atomic(uint_least16_t) *object)
  {
    WakeByAddressAll((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i2)(
  volatile _Atomic(int_least16_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(
    (volatile _Atomic(uint_least16_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
  volatile _Atomic(uint_least32_t) *object)
  {
    WakeByAddressAll((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i4)(
  volatile _Atomic(int_least32_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
    (volatile _Atomic(uint_least32_t) *) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(
  volatile _Atomic(uint_least64_t) *object)
  {
    WakeByAddressAll((PVOID) (uintptr_t) object);
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_i8)(
  volatile _Atomic(int_least64_t) *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(
    (volatile _Atomic(uint_least64_t) *) object);
  }

  // --- Native-width expected + notify (4 byte) ---

  int WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(
  const volatile _Atomic(uint_least32_t) *restrict object,
  uint_least32_t *restrict expected, const struct timespec *restrict duration,
  memory_order success, memory_order failure)
  {
    DWORD timeout_ms = INFINITE;
    if(duration)
    {
      ULONGLONG ms = (ULONGLONG) duration->tv_sec * 1000ull +
                     (duration->tv_nsec + 999999ull) / 1000000ull;
      timeout_ms = (ms > 0xFFFFFFFFull) ? INFINITE : (DWORD) ms;
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
      if(WaitOnAddress((PVOID) (uintptr_t) object, expected, 4, timeout_ms))
      {
        uint_least32_t current2 =
        atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
        if(current2 != *expected)
        {
          *expected = current2;
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
          return 1;
        }
        continue;
      }
      *expected =
      atomic_load_explicit(object, WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
      atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
      return 0;
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
    unsigned woke = 0;
    BOOL ok = TRUE;
    for(unsigned i = 0; i < max_threads_to_wake && ok; i++)
    {
      ok = WakeByAddressSingle((PVOID) (uintptr_t) object);
      if(ok)
        woke++;
    }
    return 1 + (int) woke;
  }

#ifdef __cplusplus
}
#endif
