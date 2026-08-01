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
static WG14_ATOMIC_WAITS_INLINE int
WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
uint_least32_t expected, const struct timespec *abs_timeout)
{
    int save_errno = errno;
    int ret = (int) syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAIT,
                            (int) expected, abs_timeout, NULL, 0);
    if(ret == 0 || errno == EAGAIN || errno == EINTR)
    {
        errno = save_errno;
        return 0;
    }
    errno = save_errno;
    return -1;
}

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 0

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 1
static WG14_ATOMIC_WAITS_INLINE int
WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(
volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
unsigned max_threads_to_wake)
{
    int save_errno = errno;
    long ret = syscall(SYS_futex, (int *) (uintptr_t) object, FUTEX_WAKE,
                       (int) max_threads_to_wake, NULL, NULL, 0);
    if(ret < 0)
    {
        errno = save_errno;
        return 0;
    }
    errno = save_errno;
    return (int) ret;
}

#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 0

#include "atomic_wait_common.ipp.ipp"
