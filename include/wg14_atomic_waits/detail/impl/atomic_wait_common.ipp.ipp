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

#include "../../atomic_wait.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE
#if !WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32
#error                                                                         \
"If WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE is not defined, then WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32 must be."
#endif
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE                           \
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_INIT(x)                   \
  (                                                                            \
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_store_explicit(                       \
  &(x)->atomic, 0, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_release),      \
  0)
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_DESTROY(x) (0)
// The proxy atomic is a monotonically increasing generation counter. A waiter
// parks on the value it reads at park time, so a wake that preceded the park
// (or a spurious wake) makes the re-park block on the current generation
// instead of busy-spinning on a stale flag.
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAIT(x, timeout)          \
  WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(                                 \
  &(x)->atomic,                                                                \
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(                        \
  &(x)->atomic, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_acquire),         \
  (timeout))
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAKE(x,                   \
                                                          max_threads_to_wake) \
  (                                                                            \
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_fetch_add_explicit(                   \
  &(x)->atomic, 1, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_acq_rel),      \
  WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(&(x)->atomic,                    \
                                              (max_threads_to_wake)))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    int use_count;
    WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE atomic;
  } WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t);

#define WG14_ATOMIC_WAITS_HASH_BUCKETS_INITIAL 1024
#define WG14_ATOMIC_WAITS_HASH_BUCKETS_MAX (1024 * 1024)


  typedef struct
  {
    // proxy must not move in memory after creation
    WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t) * proxy;
    void *key;
  } WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t);

  typedef struct
  {
    WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) * buckets;
    unsigned bucket_count;
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_flag lock;
  } WG14_ATOMIC_WAITS_PREFIX(hash_table_t);

  // The single process-wide proxy table, declared extern here and defined
  // below. In the compiled-library build the definition lives only in the
  // library TU; in the header-only build every TU that includes the header
  // emits its own identical definition, so the definition is marked
  // WG14_ATOMIC_WAITS_IGNORE_MULTIPLE_DEFINITIONS and the linker coalesces the
  // copies into one shared instance (a wait in one TU must find a notify's
  // proxy in another). The attribute is applied to a data item, not a function,
  // because MSVC's __declspec(selectany) only accepts data items with external
  // linkage. The table is zero-initialised, which is all it needs: the
  // atomic_flag lock starts clear and the buckets are allocated lazily on first
  // use.
  extern WG14_ATOMIC_WAITS_PREFIX(hash_table_t)
  WG14_ATOMIC_WAITS_PREFIX(shared_global_hash_table);
#ifdef __cplusplus
#define WG14_ATOMIC_WAITS_HASH_TABLE_ZERO_INIT                                 \
  {                                                                            \
  }
#else
#define WG14_ATOMIC_WAITS_HASH_TABLE_ZERO_INIT {0}
#endif
  WG14_ATOMIC_WAITS_IGNORE_MULTIPLE_DEFINITIONS
  WG14_ATOMIC_WAITS_PREFIX(hash_table_t)
  WG14_ATOMIC_WAITS_PREFIX(shared_global_hash_table) =
  WG14_ATOMIC_WAITS_HASH_TABLE_ZERO_INIT;
