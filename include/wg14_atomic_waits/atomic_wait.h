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

#ifndef WG14_ATOMIC_WAITS_ATOMIC_WAIT_H
#define WG14_ATOMIC_WAITS_ATOMIC_WAIT_H

#include "config.h"
#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  //! Smallest signed integer type with least overhead for atomic wait/notify on
  //! this platform. Always int_least32_t.
  typedef int_least32_t WG14_ATOMIC_WAITS_PREFIX(int_native_wait_notify_t);
  //! Smallest unsigned integer type with least overhead for atomic wait/notify
  //! on this platform. Always uint_least32_t.
  typedef uint_least32_t WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t);

#ifdef __cplusplus
#if __cplusplus >= 202002L
#include <atomic>
  //! `_Atomic`-qualified `int_native_wait_notify_t`. In C++20+ this is
  //! `std::atomic<int_least32_t>`.
  using WG14_ATOMIC_WAITS_PREFIX(atomic_int_native_wait_notify_t) =
  std::atomic<int_least32_t>;
  //! `_Atomic`-qualified `uint_native_wait_notify_t`. In C++20+ this is
  //! `std::atomic<uint_least32_t>`.
  using WG14_ATOMIC_WAITS_PREFIX(atomic_uint_native_wait_notify_t) =
  std::atomic<uint_least32_t>;
#else
  /* In pre-C++20 C++ mode, the _Atomic qualifier from C11 is not valid C++
     syntax. These types are unavailable as typenames. Use std::atomic<T>
     directly. */
#endif
#else
//! `_Atomic`-qualified `int_native_wait_notify_t`.
typedef _Atomic(int_least32_t)
WG14_ATOMIC_WAITS_PREFIX(atomic_int_native_wait_notify_t);
//! `_Atomic`-qualified `uint_native_wait_notify_t`.
typedef _Atomic(uint_least32_t)
WG14_ATOMIC_WAITS_PREFIX(atomic_uint_native_wait_notify_t);
#endif

