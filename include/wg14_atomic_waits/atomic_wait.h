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

/** @file atomic_wait.h
 *  \brief Proposed WG14 atomic wait/notify support (C11 / C++11 and later).
 *  \details
 *
 *  Provides the proposed WG14 atomic wait/notify API: \c atomic_wait,
 *  \c atomic_wait_explicit, \c atomic_notify_one, \c atomic_notify_all,
 *  \c atomic_wait_expected and \c atomic_notify. They suspend and wake
 *  \e threads within a single address space.
 *
 *  \section scope_process Process scope
 *  Wait/notify synchronises threads of the calling process only; it is not
 *  guaranteed to synchronise across processes sharing memory
 *  (e.g. \c mmap(MAP_SHARED)):
 *  - On Linux the native 4-byte fast path parks the thread on the \e user's
 *    atomic object using the \e shared futex operations (\c FUTEX_WAIT_BITSET
 *    and \c FUTEX_WAKE), deliberately \e not \c FUTEX_PRIVATE. \c FUTEX_PRIVATE
 *    keys waits in the process's private per-\c mm hash table, so a different
 *    process waking the same shared-memory address would never find the waiter
 *    and the wake would be silently lost. A shared futex costs negligible
 *    overhead and keeps the fast path usable if the object is in shared memory.
 *  - The 1/2/8-byte widths (and the pthreads backend) park threads on a
 *    per-process hash-table proxy allocated by the library, so they are
 *    strictly intra-process regardless of the underlying primitive.
 *  - Of the native fast paths, only Windows \c WaitOnAddress/\c WakeByAddress
 *    is documented to wake waiters across processes; macOS \c __ulock_wait is
 *    per-process. Cross-process wait/notify is therefore \e untested and
 *    \e backend-dependent and must not be relied upon; use your own
 *    process-shared primitive if you need it.
 *
 *  \section timeout_semantics Timeout semantics
 *  The \c duration argument of \c atomic_wait_expected is a \e relative
 *  remaining interval measured against a monotonic clock, or \c NULL for an
 *  infinite wait. Backends whose kernel primitive requires an absolute
 *  deadline (Linux \c FUTEX_WAIT_BITSET, FreeBSD \c _umtx_op without
 *  \c UMTX_ABSTIME) convert the relative interval internally, so all backends
 *  agree on the relative contract.
 */

#ifndef WG14_ATOMIC_WAITS_ATOMIC_WAIT_H
#define WG14_ATOMIC_WAITS_ATOMIC_WAIT_H

#include "config.h"

#include <stdint.h>  // for uint_least32_t
#include <time.h>    // for struct timespec

#ifdef __cplusplus
#include <atomic>
#define WG14_ATOMIC_WAITS_ATOMIC_PREFIX std::
extern "C"
{
#else
#include <stdatomic.h>
#define WG14_ATOMIC_WAITS_ATOMIC_PREFIX
#endif

  //! Smallest unsigned integer type suitable for atomic wait/notify on this
  //! platform. Always `uint_least32_t`.
  /*! \details
      `sizeof(uint_native_wait_notify_t)` is guaranteed to be 4 bytes, the
      largest atomic width supported by all WG14-compliant platforms.
  */
  typedef uint_least32_t WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t);

  //! `_Atomic`-qualified `uint_native_wait_notify_t`.
  /*! \details
      The atomic type used for wait/notify on objects of type `uint_native_wait_notify_t`.
  */
  typedef WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t
  WG14_ATOMIC_WAITS_PREFIX(atomic_uint_native_wait_notify_t);

  // Width-specific C11 function declarations (used by C code and the C dispatch
  // path in macros).
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object,
  uint_least8_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object,
  uint_least16_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order);

  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object);

  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object);

  WG14_ATOMIC_WAITS_EXTERN int
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t
  *__restrict object,
  uint_least32_t *__restrict expected,
  const struct timespec *__restrict duration,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order success,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order failure);

  WG14_ATOMIC_WAITS_EXTERN int WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t
  *__restrict object,
  uint_least32_t *__restrict expected, uint_least32_t desired,
  unsigned max_threads_to_wake,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order success,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order failure);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define _WG14_ATOMIC_WAITS_STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#else
#define _WG14_ATOMIC_WAITS_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#endif

#define _WG14_ATOMIC_WAITS_IMPL_atomic_width_check(object)                       \
  _WG14_ATOMIC_WAITS_STATIC_ASSERT(                                             \
  ((sizeof(*(object)) == 1) || (sizeof(*(object)) == 2) ||                       \
   (sizeof(*(object)) == 4) || (sizeof(*(object)) == 8)),                        \
  "wg14_atomic_waits: sizeof(*object) must be 1, 2, 4, or 8 bytes")

