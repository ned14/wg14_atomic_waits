OpenRouter
Step 3.7 Flash
15 steps · $0.074698
In 101,235 · Out 23,237 · Reason 0
Cache R 712,064 · W 0 · Hit Rate 87.6%

~Four minutes

# Step 3.7 Implementation Analysis

## 1. Deviations from Plan/Proposal

### 1.1 Windows 1-byte and 2-byte operations use hash table fallback

**Plan:** "Windows: always bypass (all sizes 1–8 bytes). The Windows backend never uses the hash table."

**Actual:** `atomic_wait_windows.c.ipp` only defines `HAVE_WAIT_ON_ADDRESS_32` and `HAVE_WAIT_ON_ADDRESS_64`. There are no `wait_on_address8`, `wait_on_address16`, `wake_by_address8`, or `wake_by_address16` definitions. As a result, `atomic_wait_1`, `atomic_wait_2`, `atomic_notify_one_1`, `atomic_notify_one_2`, `atomic_notify_all_1`, and `atomic_notify_all_2` on Windows fall through to `atomic_wait_generic` / `atomic_notify_generic` and use the hash table proxy.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp:42-74`

### 1.2 macOS `atomic_notify_all` return value is platform-fixed to `INT_MAX - 1`

**Plan:** "Returns a positive number (possibly 1 + N where the extra info depends on platform)."

**Actual:** Both `wake_by_address32` and `wake_by_address64` on macOS return `INT_MAX - 1` when `max_threads_to_wake != 1`. `__ulock_wake` does not return a count of woken threads, so the implementation cannot know how many were woken. Returning a fixed large value is not consistent with the plan's "possibly one plus the number of threads woken" semantics.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_macos.c.ipp:106`, `include/wg14_atomic_waits/detail/impl/atomic_wait_macos.c.ipp:123`

### 1.3 Windows `atomic_notify_all` calls `WakeByAddressSingle` instead of `WakeByAddressAll`

**Plan:** "On Windows, call `WakeByAddressSingle` / `WakeByAddressAll` directly without any `wake_waiters` helper."

**Actual:** `wake_by_address32` and `wake_by_address64` always call `WakeByAddressSingle`, regardless of `max_threads_to_wake`. `WakeByAddressAll` is never called.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp:100`, `include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp:117`

### 1.4 `atomic_wait_common.ipp.ipp` lacks warning suppression for `volatile`-to-`void*` cast

**Plan:** "Test under `-Wextra` to suppress qualifier-discard warnings."

**Actual:** The cast `void *const key = (void *) object;` on line 173 discards both `volatile` and `const` qualifiers. With `-Wextra`, this produces a warning. No `#pragma` or explicit cast-to-`uintptr_t`-then-to-`void*` is used to suppress it.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:173`

### 1.5 `atomic_wait_expected_32` timeout error handling is incorrect on Linux

**Plan:** "Return negative on error."

**Actual:** `wait_on_address32` on Linux always returns `-1` on any failure, losing the distinction between `ETIMEDOUT` and other errors. `atomic_wait_expected_32` then returns `-1` with `errno = 1` (because `-ret2 = -(-1) = 1`) for any error, including timeout. The code compares `ret2 != ETIME && ret2 != ETIMEDOUT`, but `ret2` is always `-1`, so the check never matches the positive `ETIMEDOUT` value.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_linux.c.ipp:50`, `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:653-659`

---

## 2. Bugs

### 2.1 Hash table proxy state machine is permanently stuck after first notify

**Severity:** Critical

**Description:** The proxy waiter uses `proxy->atomic` as a binary semaphore:
- Waiter parks on `proxy->atomic == 0`
- Notifier sets `proxy->atomic = 1` and calls wake

After any notify, `proxy->atomic` is permanently `1`. It is never reset to `0` for an existing proxy. The proxy is only initialized to `0` when first created in `hash_table_find_or_create`.

When a subsequent waiter arrives:
1. It acquires the hash table lock and finds the existing proxy (use_count incremented).
2. It unlocks and calls the platform wait primitive on `proxy->atomic` with expected value `0`.
3. Since `proxy->atomic == 1` (not `0`), the wait returns immediately (EAGAIN on Linux/futex, non-match on Windows/macOS).
4. The waiter reloads `*object`. If it still equals `expected` (which it will if no one notified again), the loop repeats.
5. Step 3 repeats forever: `proxy->atomic` is still `1`, so the wait returns immediately again.

**Result:** Infinite busy-loop on Windows/macOS; `atomic_wait_generic` returns `-1` on Linux.

**Root cause:** There is no mechanism to reset `proxy->atomic` back to `0` after all waiters have consumed a notify.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:38-45`, `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:220-222`

### 2.2 `atomic_wait_generic` returns `-1` on proxy spurious wake instead of looping

**Severity:** High

