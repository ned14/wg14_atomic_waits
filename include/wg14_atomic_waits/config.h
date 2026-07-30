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

#ifndef WG14_ATOMIC_WAITS_CONFIG_H
#define WG14_ATOMIC_WAITS_CONFIG_H

#include <stddef.h>  // for NULL

#ifndef WG14_ATOMIC_WAITS_PREFIX
#define WG14_ATOMIC_WAITS_PREFIX(x) x
#endif

#ifndef WG14_ATOMIC_WAITS_INLINE
#define WG14_ATOMIC_WAITS_INLINE inline
#endif

#ifndef WG14_ATOMIC_WAITS_THREAD_LOCAL
#ifdef __cplusplus
#define WG14_ATOMIC_WAITS_THREAD_LOCAL thread_local
#else
#define WG14_ATOMIC_WAITS_THREAD_LOCAL _Thread_local
#endif
#endif

#ifndef WG14_ATOMIC_WAITS_NULLPTR
#if __STDC_VERSION__ >= 202300L || __cplusplus
#define WG14_ATOMIC_WAITS_NULLPTR nullptr
#else
#define WG14_ATOMIC_WAITS_NULLPTR NULL
#endif
#endif

#ifndef WG14_ATOMIC_WAITS_IGNORE_MULTIPLE_DEFINITIONS
#ifdef _MSC_VER
#define WG14_ATOMIC_WAITS_IGNORE_MULTIPLE_DEFINITIONS __declspec(selectany)
#else
#define WG14_ATOMIC_WAITS_IGNORE_MULTIPLE_DEFINITIONS __attribute__((weak))
#endif
#endif

#ifndef WG14_ATOMIC_WAITS_DEFAULT_VISIBILITY
#ifdef _WIN32
#define WG14_ATOMIC_WAITS_DEFAULT_VISIBILITY
#else
#define WG14_ATOMIC_WAITS_DEFAULT_VISIBILITY                                   \
  __attribute__((visibility("default")))
#endif
#endif

#ifndef WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY
#define WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 0
#endif

#ifndef WG14_ATOMIC_WAITS_EXTERN_IMPL
#if defined(WG14_ATOMIC_WAITS_SOURCE) && WG14_ATOMIC_WAITS_SOURCE
#ifdef _WIN32
#define WG14_ATOMIC_WAITS_EXTERN_IMPL extern __declspec(dllexport)
#else
#define WG14_ATOMIC_WAITS_EXTERN_IMPL                                          \
  extern __attribute__((visibility("default")))
#endif
#else
#define WG14_ATOMIC_WAITS_EXTERN_IMPL extern
#endif
#endif

#ifndef WG14_ATOMIC_WAITS_EXTERN
#if WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY
#define WG14_ATOMIC_WAITS_EXTERN WG14_ATOMIC_WAITS_INLINE
#else
#define WG14_ATOMIC_WAITS_EXTERN WG14_ATOMIC_WAITS_EXTERN_IMPL
#endif
#endif

#ifndef WG14_ATOMIC_WAITS_STDERR_PRINTF
#define WG14_ATOMIC_WAITS_STDERR_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef WG14_ATOMIC_WAITS_ATTR_UNUSED
#if defined(__GNUC__) || defined(__clang__)
#define WG14_ATOMIC_WAITS_ATTR_UNUSED __attribute__((unused))
#else
#define WG14_ATOMIC_WAITS_ATTR_UNUSED
#endif
#endif

#endif