#define _WG14_ATOMIC_WAITS_IMPL_atomic_wait(object, expected)                  \
  do                                                                           \
  {                                                                            \
    _WG14_ATOMIC_WAITS_IMPL_atomic_width_check(object);                        \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t     \
       *) (object),                                                            \
       (uint_least8_t) (expected),                                              \
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);                   \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t    \
       *) (object),                                                            \
       (uint_least16_t) (expected),                                             \
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);                   \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t    \
       *) (object),                                                            \
       (uint_least32_t) (expected),                                             \
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);                   \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t    \
       *) (object),                                                            \
       (uint_least64_t) (expected),                                             \
       WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);                   \
  } while(0)

#define _WG14_ATOMIC_WAITS_IMPL_atomic_wait_explicit(object, expected, order)  \
  do                                                                           \
  {                                                                            \
    _WG14_ATOMIC_WAITS_IMPL_atomic_width_check(object);                        \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t     \
       *) (object),                                                            \
        (uint_least8_t) (expected), (order));                                   \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t    \
       *) (object),                                                            \
        (uint_least16_t) (expected), (order));                                  \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t    \
       *) (object),                                                            \
        (uint_least32_t) (expected), (order));                                  \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(                                 \
      (const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t    \
       *) (object),                                                            \
        (uint_least64_t) (expected), (order));                                  \
  } while(0)

#define _WG14_ATOMIC_WAITS_IMPL_atomic_notify_one(object)                      \
  do                                                                           \
  {                                                                            \
    _WG14_ATOMIC_WAITS_IMPL_atomic_width_check(object);                        \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t           \
       *) (object));                                                           \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t          \
       *) (object));                                                           \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t          \
       *) (object));                                                           \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t          \
       *) (object));                                                           \
  } while(0)

#define _WG14_ATOMIC_WAITS_IMPL_atomic_notify_all(object)                      \
  do                                                                           \
  {                                                                            \
    _WG14_ATOMIC_WAITS_IMPL_atomic_width_check(object);                        \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t           \
       *) (object));                                                           \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t          \
       *) (object));                                                           \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t          \
       *) (object));                                                           \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(                           \
      (volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t          \
       *) (object));                                                           \
  } while(0)

  //! Block the calling thread until `*object` equals `expected`.
  /*! \details
      Dispatch macro. Width of `*object` must be 1, 2, 4, or 8 bytes.
      Uses `memory_order_seq_cst` for the load comparison.
      If woken spuriously (value still equals `expected`), parks again.
      \param object Pointer to a `volatile _Atomic` integer of width 1, 2, 4, or 8.
      \param expected Expected value to compare against.
  */
#define atomic_wait(object, expected)                                          \
  _WG14_ATOMIC_WAITS_IMPL_atomic_wait((object), (expected))

  //! Block until `*object` equals `expected`, using a caller-specified memory order.
  /*! \details
      Dispatch macro. Width of `*object` must be 1, 2, 4, or 8 bytes.
      If woken spuriously (value still equals `expected`), parks again.
      \param object Pointer to a `volatile _Atomic` integer of width 1, 2, 4, or 8.
      \param expected Expected value to compare against.
      \param order Memory order for the load comparison.
  */
#define atomic_wait_explicit(object, expected, order)                          \
  _WG14_ATOMIC_WAITS_IMPL_atomic_wait_explicit((object), (expected), (order))

  //! Wake at least one thread blocked on `object`.
  /*! \details
      Dispatch macro. Width of `*object` must be 1, 2, 4, or 8 bytes.
      If no thread is currently blocked on `object`, the call has no effect.
      \param object Pointer to a `volatile _Atomic` integer of width 1, 2, 4, or 8.
  */
#define atomic_notify_one(object)                                              \
  _WG14_ATOMIC_WAITS_IMPL_atomic_notify_one((object))

  //! Wake all threads blocked on `object`.
  /*! \details
      Dispatch macro. Width of `*object` must be 1, 2, 4, or 8 bytes.
      If no thread is currently blocked on `object`, the call has no effect.
      \param object Pointer to a `volatile _Atomic` integer of width 1, 2, 4, or 8.
  */
#define atomic_notify_all(object)                                              \
  _WG14_ATOMIC_WAITS_IMPL_atomic_notify_all((object))

#ifdef __cplusplus
namespace wg14_atomic_waits {
template <class T>
inline int atomic_native_check_helper(T const volatile *object) {
  static_assert(
    (sizeof(*object) == sizeof(::WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t))),
    "wg14_atomic_waits: sizeof(*object) must equal "
    "sizeof(uint_native_wait_notify_t) (4 bytes)");
  (void) object;
  return 0;
}
}  // namespace wg14_atomic_waits
#define _WG14_ATOMIC_WAITS_IMPL_atomic_native_check(object)                    \
  (::wg14_atomic_waits::atomic_native_check_helper((object)))