**Description:** In `atomic_wait_generic`, after calling the proxy wait primitive:
```c
const int ret2 = WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE_WAIT(item, ...);
if(ret2 < 0)
{
  return ret2;
}
```
If the proxy wait returns a negative value (e.g., `-EAGAIN` because `proxy->atomic != 0`), the function returns `-1` immediately. This is incorrect: a spurious wake or a mismatched proxy state should trigger the re-compare-and-re-park loop, not an error return.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:411-418`

### 2.3 `atomic_wait_expected_32` returns `-1` on proxy error instead of looping

**Severity:** High

**Description:** On non-native backends (or when using the hash table fallback), `atomic_wait_expected_32` delegates to `atomic_wait_generic`. If `atomic_wait_generic` returns `-1` due to the proxy state bug (section 2.1), `atomic_wait_expected_32` propagates the `-1` to the caller. Per the proposal, the only negative return should be on actual system errors, not on spurious proxy state mismatches.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:663-664`

### 2.4 `wait_on_address32` (Linux) discards errno, preventing timeout detection

**Severity:** Medium

**Description:** The Linux `wait_on_address32` wrapper:
```c
int ret = (int) syscall(SYS_futex, ...);
if(ret == 0 || errno == EAGAIN || errno == EINTR)
{
    errno = save_errno;
    return 0;
}
errno = save_errno;
return -1;
```
It always returns `-1` on failure, regardless of whether the failure was `ETIMEDOUT`, `EINTR`, or something else. Callers cannot distinguish timeout from other errors.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_linux.c.ipp:43-51`

### 2.5 `atomic_notify_generic` creates a proxy even when no waiters exist

**Severity:** Low

**Description:** `atomic_notify_generic` calls `hash_table_find_or_create(table, object, false)`. If no proxy exists, this creates one with `use_count = 0`, stores `1` to `proxy->atomic`, and calls wake. The proxy is left in the hash table with `use_count = 0` and `proxy->atomic = 1`. When the first waiter later arrives, it inherits the stuck `proxy->atomic = 1` state, triggering the infinite loop described in section 2.1.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:443-446`

---

## 3. Race Conditions (Lost Wakes)

### 3.1 Lost-wake race in hash table proxy between notify and waiter park

**Severity:** Critical

**Timeline:**
1. Thread A calls `atomic_wait`, loads `*object == expected`, enters the hash table path.
2. Thread A is preempted before calling `hash_table_find_or_create`.
3. Thread B calls `atomic_notify_one` (or `atomic_notify_all`). It acquires the hash table lock, looks up the proxy. If none exists, one is created with `use_count = 0`. It stores `1` to `proxy->atomic` and calls `wake_by_address32`. No one is waiting, so the wake has no effect.
4. Thread B releases the lock.
5. Thread A resumes, acquires the lock, finds the proxy (or creates one), increments `use_count` to `1`, releases the lock.
6. Thread A calls the platform wait on `proxy->atomic` with expected `0`.
7. Since `proxy->atomic == 1` (set by Thread B in step 3), the wait returns immediately.
8. Thread A reloads `*object`. If still `== expected` (which it is, because Thread B may not have changed the value), Thread A loops.
9. Thread A calls wait again. `proxy->atomic` is still `1`. Wait returns immediately again.
10. **Result:** Thread A loops forever (Windows/macOS) or returns `-1` error (Linux).

**Root cause:** The notify path sets `proxy->atomic = 1` as a "wake token", but there is no handshake to ensure the waiter has observed the token before it is reused. The proxy has no generation counter or sequence number.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:220-222`, `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:41-45`

### 3.2 Lost-wake race in native paths between load-check and park

**Severity:** High

**Timeline (native Linux/macOS/FreeBSD/Windows):**
1. Thread A calls `atomic_wait`, loads `*object == expected`.
2. Thread A is preempted.
3. Thread B calls `atomic_notify_one` without changing `*object` (e.g., notify_all in a scenario where the value is already the desired state).
4. Thread B calls the platform wake primitive (`FUTEX_WAKE`, `__ulock_wake`, etc.) on `object`. No thread is currently parked, so the wake has no effect.
5. Thread A resumes and calls the platform wait primitive (`FUTEX_WAIT`, `__ulock_wait`, etc.) on `object` with expected value.
6. The kernel parks Thread A because `*object == expected`.
7. No further notify is forthcoming. Thread A blocks forever.

**Why the while-loop doesn't help:** The while-loop (`while(load == expected) { wait(); }`) only helps if the wait returns spuriously or because the value changed. In this race, the wait blocks because the value is still expected, and no one will wake Thread A.

**This is a well-known limitation of futex-style wait/notify when the notify does not change the value.** The C++20 `atomic_wait` / `atomic_notify` design assumes notify is paired with a store that changes the value, but the proposal does not require this.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_linux.c.ipp:43-44`, `include/wg14_atomic_waits/detail/impl/atomic_wait_macos.c.ipp:64-66`, `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:494-499`

---

## 4. Missing Tests

### 4.1 No tests for hash table path

The existing tests use `atomic_int`, which is 4 bytes. On Linux, macOS, FreeBSD, and Windows, 4-byte operations bypass the hash table. No tests exercise 1-byte, 2-byte, or 8-byte (Linux) operations that fall back to the hash table.

