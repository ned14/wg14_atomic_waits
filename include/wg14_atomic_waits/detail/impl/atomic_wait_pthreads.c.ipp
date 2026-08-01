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

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE                           \
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX pthread_cond_t
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_INIT(x)                   \
  pthread_cond_init(&(x)->atomic, WG14_ATOMIC_WAITS_NULLPTR)
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_DESTROY(x)                \
  pthread_cond_destroy(&(x)->atomic)
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAIT(x, timeout)          \
  pthread_cond_wait(&(x)->atomic, WG14_ATOMIC_WAITS_PREFIX(pthreads_mutex)())
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAKE(x,                   \
                                                           max_threads_to_wake) \
  (                                                                            \
  ((max_threads_to_wake == 1)                                                  \
       ? pthread_cond_signal(&(x)->atomic)                                      \
       : pthread_cond_broadcast(&(x)->atomic)),                                 \
  1)

#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 0
#define WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64 0
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32 0
#define WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64 0

  // There must be exactly one of these in the whole process
  static WG14_ATOMIC_WAITS_INLINE pthread_mutex_t *
  WG14_ATOMIC_WAITS_PREFIX(pthreads_mutex)(void)
  {
    static WG14_ATOMIC_WAITS_THREAD_LOCAL pthread_mutex_t m;
    static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_flag initialized =
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX ATOMIC_FLAG_INIT;
    if(!atomic_flag_test_and_set_explicit(
       &initialized, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_acquire))
    {
      pthread_mutex_init(&m, WG14_ATOMIC_WAITS_NULLPTR);
    }
    return &m;
  }

#include "atomic_wait_common.ipp.ipp"
