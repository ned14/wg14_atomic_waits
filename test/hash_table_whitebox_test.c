// White-box regression test for analysis §1.11. Includes the platform
// backend .ipp directly (header-only mode) and drives the internal hash table
// single-threaded, so it exercises triangular probing, rehash-under-load, and
// tombstone deletion with thousands of keys in milliseconds — fast enough for
// every CI configuration, including ThreadSanitizer (which cannot create the
// ~800 concurrent waiters a public-API rehash-under-load test would need).
//
// The feature-test macros below mirror the compiled library's build
// (CMakeLists.txt) so clock_gettime etc. are declared on glibc; the platform
// backend is selected by the header exactly as in the library build.

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 1
#include "test_common.h"

#define WHITEBOX_KEYS 5000

int hash_table_whitebox_test_main(void)
{
  int ret = 0;
  WG14_ATOMIC_WAITS_PREFIX(hash_table_t) *const table =
  &WG14_ATOMIC_WAITS_PREFIX(shared_global_hash_table);
  enum
  {
    N = WHITEBOX_KEYS
  };
  void *keys[N];
  WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t) * proxies[N];
  for(int i = 0; i < N; i++)
  {
    // Synthetic keys must be non-NULL and never equal the tombstone sentinel
    // ((void *) 1); 0x1000-aligned values give varied low hash bits.
    keys[i] = (void *) (uintptr_t) (0x1000 + (uintptr_t) i * 0x1000);
  }

  WG14_ATOMIC_WAITS_PREFIX(hash_table_lock)(table);

  // Phase 1: insert all N keys. The table grows at 3/4 load, so inserting 5000
  // keys forces several rehashes (1024 -> ... -> 8192 buckets); every insert
  // must succeed and nothing may be dropped by a rehash.
  for(int i = 0; i < N; i++)
  {
    proxies[i] =
    WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(table, keys[i], true);
    if(proxies[i] == WG14_ATOMIC_WAITS_NULLPTR)
    {
      fprintf(stderr, "FATAL: insert of key %d failed (rehash dropped it?)\n",
              i);
      ret++;
    }
  }
  CHECK(table->bucket_count > WG14_ATOMIC_WAITS_HASH_BUCKETS_INITIAL);
  CHECK(table->count == (unsigned) N);

  // Phase 2: every key is findable and resolves to the same proxy instance.
  for(int i = 0; i < N; i++)
  {
    CHECK(WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(
          table, keys[i], false) == proxies[i]);
  }

  // Phase 3: remove every even key. The odd keys must remain findable (the
  // tombstone scheme must not strand items on their own probe paths, which the
  // pre-fix backward-shift compaction did), and the removed keys must be gone.
  for(int i = 0; i < N; i += 2)
  {
    // Model the waiter's exit (the generic path decrements use_count and only
    // then removes): remove_item requires use_count == 0.
    proxies[i]->use_count = 0;
    WG14_ATOMIC_WAITS_PREFIX(hash_table_remove_item)(table, keys[i]);
  }
  CHECK(table->count == (unsigned) (N - N / 2));
  for(int i = 0; i < N; i++)
  {
    WG14_ATOMIC_WAITS_PREFIX(proxy_waiter_t) *const p =
    WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(table, keys[i], false);
    if(i & 1)
    {
      CHECK(p == proxies[i]);
    }
    else
    {
      CHECK(p == WG14_ATOMIC_WAITS_NULLPTR);
    }
  }

  // Phase 4: re-insert the even keys (inserts reuse the earliest tombstone on
  // their probe path); all N keys must be present again.
  for(int i = 0; i < N; i += 2)
  {
    proxies[i] =
    WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(table, keys[i], true);
    CHECK(proxies[i] != WG14_ATOMIC_WAITS_NULLPTR);
  }
  CHECK(table->count == (unsigned) N);
  for(int i = 0; i < N; i++)
  {
    CHECK(WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(
          table, keys[i], false) == proxies[i]);
  }

  // Phase 5: remove everything; the table must be empty and nothing findable.
  for(int i = 0; i < N; i++)
  {
    // As above: model the last waiter exiting before the node is removed.
    proxies[i]->use_count = 0;
    WG14_ATOMIC_WAITS_PREFIX(hash_table_remove_item)(table, keys[i]);
  }
  CHECK(table->count == 0);
  for(int i = 0; i < N; i++)
  {
    CHECK(WG14_ATOMIC_WAITS_PREFIX(hash_table_find_or_create)(
          table, keys[i], false) == WG14_ATOMIC_WAITS_NULLPTR);
  }

  WG14_ATOMIC_WAITS_PREFIX(hash_table_unlock)(table);
  return ret;
}

int main(void)
{
  return hash_table_whitebox_test_main();
}