**Impact:** The critical bugs in sections 2.1, 2.2, and 3.1 are not caught by CI.

### 4.2 No tests for notify-without-store race

The tests always store a new value before calling notify. They do not test the scenario where notify is called without changing the value, which triggers the lost-wake race in section 3.2.

### 4.3 No tests for multiple waiters on the same object

`atomic_notify_test.c` creates multiple waiters but uses `atomic_notify_all` to wake them all. It does not test `atomic_notify_one` with multiple waiters, or the case where `max_threads_to_wake` limits the number of woken threads.

### 4.4 No tests for spurious wakes in hash table path

The tests do not inject spurious wakes into the hash table proxy to verify that the re-compare-and-re-park loop works correctly.

---

## 5. Other Issues

### 5.1 `atomic_wait_expected_32` timeout conversion uses ceiling, but wait returns `-1` on all errors

The timeout computation in `atomic_wait_expected_32` correctly uses ceiling conversion (`tv_nsec + 999999) / 1000000`). However, because `wait_on_address32` returns `-1` for all errors (section 2.4), the caller cannot distinguish a real timeout from a spurious wake or other kernel error. The outer loop re-checks the deadline before each park, which mitigates this somewhat, but the return value semantics are still wrong.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp:52-53`, `include/wg14_atomic_waits/detail/impl/atomic_wait_macos.c.ipp:58`

### 5.2 `atomic_wait_common.ipp.ipp` default proxy uses `uint_least32_t` but atomic prefix is missing

The default proxy definition:
```c
#define WG14_ATOMIC_WAITS_HASH_TABLE_ITEM_PROXY_TYPE                           \
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t
```
The `ATOMIC_PREFIX` is applied, which is correct for C (`atomic_uint_least32_t`) but in C++ it becomes `std::atomic_uint_least32_t`. This is correct because the header includes `<atomic>` in C++ mode.

### 5.3 `hash_table_remove_item` doesn't reset proxy->atomic before free

Not a bug per se (the proxy is freed), but if the removal is ever skipped due to a non-zero use_count, the proxy remains with `proxy->atomic = 1`.

### 5.4 `hash_table_grow` doesn't reset old bucket keys to NULL after rehashing

In `hash_table_grow`, after rehashing, the old buckets are freed with `free(table->buckets)`. The new buckets are freshly calloc'd, so they are zeroed. This is correct.

### 5.5 `atomic_notify_32` fast-path for `max_threads_to_wake == 0` returns `0` without CAS

The plan explicitly allows this:
> "Use a fast-path check before acquiring the hash table lock: if `max_threads_to_wake == 0`, skip the hash table lookup and `wake_waiters` call entirely and return `0` immediately (no CAS, no notify, no side effects)."

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:674-677`

### 5.6 `atomic_wait_generic` sets `order = success` unconditionally after wait returns

If the wait returns because of an error (e.g., `-EAGAIN` from the proxy), `order` is still set to `success`. This means the subsequent load uses `success` ordering even though no notify occurred. The proposal requires `failure` ordering for accesses before any notify.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:419`

### 5.7 `atomic_wait_expected_32` return value on timeout is `0`, but the proposal says "zero if no suspension occurred, or if duration timeout occurs"

This is correctly implemented: the function returns `0` on timeout and `*expected` is updated to the timed-out value.

**Files:** `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:631-636`

---

## 6. Summary of Critical Issues

| Issue | Severity | Location |
|-------|----------|----------|
| Hash table proxy stuck at `atomic=1` after notify | **Critical** | `atomic_wait_common.ipp.ipp:220-222` |
| Lost-wake race: notify before park in hash table path | **Critical** | `atomic_wait_common.ipp.ipp:41-45` |
| `atomic_wait_generic` returns `-1` instead of looping on proxy error | **High** | `atomic_wait_common.ipp.ipp:411-418` |
| Lost-wake race: notify without store in native path | **High** | `atomic_wait_linux.c.ipp:43-44` |
| Windows `notify_all` uses `WakeByAddressSingle` | **Medium** | `atomic_wait_windows.c.ipp:100` |
| Windows 1/2-byte ops use hash table instead of bypass | **Medium** | `atomic_wait_common.ipp.ipp:524-528` |
| `wait_on_address32` discards errno | **Medium** | `atomic_wait_linux.c.ipp:50-51` |
| macOS `notify_all` ignores `max_threads_to_wake` limit | **Medium** | `atomic_wait_macos.c.ipp:99-106` |
| No tests for hash table path | **High** | `test/atomic_wait_test.c` |
| No tests for notify-without-store | **High** | `test/atomic_notify_test.c` |

The most severe issue is the **hash table proxy state machine** (section 2.1 / race 3.1). The proxy is a single-slot binary semaphore that is set to "notified" (`1`) by the notifier but never reset to "waiting" (`0`) after consumption. This makes the hash table path fundamentally broken for any object that receives more than one notify, or for any waiter that arrives after a notify has already been issued for that object.