// Width-specific C11 function declarations (used by C code and the C dispatch
// path in macros).
#ifndef __cplusplus
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(
  const volatile _Atomic(uint_least8_t) *object, uint_least8_t expected,
  memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(
  const volatile _Atomic(uint_least16_t) *object, uint_least16_t expected,
  memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
  const volatile _Atomic(uint_least32_t) *object, uint_least32_t expected,
  memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(
  const volatile _Atomic(uint_least64_t) *object, uint_least64_t expected,
  memory_order order);

  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(
  atomic_wait_explicit_1)(const volatile _Atomic(uint_least8_t) *object,
                          uint_least8_t expected, memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(
  atomic_wait_explicit_2)(const volatile _Atomic(uint_least16_t) *object,
                          uint_least16_t expected, memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(
  atomic_wait_explicit_4)(const volatile _Atomic(uint_least32_t) *object,
                          uint_least32_t expected, memory_order order);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(
  atomic_wait_explicit_8)(const volatile _Atomic(uint_least64_t) *object,
                          uint_least64_t expected, memory_order order);

  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(
  volatile _Atomic(uint_least8_t) *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(
  volatile _Atomic(uint_least16_t) *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
  volatile _Atomic(uint_least32_t) *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(
  volatile _Atomic(uint_least64_t) *object);

  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(
  volatile _Atomic(uint_least8_t) *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(
  volatile _Atomic(uint_least16_t) *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
  volatile _Atomic(uint_least32_t) *object);
  WG14_ATOMIC_WAITS_EXTERN void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(
  volatile _Atomic(uint_least64_t) *object);

  WG14_ATOMIC_WAITS_EXTERN int
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(
  const volatile _Atomic(uint_least32_t) *restrict object,
  uint_least32_t *restrict expected, const struct timespec *restrict duration,
  memory_order success, memory_order failure);

  WG14_ATOMIC_WAITS_EXTERN int WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(
  volatile _Atomic(uint_least32_t) *restrict object,
  uint_least32_t *restrict expected, uint_least32_t desired,
  unsigned max_threads_to_wake, memory_order success, memory_order failure);
#endif

#ifdef __cplusplus
}
#endif

// -- C++20 Template Wrappers --
#ifdef __cplusplus
#if __cplusplus >= 202002L
#include <chrono>
namespace wg14_atomic_waits
{

  template <typename T>
  inline void atomic_wait(const volatile std::atomic<T> *vobj, T expected,
                          std::memory_order order = std::memory_order_seq_cst)
  {
    const std::atomic<T> *obj = const_cast<const std::atomic<T> *>(vobj);
    obj->wait(expected, order);
  }

  template <typename T>
  inline void atomic_notify_one(volatile std::atomic<T> *vobj)
  {
    std::atomic<T> *obj = const_cast<std::atomic<T> *>(vobj);
    obj->notify_one();
  }

  template <typename T>
  inline void atomic_notify_all(volatile std::atomic<T> *vobj)
  {
    std::atomic<T> *obj = const_cast<std::atomic<T> *>(vobj);
    obj->notify_all();
  }

  template <typename T>
  inline int atomic_wait_expected(const volatile std::atomic<T> *vobj,
                                  T *expected, const struct timespec *duration,
                                  std::memory_order success,
                                  std::memory_order failure)
  {
    (void) success;
    (void) failure;
    const std::atomic<T> *obj = const_cast<const std::atomic<T> *>(vobj);
    T current = obj->load(std::memory_order_seq_cst);
    if(current != *expected)
    {
      *expected = current;
      return 0;
    }
    if(duration)
    {
      using namespace std::chrono;
      auto deadline = steady_clock::now() + seconds(duration->tv_sec) +
                      nanoseconds(duration->tv_nsec);
      for(;;)
      {
        auto now = steady_clock::now();
        if(now >= deadline)
        {
          *expected = obj->load(std::memory_order_seq_cst);
          return 0;
        }
        obj->wait(*expected);
        current = obj->load(std::memory_order_seq_cst);
        if(current != *expected)
        {
          *expected = current;
          return 1;
        }
      }
    }
    else
    {
      for(;;)
      {
        obj->wait(*expected);
        current = obj->load(std::memory_order_seq_cst);
        if(current != *expected)
        {
          *expected = current;
          return 1;
        }
      }
    }
  }

  template <typename T>
  inline int atomic_notify(volatile std::atomic<T> *vobj, T *expected,
                           T desired, unsigned max_threads_to_wake,
                           std::memory_order success, std::memory_order failure)
  {
    std::atomic<T> *obj = const_cast<std::atomic<T> *>(vobj);
    bool ok =
    obj->compare_exchange_strong(*expected, desired, success, failure);
    if(!ok)
      return 0;
    if(max_threads_to_wake == 0)
      return 1;
    for(unsigned i = 0; i < max_threads_to_wake; ++i)
    {
      obj->notify_one();
    }
    return 1 + static_cast<int>(max_threads_to_wake);
  }

}  // namespace wg14_atomic_waits
#endif  // C++20
#endif

// -- Public C11-compatible dispatch macros --

#ifdef __cplusplus
#if __cplusplus >= 202002L
#define _WG14_ATOMIC_WAITS_IMPL_atomic_wait(object, expected)                  \
  do                                                                           \
  {                                                                            \
    wg14_atomic_waits::atomic_wait((object), (expected));                      \
  } while(0)
#define _WG14_ATOMIC_WAITS_IMPL_atomic_wait_explicit(object, expected, order)  \
  do                                                                           \
  {                                                                            \
    wg14_atomic_waits::atomic_wait((object), (expected), (order));             \
  } while(0)
#define _WG14_ATOMIC_WAITS_IMPL_atomic_notify_one(object)                      \
  do                                                                           \
  {                                                                            \
    wg14_atomic_waits::atomic_notify_one((object));                            \
  } while(0)
#define _WG14_ATOMIC_WAITS_IMPL_atomic_notify_all(object)                      \
  do                                                                           \
  {                                                                            \
    wg14_atomic_waits::atomic_notify_all((object));                            \
  } while(0)
#define atomic_wait(object, expected)                                          \
  _WG14_ATOMIC_WAITS_IMPL_atomic_wait((object), (expected))
#define atomic_wait_explicit(object, expected, order)                          \
  _WG14_ATOMIC_WAITS_IMPL_atomic_wait_explicit((object), (expected), (order))
#define atomic_notify_one(object)                                              \
  _WG14_ATOMIC_WAITS_IMPL_atomic_notify_one((object))
#define atomic_notify_all(object)                                              \
  _WG14_ATOMIC_WAITS_IMPL_atomic_notify_all((object))
#define atomic_wait_expected(object, expected, duration, success, failure)     \
  (                                                                            \
  (void) (object), (void) (expected), (void) (duration), (void) (success),     \
  (void) (failure),                                                            \
  wg14_atomic_waits::atomic_wait_expected((object), (expected), (duration),    \
                                          (success), (failure)))
#define atomic_notify(object, expected, desired, max_threads_to_wake, success, \
                      failure)                                                 \
  (                                                                            \
  (void) (object), (void) (expected), (void) (desired),                        \
  (void) (max_threads_to_wake), (void) (success), (void) (failure),            \
  wg14_atomic_waits::atomic_notify((object), (expected), (desired),            \
                                   (max_threads_to_wake), (success),           \
                                   (failure)))
#else
/* std::atomic<T>::wait(), notify_one(), notify_all() require C++20.
   This translation unit was compiled without C++20 support. */
#define atomic_wait(object, expected)                                          \
  do                                                                           \
  {                                                                            \
    (void) (static_cast<const volatile void *>(object));                       \
    (void) (expected);                                                         \
  } while(0)
#define atomic_wait_explicit(object, expected, order)                          \
  do                                                                           \
  {                                                                            \
    (void) (static_cast<const volatile void *>(object));                       \
    (void) (expected);                                                         \
    (void) (order);                                                            \
  } while(0)
#define atomic_notify_one(object)                                              \
  do                                                                           \
  {                                                                            \
    (void) (static_cast<volatile void *>(object));                             \
  } while(0)
#define atomic_notify_all(object)                                              \
  do                                                                           \
  {                                                                            \
    (void) (static_cast<volatile void *>(object));                             \
  } while(0)
#define atomic_wait_expected(object, expected, duration, success, failure)     \
  (                                                                            \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(object),                                       \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(expected),                                     \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(duration),                                     \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(success),                                      \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(failure), -1)
#define atomic_notify(object, expected, desired, max_threads_to_wake, success, \
                      failure)                                                 \
  (                                                                            \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(object),                                       \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(expected),                                     \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(desired),                                      \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(max_threads_to_wake),                          \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(success),                                      \
  WG14_ATOMIC_WAITS_ATTR_UNUSED(failure), 0)
#endif
#else
// -- C11-compatible dispatch macros (C only) ---

#define _WG14_ATOMIC_WAITS_IMPL_atomic_wait(object, expected)                  \
  do                                                                           \
  {                                                                            \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(                                 \
      (const volatile _Atomic(uint_least8_t) *) (object),                      \
      (uint_least8_t) (expected), memory_order_seq_cst);                       \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(                                 \
      (const volatile _Atomic(uint_least16_t) *) (object),                     \
      (uint_least16_t) (expected), memory_order_seq_cst);                      \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(                                 \
      (const volatile _Atomic(uint_least32_t) *) (object),                     \
      (uint_least32_t) (expected), memory_order_seq_cst);                      \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(                                 \
      (const volatile _Atomic(uint_least64_t) *) (object),                     \
      (uint_least64_t) (expected), memory_order_seq_cst);                      \
  } while(0)

#define _WG14_ATOMIC_WAITS_IMPL_atomic_wait_explicit(object, expected, order)  \
  do                                                                           \
  {                                                                            \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_1)(                        \
      (const volatile _Atomic(uint_least8_t) *) (object),                      \
      (uint_least8_t) (expected), (order));                                    \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_2)(                        \
      (const volatile _Atomic(uint_least16_t) *) (object),                     \
      (uint_least16_t) (expected), (order));                                   \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_4)(                        \
      (const volatile _Atomic(uint_least32_t) *) (object),                     \
      (uint_least32_t) (expected), (order));                                   \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit_8)(                        \
      (const volatile _Atomic(uint_least64_t) *) (object),                     \
      (uint_least64_t) (expected), (order));                                   \
  } while(0)

