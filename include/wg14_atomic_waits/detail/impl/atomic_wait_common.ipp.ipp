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
#include "../../config.h"
#include "lock_unlock.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <errno.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_s)
  {
    struct WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_s) * next;
    volatile int notified;
    pthread_cond_t cv;
    pthread_mutex_t mtx;
    uint8_t expected_storage[8];
  } WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t);

  typedef struct
  {
    WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) * head;
    WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) * tail;
  } WG14_ATOMIC_WAITS_PREFIX(wait_queue_t);

#define WG14_ATOMIC_WAITS_HASH_BUCKETS_INITIAL 1024
#define WG14_ATOMIC_WAITS_HASH_BUCKETS_MAX (1024 * 1024)


  typedef struct
  {
    WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) queue;
    void *key;
  } WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t);

  typedef struct
  {
    WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) * buckets;
    unsigned bucket_count;
    unsigned lock;
  } WG14_ATOMIC_WAITS_PREFIX(hash_table_t);

  static WG14_ATOMIC_WAITS_INLINE WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *
  WG14_ATOMIC_WAITS_PREFIX(hash_table)(void)
  {
    static WG14_ATOMIC_WAITS_PREFIX(hash_table_t) table;
    return &table;
  }

  static WG14_ATOMIC_WAITS_INLINE unsigned
  WG14_ATOMIC_WAITS_PREFIX(hash_func)(const void *key, unsigned mask)
  {
    uintptr_t h = (uintptr_t) key;
    h = ((h >> 3) ^ (h >> (3 + 4))) * 0x9E3779B9u;
    return (unsigned) h & mask;
  }

  static WG14_ATOMIC_WAITS_INLINE void *
  WG14_ATOMIC_WAITS_PREFIX(volatile_to_voidp)(const volatile void *p)
  {
    return (void *) (uintptr_t) p;
  }

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(hash_table_grow)(
  WG14_ATOMIC_WAITS_PREFIX(hash_table_t) * table)
  {
    unsigned old_count = table->bucket_count;
    unsigned new_count = old_count * 2;
    if(new_count > WG14_ATOMIC_WAITS_HASH_BUCKETS_MAX)
      new_count = WG14_ATOMIC_WAITS_HASH_BUCKETS_MAX;
    if(new_count <= old_count)
      return -1;
    WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *new_buckets =
    (WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *) calloc(
    new_count, sizeof(WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t)));
    if(!new_buckets)
      return -1;
    unsigned mask = new_count - 1;
    for(unsigned i = 0; i < old_count; i++)
    {
      WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *old_bucket = &table->buckets[i];
      if(old_bucket->key == NULL)
        continue;
      unsigned h = WG14_ATOMIC_WAITS_PREFIX(hash_func)(old_bucket->key, mask);
      for(unsigned step = 0; step < new_count; step++)
      {
        unsigned idx = (h + step * step) & mask;
        WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *new_bucket = &new_buckets[idx];
        if(new_bucket->key == NULL)
        {
          new_bucket->key = old_bucket->key;
          new_bucket->queue = old_bucket->queue;
          break;
        }
      }
    }
    free(table->buckets);
    table->buckets = new_buckets;
    table->bucket_count = new_count;
    return 0;
  }

  static WG14_ATOMIC_WAITS_INLINE WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) *
  WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(
  const volatile void *object)
  {
    WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *table =
    WG14_ATOMIC_WAITS_PREFIX(hash_table)();
    void *key = WG14_ATOMIC_WAITS_PREFIX(volatile_to_voidp)(object);

    LOCK(table->lock);
    if(table->buckets == NULL)
    {
      table->buckets = (WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *) calloc(
      WG14_ATOMIC_WAITS_HASH_BUCKETS_INITIAL,
      sizeof(WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t)));
      table->bucket_count = WG14_ATOMIC_WAITS_HASH_BUCKETS_INITIAL;
      if(!table->buckets)
      {
        UNLOCK(table->lock);
        return NULL;
      }
    }
    for(;;)
    {
      unsigned h =
      WG14_ATOMIC_WAITS_PREFIX(hash_func)(key, table->bucket_count - 1);
      for(unsigned step = 0; step < table->bucket_count; step++)
      {
        unsigned idx = (h + step * step) & (table->bucket_count - 1);
        WG14_ATOMIC_WAITS_PREFIX(hash_bucket_t) *bucket = &table->buckets[idx];
        if(bucket->key == NULL)
        {
          bucket->key = key;
          memset(&bucket->queue, 0, sizeof(bucket->queue));
          UNLOCK(table->lock);
          return &bucket->queue;
        }
        if(bucket->key == key)
        {
          UNLOCK(table->lock);
          return &bucket->queue;
        }
      }
      if(WG14_ATOMIC_WAITS_PREFIX(hash_table_grow)(table) != 0)
        break;
    }
    UNLOCK(table->lock);
    return NULL;
  }

  static WG14_ATOMIC_WAITS_INLINE void WG14_ATOMIC_WAITS_PREFIX(wq_append)(
  WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) * queue,
  WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) * item)
  {
    item->next = NULL;
    if(queue->tail)
      queue->tail->next = item;
    else
      queue->head = item;
    queue->tail = item;
  }

  static WG14_ATOMIC_WAITS_INLINE void WG14_ATOMIC_WAITS_PREFIX(wq_remove)(
  WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) * queue,
  WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) * item)
  {
    WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) **p = &queue->head;
    while(*p)
    {
      if(*p == item)
      {
        *p = item->next;
        if(queue->tail == item)
          queue->tail = NULL;
        item->next = NULL;
        return;
      }
      p = &(*p)->next;
    }
  }

  static WG14_ATOMIC_WAITS_INLINE WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) *
  WG14_ATOMIC_WAITS_PREFIX(wq_item_new)(const void *expected, size_t size)
  {
    WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) *item =
    (WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) *) malloc(sizeof(*item));
    if(!item)
      return NULL;
    memset(item, 0, sizeof(*item));
    memcpy(item->expected_storage, expected, size);
    pthread_cond_init(&item->cv, NULL);
    pthread_mutex_init(&item->mtx, NULL);
    return item;
  }

  static WG14_ATOMIC_WAITS_INLINE void WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(
  WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) * item)
  {
    if(!item)
      return;
    pthread_cond_destroy(&item->cv);
    pthread_mutex_destroy(&item->mtx);
    free(item);
  }

  static WG14_ATOMIC_WAITS_INLINE unsigned WG14_ATOMIC_WAITS_PREFIX(
  wake_waiters)(WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) * queue, unsigned max)
  {
    unsigned count = 0;
    WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) *item = queue->head;

    while(item && count < max)
    {
      if(!item->notified)
      {
        item->notified = 1;
        pthread_cond_signal(&item->cv);
        count++;
      }
      item = item->next;
    }
    return count;
  }

  static WG14_ATOMIC_WAITS_INLINE uint64_t
  WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(const volatile void *object,
                                             size_t size, memory_order order)
  {
    if(size == 1)
      return (uint64_t) atomic_load_explicit(
      (const volatile _Atomic(uint_least8_t) *) object,
      WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
    if(size == 2)
      return (uint64_t) atomic_load_explicit(
      (const volatile _Atomic(uint_least16_t) *) object,
      WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
    if(size == 4)
      return (uint64_t) atomic_load_explicit(
      (const volatile _Atomic(uint_least32_t) *) object,
      WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
    return atomic_load_explicit(
    (const volatile _Atomic(uint_least64_t) *) object,
    WG14_ATOMIC_WAITS_ATOMIC_PREFIX order);
  }

  static WG14_ATOMIC_WAITS_INLINE void
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_generic)(const volatile void *object,
                                                size_t size, uint64_t expected,
                                                memory_order order)
  {
    WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *htable =
    WG14_ATOMIC_WAITS_PREFIX(hash_table)();

    for(;;)
    {
      uint64_t current = WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(
      (const volatile void *) object, size, order);
      if(current != expected)
        return;

      WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) *queue =
      WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(object);
      if(!queue)
        return;

      WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) *item =
      WG14_ATOMIC_WAITS_PREFIX(wq_item_new)(&expected, size);
      if(!item)
        return;

      for(;;)
      {
        uint64_t cur = WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(
        (const volatile void *) object, size, order);
        if(cur != expected)
        {
          WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
          return;
        }

        LOCK(htable->lock);
        WG14_ATOMIC_WAITS_PREFIX(wq_append)(queue, item);
        uint64_t cur2 = WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(
        (const volatile void *) object, size, order);
        if(cur2 != expected)
        {
          WG14_ATOMIC_WAITS_PREFIX(wq_remove)(queue, item);
          UNLOCK(htable->lock);
          WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
          return;
        }
        UNLOCK(htable->lock);
        break;
      }

      pthread_mutex_lock(&item->mtx);
      while(!item->notified)
      {
        uint64_t cur = WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(
        (const volatile void *) object, size, order);
        if(cur != expected)
          break;
        struct timespec ts;
        ts.tv_sec = 5;
        ts.tv_nsec = 0;
        pthread_cond_timedwait(&item->cv, &item->mtx, &ts);
      }
      pthread_mutex_unlock(&item->mtx);

      LOCK(htable->lock);
      WG14_ATOMIC_WAITS_PREFIX(wq_remove)(queue, item);
      UNLOCK(htable->lock);

      WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
    }
  }

  static WG14_ATOMIC_WAITS_INLINE void
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_generic)(volatile void *object,
                                                  size_t size,
                                                  unsigned max_threads_to_wake)
  {
    (void) size;
    if(max_threads_to_wake == 0)
      return;

    WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *htable =
    WG14_ATOMIC_WAITS_PREFIX(hash_table)();

    LOCK(htable->lock);
    WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) *queue = WG14_ATOMIC_WAITS_PREFIX(
    hash_table_find_or_create)((const void *) (uintptr_t) object);
    if(queue)
    {
      unsigned count =
      WG14_ATOMIC_WAITS_PREFIX(wake_waiters)(queue, max_threads_to_wake);
      (void) count;
    }
    UNLOCK(htable->lock);
  }

  static WG14_ATOMIC_WAITS_INLINE int WG14_ATOMIC_WAITS_PREFIX(
  atomic_wait_expected_generic)(const volatile void *object, size_t size,
                                void *expected, const struct timespec *duration,
                                memory_order success, memory_order failure)
  {
    WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *htable =
    WG14_ATOMIC_WAITS_PREFIX(hash_table)();
    struct timespec end;

    if(duration)
    {
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      end.tv_sec = now.tv_sec + duration->tv_sec;
      end.tv_nsec = now.tv_nsec + duration->tv_nsec;
      if(end.tv_nsec >= 1000000000)
      {
        end.tv_sec++;
        end.tv_nsec -= 1000000000;
      }
    }

    for(;;)
    {
      uint64_t current =
      WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(object, size, failure);
      uint64_t exp_u64;
      if(size == 1)
        exp_u64 = (uint64_t) *(const volatile uint_least8_t *) expected;
      else if(size == 2)
        exp_u64 = (uint64_t) *(const volatile uint_least16_t *) expected;
      else if(size == 4)
        exp_u64 = (uint64_t) *(const volatile uint_least32_t *) expected;
      else
        exp_u64 = *(const volatile uint_least64_t *) expected;

      if(current != exp_u64)
      {
        if(size == 1)
          *(volatile uint_least8_t *) expected = (uint_least8_t) current;
        else if(size == 2)
          *(volatile uint_least16_t *) expected = (uint_least16_t) current;
        else if(size == 4)
          *(volatile uint_least32_t *) expected = (uint_least32_t) current;
        else
          *(volatile uint_least64_t *) expected = (uint_least64_t) current;
        atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
        return 0;
      }

      WG14_ATOMIC_WAITS_PREFIX(wait_queue_t) *queue =
      WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(object);
      if(!queue)
        return -1;

      WG14_ATOMIC_WAITS_PREFIX(wait_queue_item_t) *item =
      WG14_ATOMIC_WAITS_PREFIX(wq_item_new)(expected, size);
      if(!item)
        return -1;

      for(;;)
      {
        uint64_t cur =
        WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(object, size, failure);
        if(cur != exp_u64)
        {
          WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
          if(size == 1)
            *(volatile uint_least8_t *) expected = (uint_least8_t) cur;
          else if(size == 2)
            *(volatile uint_least16_t *) expected = (uint_least16_t) cur;
          else if(size == 4)
            *(volatile uint_least32_t *) expected = (uint_least32_t) cur;
          else
            *(volatile uint_least64_t *) expected = (uint_least64_t) cur;
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
          return 0;
        }

        LOCK(htable->lock);
        WG14_ATOMIC_WAITS_PREFIX(wq_append)(queue, item);
        uint64_t cur2 =
        WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(object, size, failure);
        if(cur2 != exp_u64)
        {
          WG14_ATOMIC_WAITS_PREFIX(wq_remove)(queue, item);
          UNLOCK(htable->lock);
          WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
          if(size == 1)
            *(volatile uint_least8_t *) expected = (uint_least8_t) cur2;
          else if(size == 2)
            *(volatile uint_least16_t *) expected = (uint_least16_t) cur2;
          else if(size == 4)
            *(volatile uint_least32_t *) expected = (uint_least32_t) cur2;
          else
            *(volatile uint_least64_t *) expected = (uint_least64_t) cur2;
          atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
          return 0;
        }
        UNLOCK(htable->lock);
        break;
      }

      pthread_mutex_lock(&item->mtx);
      int timedout = 0;
      if(duration)
      {
        for(;;)
        {
          struct timespec now;
          clock_gettime(CLOCK_MONOTONIC, &now);
          if(now.tv_sec > end.tv_sec ||
             (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
          {
            timedout = 1;
            break;
          }
          struct timespec remaining;
          remaining.tv_sec = end.tv_sec - now.tv_sec;
          if(end.tv_nsec >= now.tv_nsec)
            remaining.tv_nsec = end.tv_nsec - now.tv_nsec;
          else
          {
            remaining.tv_sec--;
            remaining.tv_nsec = end.tv_nsec - now.tv_nsec + 1000000000;
          }
          uint64_t cur =
          WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(object, size, failure);
          if(cur != exp_u64)
          {
            pthread_mutex_unlock(&item->mtx);
            WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
            if(size == 1)
              *(volatile uint_least8_t *) expected = (uint_least8_t) cur;
            else if(size == 2)
              *(volatile uint_least16_t *) expected = (uint_least16_t) cur;
            else if(size == 4)
              *(volatile uint_least32_t *) expected = (uint_least32_t) cur;
            else
              *(volatile uint_least64_t *) expected = (uint_least64_t) cur;
            atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
            return 1;
          }
          if(!item->notified &&
             pthread_cond_timedwait(&item->cv, &item->mtx, &remaining) == 0)
            break;
        }
      }
      else
      {
        for(;;)
        {
          uint64_t cur =
          WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(object, size, failure);
          if(cur != exp_u64)
          {
            pthread_mutex_unlock(&item->mtx);
            WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
            if(size == 1)
              *(volatile uint_least8_t *) expected = (uint_least8_t) cur;
            else if(size == 2)
              *(volatile uint_least16_t *) expected = (uint_least16_t) cur;
            else if(size == 4)
              *(volatile uint_least32_t *) expected = (uint_least32_t) cur;
            else
              *(volatile uint_least64_t *) expected = (uint_least64_t) cur;
            atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX success);
            return 1;
          }
          pthread_cond_wait(&item->cv, &item->mtx);
        }
      }
      pthread_mutex_unlock(&item->mtx);

      LOCK(htable->lock);
      WG14_ATOMIC_WAITS_PREFIX(wq_remove)(queue, item);
      UNLOCK(htable->lock);

      if(timedout)
      {
        WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
        uint64_t cur =
        WG14_ATOMIC_WAITS_PREFIX(atomic_load_uint)(object, size, failure);
        if(size == 1)
          *(volatile uint_least8_t *) expected = (uint_least8_t) cur;
        else if(size == 2)
          *(volatile uint_least16_t *) expected = (uint_least16_t) cur;
        else if(size == 4)
          *(volatile uint_least32_t *) expected = (uint_least32_t) cur;
        else
          *(volatile uint_least64_t *) expected = (uint_least64_t) cur;
        atomic_signal_fence(WG14_ATOMIC_WAITS_ATOMIC_PREFIX failure);
        return 0;
      }

      WG14_ATOMIC_WAITS_PREFIX(wq_item_delete)(item);
    }
  }

#ifdef __cplusplus
}
#endif
#endif
