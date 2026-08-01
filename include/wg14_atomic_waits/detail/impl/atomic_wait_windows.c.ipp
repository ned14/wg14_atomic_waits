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
      timeout_ms = (ms > 0xFFFFFFFFull) ? INFINITE : (DWORD) ms;
    }
    int save_errno = errno;
    if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 4, timeout_ms))
    {
      errno = save_errno;
      return 0;
    }
    DWORD lasterr = GetLastError();
    errno = save_errno;
    return (lasterr == ERROR_TIMEOUT) ? -ETIMEDOUT : -1;
  }

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
      timeout_ms = (ms > 0xFFFFFFFFull) ? INFINITE : (DWORD) ms;
    }
    int save_errno = errno;
    if(WaitOnAddress((PVOID) (uintptr_t) object, &expected, 8, timeout_ms))
    {
      errno = save_errno;
      return 0;
    }
    DWORD lasterr = GetLastError();
    errno = save_errno;
    return (lasterr == ERROR_TIMEOUT) ? -ETIMEDOUT : -1;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    if(WakeByAddressSingle((PVOID) (uintptr_t) object))
    {
      errno = save_errno;
      return (max_threads_to_wake == 1) ? 1 : 1;
    }
    errno = save_errno;
    return 0;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    if(WakeByAddressSingle((PVOID) (uintptr_t) object))
    {
      errno = save_errno;
      return (max_threads_to_wake == 1) ? 1 : 1;
    }
    errno = save_errno;
    return 0;
  }

#include "atomic_wait_common.ipp.ipp"
