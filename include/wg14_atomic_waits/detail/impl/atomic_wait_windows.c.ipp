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

#ifndef WG14_ATOMIC_WAITS_WINDOWS_IMPL_GUARD
#define WG14_ATOMIC_WAITS_WINDOWS_IMPL_GUARD

#if !defined(_WIN32) && !defined(_WIN64)
#error "atomic_wait_windows.c.ipp must only be included on Windows"
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0602)
#error                                                                         \
"atomic_wait_windows requires Windows 8 or later (_WIN32_WINNT >= 0x0602)"
#endif

#include "../../atomic_wait.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// MUST come before other Windows only includes
#include <windows.h>

#include <synchapi.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // Converts a Win32 GetLastError() code into a POSIX errno value so wait
  // failures are reported in the same -errno convention as the other
  // backends. Unrecognised codes fall back to EIO.
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(windows_error_to_errno)(DWORD lasterr)
  {
    switch(lasterr)
    {
    case ERROR_SUCCESS: return 0;
    case ERROR_TIMEOUT: return ETIMEDOUT;
    case ERROR_INVALID_PARAMETER: return EINVAL;
    case ERROR_NOT_ENOUGH_MEMORY: return ENOMEM;
    case ERROR_OUTOFMEMORY: return ENOMEM;
    case ERROR_ACCESS_DENIED: return EACCES;
    case ERROR_INVALID_HANDLE: return EBADF;
    case ERROR_NO_SYSTEM_RESOURCES: return EAGAIN;
    default: return EIO;
    }
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_8 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object,
  uint_least8_t expected, const struct timespec *duration)
  {
    DWORD timeout_ms = INFINITE;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      ULONGLONG ms = (ULONGLONG) duration->tv_sec * 1000ull +
                     (duration->tv_nsec + 999999ull) / 1000000ull;
      timeout_ms = (ms >= 0xFFFFFFFEull) ? (DWORD) 0xFFFFFFFE : (DWORD) ms;
    }
    if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 1, timeout_ms))
    {
      return 0;
    }
    // WaitOnAddress returns FALSE on timeout or other failure; map the Win32
    // error code to -errno so callers can distinguish retryable conditions.
    return -WG14_ATOMIC_WAITS_PREFIX(windows_error_to_errno)(GetLastError());
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_16 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address16)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object,
  uint_least16_t expected, const struct timespec *duration)
  {
    DWORD timeout_ms = INFINITE;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      ULONGLONG ms = (ULONGLONG) duration->tv_sec * 1000ull +
                     (duration->tv_nsec + 999999ull) / 1000000ull;
      timeout_ms = (ms >= 0xFFFFFFFEull) ? (DWORD) 0xFFFFFFFE : (DWORD) ms;
    }
    if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 2, timeout_ms))
    {
      return 0;
    }
    // WaitOnAddress returns FALSE on timeout or other failure; map the Win32
    // error code to -errno so callers can distinguish retryable conditions.
    return -WG14_ATOMIC_WAITS_PREFIX(windows_error_to_errno)(GetLastError());
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, const struct timespec *duration)
  {
    DWORD timeout_ms = INFINITE;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      ULONGLONG ms = (ULONGLONG) duration->tv_sec * 1000ull +
                     (duration->tv_nsec + 999999ull) / 1000000ull;
      timeout_ms = (ms >= 0xFFFFFFFEull) ? (DWORD) 0xFFFFFFFE : (DWORD) ms;
    }
    if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 4, timeout_ms))
    {
      return 0;
    }
    // WaitOnAddress returns FALSE on timeout or other failure; map the Win32
    // error code to -errno so callers can distinguish retryable conditions.
    return -WG14_ATOMIC_WAITS_PREFIX(windows_error_to_errno)(GetLastError());
  }

#if defined(_WIN64)
#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address64)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, const struct timespec *duration)
  {
    DWORD timeout_ms = INFINITE;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      ULONGLONG ms = (ULONGLONG) duration->tv_sec * 1000ull +
                     (duration->tv_nsec + 999999ull) / 1000000ull;
      timeout_ms = (ms >= 0xFFFFFFFEull) ? (DWORD) 0xFFFFFFFE : (DWORD) ms;
    }
    if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 8, timeout_ms))
    {
      return 0;
    }
    // WaitOnAddress returns FALSE on timeout or other failure; map the Win32
    // error code to -errno so callers can distinguish retryable conditions.
    return -WG14_ATOMIC_WAITS_PREFIX(windows_error_to_errno)(GetLastError());
  }
#endif

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_8 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object,
  unsigned max_threads_to_wake)
  {
    if(max_threads_to_wake == 1)
    {
      WakeByAddressSingle((PVOID) (uintptr_t) object);
    }
    else
    {
      WakeByAddressAll((PVOID) (uintptr_t) object);
    }
    // WakeByAddress* reports no woken count and never fails (returns VOID), so
    // mirror the macOS backend's success convention: 1 for a single wake, a
    // large count for a wake-all.
    return (max_threads_to_wake == 1) ? 1 : (INT_MAX - 1);
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_16 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address16)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object,
  unsigned max_threads_to_wake)
  {
    if(max_threads_to_wake == 1)
    {
      WakeByAddressSingle((PVOID) (uintptr_t) object);
    }
    else
    {
      WakeByAddressAll((PVOID) (uintptr_t) object);
    }
    // WakeByAddress* reports no woken count and never fails (returns VOID), so
    // mirror the macOS backend's success convention: 1 for a single wake, a
    // large count for a wake-all.
    return (max_threads_to_wake == 1) ? 1 : (INT_MAX - 1);
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    if(max_threads_to_wake == 1)
    {
      WakeByAddressSingle((PVOID) (uintptr_t) object);
    }
    else
    {
      WakeByAddressAll((PVOID) (uintptr_t) object);
    }
    // WakeByAddress* reports no woken count and never fails (returns VOID), so
    // mirror the macOS backend's success convention: 1 for a single wake, a
    // large count for a wake-all.
    return (max_threads_to_wake == 1) ? 1 : (INT_MAX - 1);
  }

#if defined(_WIN64)
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  unsigned max_threads_to_wake)
  {
    if(max_threads_to_wake == 1)
    {
      WakeByAddressSingle((PVOID) (uintptr_t) object);
    }
    else
    {
      WakeByAddressAll((PVOID) (uintptr_t) object);
    }
    // WakeByAddress* reports no woken count and never fails (returns VOID), so
    // mirror the macOS backend's success convention: 1 for a single wake, a
    // large count for a wake-all.
    return (max_threads_to_wake == 1) ? 1 : (INT_MAX - 1);
  }
#endif

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_WINDOWS_IMPL_GUARD */