#undef WG14_ATOMIC_WAITS_HASH_TABLE_ZERO_INIT

  static WG14_ATOMIC_WAITS_INLINE void
  WG14_ATOMIC_WAITS_PREFIX(hash_table_lock)(hash_table_t *table)
  {
    for(;;)
    {
      if(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_flag_test_and_set_explicit(
         &table->lock, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_acquire) ==
         false)
      {
        return;
      }
    }
  }

  static WG14_ATOMIC_WAITS_INLINE void
  WG14_ATOMIC_WAITS_PREFIX(hash_table_unlock)(hash_table_t *table)
  {
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_flag_clear_explicit(
    &table->lock, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_release);
  }

  static WG14_ATOMIC_WAITS_INLINE unsigned
  WG14_ATOMIC_WAITS_PREFIX(hash_func)(const void *key)
  {
    const unsigned h = (unsigned) (uintptr_t) key;
    return (h ^ 2166136261U) * 16777619U;
  }

  // MUST HOLD THE LOCK ON ENTRY
  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(hash_table_grow)(
  WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *const table)
  {
    const unsigned old_count = table->bucket_count;
    unsigned new_count = old_count * 2;
    if(new_count > WG14_ATOMIC_WAITS_HASH_BUCKETS_MAX)
    {
      new_count = WG14_ATOMIC_WAITS_HASH_BUCKETS_MAX;
    }
    if(new_count <= old_count)
    {
      errno = ENOMEM;
      return -1;
    }
    WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *const new_buckets =
    (WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *) calloc(
    new_count, sizeof(WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t)));
    if(new_buckets == WG14_ATOMIC_WAITS_NULLPTR)
    {
      // errno already set
      return -1;
    }
    const unsigned mask = new_count - 1;
    for(unsigned i = 0; i < old_count; i++)
    {
      WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *const old_bucket =
      &table->buckets[i];
      if(old_bucket->key == WG14_ATOMIC_WAITS_NULLPTR)
      {
        continue;
      }
      const unsigned h =
      WG14_ATOMIC_WAITS_PREFIX(hash_func)(old_bucket->key) & mask;
      for(unsigned step = 0; step < new_count; step++)
      {
        const unsigned idx = (h + step * step) & mask;
        WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *const new_bucket =
        &new_buckets[idx];
        if(new_bucket->key == WG14_ATOMIC_WAITS_NULLPTR)
        {
          *new_bucket = *old_bucket;
          break;
        }
      }
    }
    free(table->buckets);
    table->buckets = new_buckets;
    table->bucket_count = new_count;
    return 0;
  }

  // MUST HOLD THE LOCK ON ENTRY
  static WG14_ATOMIC_WAITS_INLINE WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t) *
  WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(
  WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *const table,
  const volatile void *object, bool increment_use_count)
  {
    void *const key = (void *) object;

    if(table->buckets == WG14_ATOMIC_WAITS_NULLPTR)
    {
      table->buckets = (WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *) calloc(
      WG14_ATOMIC_WAITS_HASH_BUCKETS_INITIAL,
      sizeof(WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t)));
      table->bucket_count = WG14_ATOMIC_WAITS_HASH_BUCKETS_INITIAL;
      if(table->buckets == WG14_ATOMIC_WAITS_NULLPTR)
      {
        // out of memory
        return WG14_ATOMIC_WAITS_NULLPTR;
      }
    }
    for(;;)
    {
      const unsigned h =
      WG14_ATOMIC_WAITS_PREFIX(hash_func)(key) & (table->bucket_count - 1);
      for(unsigned step = 0; step < table->bucket_count; step++)
      {
        const unsigned idx = (h + step * step) & (table->bucket_count - 1);
        WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *bucket = &table->buckets[idx];
        if(bucket->key == WG14_ATOMIC_WAITS_NULLPTR)
        {
          if(!increment_use_count)
          {
            // not found
            return WG14_ATOMIC_WAITS_NULLPTR;
          }
          bucket->proxy = (WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t) *) calloc(
          1, sizeof(proxy_waiter_t));
          if(bucket->proxy == WG14_ATOMIC_WAITS_NULLPTR)
          {
            // out of memory
            return WG14_ATOMIC_WAITS_NULLPTR;
          }
          const int ret =
          WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_INIT(bucket->proxy);
          if(ret != 0)
          {
            // custom init failed
            free(bucket->proxy);
            bucket->proxy = WG14_ATOMIC_WAITS_NULLPTR;
            return WG14_ATOMIC_WAITS_NULLPTR;
          }
          bucket->key = key;
          // found
          bucket->proxy->use_count = 1;
          return bucket->proxy;
        }
        if(bucket->key == key)
        {
          // found
          if(increment_use_count)
          {
            bucket->proxy->use_count++;
          }
          return bucket->proxy;
        }
      }
      if(WG14_ATOMIC_WAITS_PREFIX(hash_table_grow)(table) != 0)
      {
        // out of memory or hit maximum size limit
        return WG14_ATOMIC_WAITS_NULLPTR;
      }
    }
  }

  // MUST HOLD THE LOCK ON ENTRY
  static void WG14_ATOMIC_WAITS_PREFIX(hash_table_remove_item)(
  WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *const table,
  const volatile void *object)
  {
    void *const key = (void *) object;
    if(table->buckets == WG14_ATOMIC_WAITS_NULLPTR)
    {
      return;
    }
    const unsigned h =
    WG14_ATOMIC_WAITS_PREFIX(hash_func)(key) & (table->bucket_count - 1);
    for(unsigned step = 0; step < table->bucket_count; step++)
    {
      const unsigned idx = (h + step * step) & (table->bucket_count - 1);
      WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *bucket = &table->buckets[idx];
      if(bucket->key == key)
      {
        // found
        assert(bucket->proxy->use_count == 0);
        const int ret =
        WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_DESTROY(bucket->proxy);
        if(ret != 0)
        {
          abort();
        }
        free(bucket->proxy);
        memset(bucket, 0, sizeof(*bucket));
        for(step++; step < table->bucket_count; step++)
        {
          const unsigned idx2 = (h + step * step) & (table->bucket_count - 1);
          WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *next_bucket =
          &table->buckets[idx2];
          if(next_bucket->key != WG14_ATOMIC_WAITS_NULLPTR)
          {
            *bucket = *next_bucket;
            bucket = next_bucket;
          }
        }
        return;
      }
    }
  }

  static WG14_ATOMIC_WAITS_INLINE void WG14_ATOMIC_WAITS_PREFIX(
  atomic_load_generic)(uint8_t *dest, const volatile void *object, size_t bytes,
                       WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order)
  {
    switch(bytes)
    {
    case 1:
    {
      const uint8_t v = WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
      (const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *) object,
      order);
      memcpy(dest, &v, bytes);
      break;
    }
    case 2:
    {
      const uint16_t v = WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
      (const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *) object,
      order);
      memcpy(dest, &v, bytes);
      break;
    }
    case 4:
    {
      const uint32_t v = WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
      (const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *) object,
      order);
      memcpy(dest, &v, bytes);
      break;
    }
    case 8:
    {
      const uint64_t v = WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(
      (const WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *) object,
      order);
      memcpy(dest, &v, bytes);
      break;
    }
    default:
      abort();
    }
  }


  static WG14_ATOMIC_WAITS_INLINE void
  WG14_ATOMIC_WAITS_PREFIX(monotonic_now)(struct timespec *ts)
  {
#if defined(_WIN32) || defined(_WIN64)
    // MSVC's CRT provides neither clock_gettime nor CLOCK_MONOTONIC, so use
    // the Win32 high-resolution monotonic counter instead.
    //
    // QueryPerformanceFrequency returns a constant non-zero value for the
    // life of the process, so cache it. Zero also serves as the
    // not-yet-initialised sentinel. The shared library is compiled in a
    // single translation unit, so exactly one cache exists per process.
    static LARGE_INTEGER freq;
    if(freq.QuadPart == 0)
    {
      QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    ts->tv_sec = (time_t) (count.QuadPart / freq.QuadPart);
    ts->tv_nsec =
    (long) (((count.QuadPart % freq.QuadPart) * 1000000000L) / freq.QuadPart);
#else
  (void) clock_gettime(CLOCK_MONOTONIC, ts);
#endif
  }

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(
  atomic_wait_generic)(const volatile void *object, size_t bytes,
                       void *expected, const struct timespec *duration,
                       WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order success,
                       WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order failure)
  {
    int ret = 0;
    struct timespec end;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      struct timespec now;
      WG14_ATOMIC_WAITS_PREFIX(monotonic_now)(&now);
      end.tv_sec = now.tv_sec + duration->tv_sec;
      end.tv_nsec = now.tv_nsec + duration->tv_nsec;
      if(end.tv_nsec >= 1000000000)
      {
        end.tv_sec++;
        end.tv_nsec -= 1000000000;
      }
    }
    WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *const table =
    &WG14_ATOMIC_WAITS_PREFIX(shared_global_hash_table);
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order = failure;
    WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t) *item = WG14_ATOMIC_WAITS_NULLPTR;
    bool lock_is_held = false;
    for(;;)
    {
      union
      {
        uint64_t as_uint64;
        uint8_t as_bytes[8];
      } current;
      WG14_ATOMIC_WAITS_PREFIX(atomic_load_generic)(current.as_bytes, object,
                                                    bytes, order);
      if(memcmp(current.as_bytes, expected, bytes) != 0)
      {
        // We have successfully been woken
        memcpy(expected, current.as_bytes, bytes);
        break;
      }
      if(item == WG14_ATOMIC_WAITS_NULLPTR)
      {
        WG14_ATOMIC_WAITS_PREFIX(hash_table_lock)(table);
        item = WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(
        table, object, true);
        if(item == WG14_ATOMIC_WAITS_NULLPTR)
        {
          WG14_ATOMIC_WAITS_PREFIX(hash_table_unlock)(table);
          return -1;
        }
        // Recheck our atomic now we are holding the lock
        WG14_ATOMIC_WAITS_PREFIX(atomic_load_generic)(current.as_bytes, object,
                                                      bytes, order);
        if(memcmp(current.as_bytes, expected, bytes) != 0)
        {
          // We have successfully been woken. Probably delete my
          // entry.
          memcpy(expected, current.as_bytes, bytes);
          lock_is_held = true;
          break;
        }
        WG14_ATOMIC_WAITS_PREFIX(hash_table_unlock)(table);
        ret = 1;  // return that we did enter a wait cycle
      }

      struct timespec ts_remaining;
      if(duration != WG14_ATOMIC_WAITS_NULLPTR)
      {
        struct timespec now;
        WG14_ATOMIC_WAITS_PREFIX(monotonic_now)(&now);
        if(now.tv_sec > end.tv_sec ||
           (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
        {
          errno = ETIME;
          return 0;
        }
        ts_remaining.tv_sec = end.tv_sec - now.tv_sec;
        if(end.tv_nsec >= now.tv_nsec)
          ts_remaining.tv_nsec = end.tv_nsec - now.tv_nsec;
        else
        {
          ts_remaining.tv_sec--;
          ts_remaining.tv_nsec = end.tv_nsec - now.tv_nsec + 1000000000;
        }
      }
      const int ret2 = WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAIT(
      item, (duration != WG14_ATOMIC_WAITS_NULLPTR) ?
            &ts_remaining :
            WG14_ATOMIC_WAITS_NULLPTR);
      if(ret2 < 0 && ret2 != -EAGAIN && ret2 != -EINTR && ret2 != -ETIME &&
         ret2 != -ETIMEDOUT)
      {
        errno = -ret2;
        return -1;
      }
      order = success;
    }
    if(item != WG14_ATOMIC_WAITS_NULLPTR)
    {
      if(!lock_is_held)
      {
        WG14_ATOMIC_WAITS_PREFIX(hash_table_lock)(table);
      }
      // Decrement the use count, and if we are the last waiter delete
      // ourselves
      if(0 == --item->use_count)
      {
        WG14_ATOMIC_WAITS_PREFIX(hash_table_remove_item)(table, object);
      }
      WG14_ATOMIC_WAITS_PREFIX(hash_table_unlock)(table);
    }
    return ret;
  }

  static WG14_ATOMIC_WAITS_INLINE int
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(volatile void *object,
                                                  unsigned max_threads_to_wake)
  {
    WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *const table =
    &WG14_ATOMIC_WAITS_PREFIX(shared_global_hash_table);
    WG14_ATOMIC_WAITS_PREFIX(hash_table_lock)(table);
    WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t) *const item =
    WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(table, object, false);
    if(item == WG14_ATOMIC_WAITS_NULLPTR)
    {
      // Nothing to wake
      WG14_ATOMIC_WAITS_PREFIX(hash_table_unlock)(table);
      return 0;
    }
    const int ret = WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAKE(
    item, max_threads_to_wake);
    WG14_ATOMIC_WAITS_PREFIX(hash_table_unlock)(table);
    return ret;
  }

  /************************ External API
   * ********************************/

  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_1)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object,
  uint_least8_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_8
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(object, order) ==
          expected)
    {
      (void) WG14_ATOMIC_WAITS_PREFIX(
      wait_on_address8(object, expected, WG14_ATOMIC_WAITS_NULLPTR));
    }
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
  object, 1, &expected, WG14_ATOMIC_WAITS_NULLPTR, order, order);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_2)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object,
  uint_least16_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_16
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(object, order) ==
          expected)
    {
      (void) WG14_ATOMIC_WAITS_PREFIX(
      wait_on_address16(object, expected, WG14_ATOMIC_WAITS_NULLPTR));
    }
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
  object, 2, &expected, WG14_ATOMIC_WAITS_NULLPTR, order, order);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_4)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object,
  uint_least32_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(object, order) ==
          expected)
    {
      (void) WG14_ATOMIC_WAITS_PREFIX(
      wait_on_address32(object, expected, WG14_ATOMIC_WAITS_NULLPTR));
    }
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
  object, 4, &expected, WG14_ATOMIC_WAITS_NULLPTR, order, order);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_wait_8)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object,
  uint_least64_t expected, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_64
    while(WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(object, order) ==
          expected)
    {
      (void) WG14_ATOMIC_WAITS_PREFIX(
      wait_on_address64(object, expected, WG14_ATOMIC_WAITS_NULLPTR));
    }
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
  object, 8, &expected, WG14_ATOMIC_WAITS_NULLPTR, order, order);