#define _WG14_ATOMIC_WAITS_IMPL_atomic_notify_one(object)                      \
  do                                                                           \
  {                                                                            \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(                           \
      (volatile _Atomic(uint_least8_t) *) (object));                           \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(                           \
      (volatile _Atomic(uint_least16_t) *) (object));                          \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(                           \
      (volatile _Atomic(uint_least32_t) *) (object));                          \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(                           \
      (volatile _Atomic(uint_least64_t) *) (object));                          \
  } while(0)

#define _WG14_ATOMIC_WAITS_IMPL_atomic_notify_all(object)                      \
  do                                                                           \
  {                                                                            \
    if(sizeof(*(object)) == 1)                                                 \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(                           \
      (volatile _Atomic(uint_least8_t) *) (object));                           \
    else if(sizeof(*(object)) == 2)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(                           \
      (volatile _Atomic(uint_least16_t) *) (object));                          \
    else if(sizeof(*(object)) == 4)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(                           \
      (volatile _Atomic(uint_least32_t) *) (object));                          \
    else if(sizeof(*(object)) == 8)                                            \
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(                           \
      (volatile _Atomic(uint_least64_t) *) (object));                          \
  } while(0)

#define atomic_wait(object, expected)                                          \
  _WG14_ATOMIC_WAITS_IMPL_atomic_wait((object), (expected))
