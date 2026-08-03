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

#ifndef WG14_ATOMIC_WAITS_LINUX_IMPL_GUARD
#define WG14_ATOMIC_WAITS_LINUX_IMPL_GUARD

#if !defined(__linux__)
#error "atomic_wait_linux.c.ipp must only be included on Linux"
#endif

#include "../../atomic_wait.h"

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

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, const struct timespec *abs_timeout)
  {
    int save_errno = errno;
    int ret = (int) syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAIT,
                            (int) expected, abs_timeout, NULL, 0);
    if(ret == 0)
    {
      errno = save_errno;
      return 0;
    }
    int e = errno;
    errno = save_errno;
    return -e;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 0

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    // FUTEX_WAKE takes a signed int count. Callers pass (unsigned)-1 to mean
    // "wake all"; passed through unclamped that becomes -1, and the kernel's
    // futex_wake() wake loop breaks after waking just one waiter
    // (if (++ret >= nr_wake) break), so atomic_notify_all would leave every
    // other parked waiter asleep forever. Clamp to INT_MAX so that wake-all
    // really wakes every waiter parked on the futex.
    const int nr_wake = (max_threads_to_wake > (unsigned) INT_MAX) ?
                        INT_MAX :
                        (int) max_threads_to_wake;
    long ret = syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAKE,
                       nr_wake, NULL, NULL, 0);
    if(ret < 0)
    {
      int e = errno;
      errno = save_errno;
      return -e;
    }
    errno = save_errno;
    return (int) ret;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 0

#include "atomic_wait_common.ipp.ipp"

#ifdef __cplusplus
}
#endif

#endif /* WG14_ATOMIC_WAITS_LINUX_IMPL_GUARD */