#endif
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_8
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address8)(object, 1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, 1);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_16
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address16)(object, 1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, 1);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(object, 1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, 1);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one_8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(object, 1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, 1);
#endif
  }

  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_1)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least8_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_8
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address8)(object, (unsigned) -1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, (unsigned) -1);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_2)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least16_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_16
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address16)(object, (unsigned) -1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, (unsigned) -1);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_4)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(object, (unsigned) -1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, (unsigned) -1);
#endif
  }
  void WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all_8)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least64_t *object)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_64
    (void) WG14_ATOMIC_WAITS_PREFIX(wake_by_address64)(object, (unsigned) -1);
#else
  (void) WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, (unsigned) -1);
#endif
  }

  int WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected_32)(
  const volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t
  *__restrict object,
  uint_least32_t *__restrict expected,
  const struct timespec *__restrict duration,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order success,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order failure)
  {
#if WG14_ATOMIC_WAITS_HAVE_WAIT_ON_ADDRESS_32
    int ret = 0;
    struct timespec end;
    if(duration != WG14_ATOMIC_WAITS_NULLPTR)
    {
      struct timespec now;
      WG14_ATOMIC_WAITS_PREFIX(monotonic_now)(&now);
      end.tv_sec = now.tv_sec + duration->tv_sec;
      end.tv_nsec = now.tv_nsec + duration->tv_nsec;
      if(end.tv_nsec >= 1000000000)
      {
        end.tv_sec++;
        end.tv_nsec -= 1000000000;
      }
    }
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order order = failure;
    for(;;)
    {
      const uint_least32_t current =
      WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_load_explicit(object, order);
      if(current != *expected)
      {
        *expected = current;
        return ret;
      }
      ret = 1;

      struct timespec ts_remaining;
      if(duration != WG14_ATOMIC_WAITS_NULLPTR)
      {
        struct timespec now;
        WG14_ATOMIC_WAITS_PREFIX(monotonic_now)(&now);
        if(now.tv_sec > end.tv_sec ||
           (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
        {
          errno = ETIME;
          return 0;
        }
        ts_remaining.tv_sec = end.tv_sec - now.tv_sec;
        if(end.tv_nsec >= now.tv_nsec)
          ts_remaining.tv_nsec = end.tv_nsec - now.tv_nsec;
        else
        {
          ts_remaining.tv_sec--;
          ts_remaining.tv_nsec = end.tv_nsec - now.tv_nsec + 1000000000;
        }
      }
      const int ret2 = WG14_ATOMIC_WAITS_PREFIX(wait_on_address32)(
      object, *expected,
      (duration != WG14_ATOMIC_WAITS_NULLPTR) ? &ts_remaining :
                                                WG14_ATOMIC_WAITS_NULLPTR);
      if(ret2 < 0 && ret2 != -EAGAIN && ret2 != -EINTR && ret2 != -ETIME &&
         ret2 != -ETIMEDOUT)
      {
        errno = -ret2;
        return -1;
      }
      order = success;
    }
#else
  return WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(
  object, 4, expected, duration, success, failure);
#endif
  }

  int WG14_ATOMIC_WAITS_PREFIX(atomic_notify_32)(
  volatile WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t
  *__restrict object,
  uint_least32_t *__restrict expected, uint_least32_t desired,
  unsigned max_threads_to_wake,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order success,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order failure)
  {
    if(max_threads_to_wake == 0)
    {
      return 0;
    }
    if(!WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_compare_exchange_strong_explicit(
       object, expected, desired, success, failure))
    {
      return 0;
    }
#if WG14_ATOMIC_WAITS_HAVE_WAKE_BY_ADDRESS_32
    const int ret =
    WG14_ATOMIC_WAITS_PREFIX(wake_by_address32)(object, max_threads_to_wake);
#else
  const int ret =
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(object, max_threads_to_wake);
#endif
    return (ret < 0) ? ret : (1 + ret);
  }

#ifdef __cplusplus
}
#endif
