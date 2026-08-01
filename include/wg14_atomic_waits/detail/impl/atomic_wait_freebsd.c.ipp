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

#include <errno.h>
#include <limits.h>
#include <sys/umtx.h>
#include <time.h>

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
    struct _umtx_time umtx_time;
    if(abs_timeout != WG14_ATOMIC_WAITS_NULLPTR)
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
    if(ret == 0 || errno == EAGAIN || errno == EINTR)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return -1;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 1
  // Returns -errno if failed, 0 if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address64)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, const struct timespec *abs_timeout)
  {
    int save_errno = errno;
    struct _umtx_time umtx_time;
    if(abs_timeout != WG14_ATOMIC_WAITS_NULLPTR)
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
    int ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAIT,
                       (long) &umtx_time, (long) expected);
    if(ret == 0 || errno == EAGAIN || errno == EINTR)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return -1;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    long ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAKE_UINT,
                        (long) max_threads_to_wake, 0);
    if(ret < 0)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return (int) ret;
  }

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 1
  // Returns -errno if failed, number of threads woken if success
  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  unsigned max_threads_to_wake)
  {
    int save_errno = errno;
    long ret = _umtx_op((volatile void *) (uintptr_t) object, UMTX_OP_WAKE,
                        (long) max_threads_to_wake, 0);
    if(ret < 0)
    {
      errno = save_errno;
      return 0;
    }
    errno = save_errno;
    return (int) ret;
  }

#include "atomic_wait_common.ipp.ipp"