#define atomic_wait_explicit(object, expected, order)                          \
  _WG14_ATOMIC_WAITS_IMPL_atomic_wait_explicit((object), (expected), (order))
#define atomic_notify_one(object)                                              \
  _WG14_ATOMIC_WAITS_IMPL_atomic_notify_one((object))
#define atomic_notify_all(object)                                              \
  _WG14_ATOMIC_WAITS_IMPL_atomic_notify_all((object))

#define atomic_wait_expected(object, expected, duration, success, failure)     \
  (                                                                            \
  sizeof(                                                                      \
  char[1 - 2 * !!(sizeof(*(object)) != sizeof(WG14_ATOMIC_WAITS_PREFIX(        \
                                       uint_native_wait_notify_t)))]) == 1 ?   \
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(                           \
  (const volatile _Atomic(WG14_ATOMIC_WAITS_PREFIX(                            \
  uint_native_wait_notify_t)) *) (object),                                     \
  (WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) *) (expected),          \
  (duration), (success), (failure)) :                                          \
  (assert(0 && "atomic_wait_expected: unsupported type width"), -1))

#define atomic_notify(object, expected, desired, max_threads_to_wake, success, \
                      failure)                                                 \
  (                                                                            \
  sizeof(                                                                      \
  char[1 - 2 * !!(sizeof(*(object)) != sizeof(WG14_ATOMIC_WAITS_PREFIX(        \
                                       uint_native_wait_notify_t)))]) == 1 ?   \
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(                                  \
  (volatile _Atomic(WG14_ATOMIC_WAITS_PREFIX(                                  \
  uint_native_wait_notify_t)) *) (object),                                     \
  (WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) *) (expected),          \
  (desired), (max_threads_to_wake), (success), (failure)) :                    \
  (assert(0 && "atomic_notify: unsupported type width"), 0))
#endif

// -- Header-only platform selection --
#if WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY
/* The platform .ipp files contain C11 _Atomic(T) casts that are not valid C++
   syntax. They are included only in C mode. */
#ifndef __cplusplus
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

#endif
