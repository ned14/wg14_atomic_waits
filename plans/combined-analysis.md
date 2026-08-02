# Combined Implementation Analysis — wg14_atomic_waits

The single source of truth is **`docs/proposal.md`** (proposed §7.17.7.7–7.17.7.11).

---

## 1. Deviations from the proposal

These are places where the implementation does not meet what the proposal requires.

### 1.1 (Critical) Fallback proxy busy-spins instead of suspending

**Proposal basis:** §7.17.7.7 (and §7.17.7.10) says the calling thread "is suspended until it
is woken up … if when woken up the value still compares equal to `expected` the thread is
suspended again." That is an explicit requirement to actually sleep, and to keep sleeping on
re-park.

**Implementation:** the default hash-table proxy in `atomic_wait_common.ipp.ipp` is a
mono-directional `0 → 1` flag:
```c
..._WAIT:  wait_on_address32(&(x)->atomic, 0, ...)   /* park while proxy->atomic == 0 */
..._WAKE:  atomic_store_explicit(&(x)->atomic, 1, release); wake_by_address32(...)
```
The flag is **never written back to `0` while the node lives** (it is reset only at node
creation in `hash_table_find_or_create`). A waiter that must re-park (spurious wake, or a
wake with the value still equal) parks on expected `0` while `proxy->atomic == 1`; the kernel
returns `EAGAIN` immediately and the waiter **busy-spins at 100% CPU** instead of being
"suspended again". A later `store + notify` then has no sleeping thread to wake, so the wake
is lost and correctness depends on the spin-poll.

Affects the hash-table fallback: Linux 1/2/8-byte; macOS/FreeBSD 1/2-byte. (The pthreads
backend is fixed — it uses its own mutex/condvar/token proxy that does sleep correctly.)
Files: `atomic_wait_common.ipp.ipp:41-49`, `:220-222`

### 1.2 (Critical) Same defect viewed as a notify-before-park lost wake

**Proposal basis:** §7.17.7.8 "If prior to the call a thread has been suspended … at least one
such thread is woken up." The wording only guarantees wakening threads suspended *prior* to
the notify, so a notify that lands before the waiter parks is not, on its own, a violation —
but the combination with 1.1 turns it into a permanent busy-loop.

**Implementation:** Thread A is preempted before `hash_table_find_or_create`; Thread B
notifies (setting `proxy->atomic = 1`) with nobody parked; Thread A then parks on expected `0`
but the flag is already `1` → the wait returns immediately and A loops forever.

### 1.3 (High) `memory_order` result not tied to suspension in `atomic_wait_expected`

**Proposal basis:** §7.17.7.10 Returns: "If no thread suspension occurred or duration timeout
occurs, returns zero and the memory synchronization ordering will be `failure`. Otherwise, if
the calling thread was suspended at least once, returns a positive number and the memory
synchronization ordering will be `success`."

**Implementation:** `atomic_wait_generic` (and the shared path) sets `order = success`
**unconditionally** after every park return, including retryable/spurious returns that lead to
a zero (timeout / no-suspension) result. The loads that precede a zero return are therefore
performed with `success` ordering, not `failure` as the proposal mandates. This is a direct
deviation. Files: `atomic_wait_common.ipp.ipp:453`

### 1.4 (Medium) Store/exchange wakeups (NOTE 1) not implemented on the fallback path

**Proposal basis:** §7.17.7.7 NOTE 1 and §7.17.7.10 NOTE 1: "a wakeup is possibly triggered by
either a notifying operation or by an atomic store or exchange."

**Implementation:** on the hash-table fallback path the waiter parks on the *proxy* object,
not the user's atomic. A plain `atomic_store` on the user object never signals the proxy, so a
store alone cannot unblock a genuinely-sleeping fallback waiter; store-triggered wakeups are
only observed via the busy-spin poll (which is the bug in 1.1). So the "wakeup triggered by a
store" behaviour is effectively absent on this path. (Treating NOTE 1 as advisory softens this
to an under-specification — see §2.)

---

## 2. Where the proposal is under-specified (implementation is defensible but the wording is loose)

These are cases where the implementation deviates from a *literal* reading or where the
proposal leaves behaviour open; the flag here is "document for the reader", not "fix the code".

- **`atomic_notify` return when CAS succeeds but no waiters are parked.** §7.17.7.11 Returns
  ties a positive return to "the atomic was successfully notified", with "possibly one plus the
  number of threads woken". If no thread is parked, whether it was "successfully notified" is
  ambiguous. The implementation returns `1` (`1 + ret`) on a successful exchange even with no
  waiters. That is consistent with a literal reading ("positive … possibly one plus the number
  woken") but the proposal would benefit from stating explicitly whether a successful exchange
  with zero waiters returns `0` or a positive value. Not a clear violation.
- **`atomic_notify_all` return value on macOS.** §7.17.7.11 says "positive number (possibly one
  plus the number of threads woken)". `__ulock_wake` returns no count, so the implementation
  returns a fixed `INT_MAX - 1` for `max != 1`. This satisfies "positive" but the magnitude is
  arbitrary; the proposal doesn't bound or specify it. Not a violation, but under-specified.
  Files: `atomic_wait_macos.c.ipp:127`, `:141`