#else
#define _WG14_ATOMIC_WAITS_IMPL_atomic_native_check(object)                    \
  ((void) sizeof(struct {                                                      \
    _Static_assert((sizeof(*(object)) ==                                       \
                    sizeof(WG14_ATOMIC_WAITS_PREFIX(                           \
                    uint_native_wait_notify_t))),                              \
                   "wg14_atomic_waits: sizeof(*object) must equal "           \
                   "sizeof(uint_native_wait_notify_t) (4 bytes)");              \
    int _unused;                                                                \
  }))
#endif

  //! Wait up to `duration` for `*object` to no longer equal `*expected`.
  /*! \details
      `sizeof(*object)` must equal `sizeof(uint_native_wait_notify_t)`.
      If `*object` does not equal `*expected`, returns immediately. Otherwise
      suspends the thread until notified or until `duration` passes. On wake,
      re-compares; if still equal, parks again. Total accumulated wait is at
      least `*duration`. On return, `*expected` is updated to the most recently
      loaded value.
      `duration` is a relative remaining interval measured against a monotonic
      clock, or `NULL` for no timeout. Every backend implements it as a
      relative duration: macOS and Windows consume it directly, Linux converts
      it to an absolute CLOCK_MONOTONIC deadline for `FUTEX_WAIT_BITSET`, FreeBSD
      leaves the `UMTX_ABSTIME` flag clear (making the kernel treat it as
      relative), and the pthreads backend converts it to an absolute
      `pthread_cond_timedwait` deadline internally.
      \param object Pointer to a `volatile _Atomic uint_least32_t`.
      \param expected Pointer to value to compare against; updated on return.
      \param duration Maximum time to wait, or `NULL` for no timeout.
      \param success Memory order when the wait ends with value != `*expected`.
      \param failure Memory order on timeout or early mismatch.
      \return Zero if no suspension or timeout; positive if suspended at least once; negative on error.
      \retval 0 No suspension occurred, or duration timeout.
      \retval >0 Thread was suspended at least once.
      \retval <0 Error.
  */
#define atomic_wait_expected(object, expected, duration, success, failure)        \
  (                                                                               \
  _WG14_ATOMIC_WAITS_IMPL_atomic_native_check(object),                            \
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(                              \
  (                                                                               \
  const volatile WG14_ATOMIC_WAITS_PREFIX(atomic_uint_native_wait_notify_t)     \
  *) (object),                                                                    \
  (WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) *) (expected),             \
  (duration), (success), (failure)))

  //! Atomically compare-exchange and notify up to `max_threads_to_wake` waiters.
  /*! \details
      `sizeof(*object)` must equal `sizeof(uint_native_wait_notify_t)`.
      Performs `atomic_compare_exchange_strong_explicit`. If `*object` still
      equals `*expected`, replaces it with `desired` and wakes up to
      `max_threads_to_wake` waiting threads. On some platforms the return
      value may be 1 plus the number of threads woken.
      \param object Pointer to a `volatile _Atomic uint_least32_t`.
      \param expected Pointer to value compared against; updated on failure.
      \param desired Value to store on successful exchange.
      \param max_threads_to_wake Maximum number of waiters to wake.
      \param success Memory order on success.
      \param failure Memory order on failure.
      \return Positive (1 + woken count) on successful exchange; zero on failure; negative on error.
      \retval >0 Successful exchange and notify.
      \retval 0 Compare-exchange failed.
      \retval <0 Error.
  */
#define atomic_notify(object, expected, desired, max_threads_to_wake, success, \
                      failure)                                                 \
  (                                                                            \
  _WG14_ATOMIC_WAITS_IMPL_atomic_native_check(object),                            \
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(                                  \
  (volatile WG14_ATOMIC_WAITS_PREFIX(atomic_uint_native_wait_notify_t) *)        \
  (object),                                                                       \
  (WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) *) (expected),          \
  (desired), (max_threads_to_wake), (success), (failure)))


// -- Header-only platform selection --
#if WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY
#if defined(_WIN32) || defined(_WIN64)
#include "detail/impl/atomic_wait_windows.c.ipp"
#elif defined(__linux__)
#include "detail/impl/atomic_wait_linux.c.ipp"
#elif defined(__FreeBSD__)
#include "detail/impl/atomic_wait_freebsd.c.ipp"
#elif defined(__APPLE__)
#include "detail/impl/atomic_wait_macos.c.ipp"
#else
#include "detail/impl/atomic_wait_pthreads.c.ipp"
#endif
#endif

#endif