- **`atomic_wait_expected` positive result when only "intent to park" occurred.**
  §7.17.7.10 ties positive to "suspended at least once". `atomic_wait_generic` pre-sets
  `ret = 1` before the first *actual* kernel suspension, so a spurious return before a real
  park can be reported positive. Loosely worded in the proposal; the intent-to-suspend reading
  is defensible.
- **`atomic_notify` under-reports the woken count when no proxy node is registered.**
  `atomic_notify_generic` returns `0` when `hash_table_find_or_create(..., false)` finds
  nothing. The proposal only requires "at least `max_threads_to_wake` … unblocked" on success,
  so this is consistent; the "possibly one plus the number woken" wording leaves the exact
  count open.
- **Store-triggered wakeups and the lost-wake-before-park (native) case.** §7.17.7.8's
  "threads suspended prior to the call" phrasing means a notify that precedes a waiter's park
  is not guaranteed to wake it. On the native fast path (Linux/macOS/FreeBSD/Windows) a waiter
  that loads `*object == expected`, is preempted while a value-unchanging notify fires, then
  parks, can block with no further notify. This is an inherent futex-style limitation and not a
  proposal violation, but the proposal could state the intended pairing of notify with a
  value-changing store more explicitly.

---

## 3. Non-proposal (internal / portability / test) notes

These are implementation-quality concerns not grounded in the proposal text, kept for
completeness:

- **Header-only definitions not `static inline`.** The `atomic_wait_*_N` /
  `atomic_notify*_N` definitions rely on the C `extern-inline` model; workable on GCC/Clang but
  fragile in the multi-TU header-only build. The `hash_table()` weak singleton is fine.
- **`errno` clobbered on public return paths.** The syscall wrappers preserve `errno`, but
  `atomic_wait_expected_32` then sets `errno = ETIME` (timeout) / `errno = -ret2` (error).
- **`hash_func` truncates a 64-bit pointer to 32-bit** before mixing — avoidable hash
  collisions only.
- **`benchmark_atomic_wait_test.c` runs no benchmark** (returns 0); CI excludes it correctly so
  it validates nothing.

---

## 4. Missing test coverage

- **Hash-table path** — tests only use 4-byte `atomic_int`/`atomic_uint_least32_t`, which
  bypass the hash table; 1/2/8-byte (Linux) fallback untested. Leaves 1.1/1.2 uncaught by CI.
- **Notify-without-store / notify-before-park** — never tested; exercises §2's native lost-wake
  case and 1.1.
- **Multiple waiters / `atomic_notify_one` / `max_threads_to_wake` cap** — only
  `notify_all`-wakes-all is tested.
- **Spurious wakes in the hash-table path** — re-compare-and-re-park loop never exercised.

---

## 5. Summary

| Issue | Severity | Basis / Location |
|-------|----------|------------------|
| Fallback proxy flag stuck at `atomic=1` → busy-spin not suspension | **Critical** | proposal §7.17.7.7; `atomic_wait_common.ipp.ipp:41-49`, `:220-222` |
| Lost-wake: notify before park (hash table) | **Critical** | §7.17.7.8 + 1.1; `atomic_wait_common.ipp.ipp:220-222`, `:41-45` |
| `order=success` set unconditionally (failure-ordering deviation) | **High** | §7.17.7.10 Returns; `atomic_wait_common.ipp.ipp:453` |
| Store/exchange wakeups absent on fallback path | Medium | §7.17.7.7 / §7.17.7.10 NOTE 1 |
| macOS `notify_all` returns `INT_MAX - 1` | Medium | §7.17.7.11 (under-specified); `atomic_wait_macos.c.ipp:127` |
| CAS-success-with-no-waiters returns `1` | Low | §7.17.7.11 (under-specified) |
| Header-only defs not `static inline` | Low | internal |
| `errno` clobbered on timeout/error returns | Low | internal |
| No tests for hash-table path | **High** | `test/atomic_wait_test.c` |
| No tests for notify-without-store | **High** | `test/atomic_notify_test.c` |

The dominant defect remains **1.1**: the default hash-table proxy flag is never re-armed, so
waiters busy-spin instead of suspending, which violates §7.17.7.7's suspension requirement and
loses wakes — the exact race the earlier analyses identified.
