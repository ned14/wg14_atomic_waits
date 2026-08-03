# Combined Implementation Analysis — wg14_atomic_waits

The single source of truth is **`docs/proposal.md`** (proposed §7.17.7.7–7.17.7.11).

---

## 1. Deviations from the proposal

These are places where the implementation does not meet what the proposal requires.


### 1.4 (Medium) Store/exchange wakeups (NOTE 1) not implemented on the fallback path

**Proposal basis:** §7.17.7.7 NOTE 1 and §7.17.7.10 NOTE 1: "a wakeup is possibly triggered by
either a notifying operation or by an atomic store or exchange."

**Implementation:** on the hash-table fallback path the waiter parks on the *proxy* object,
not the user's atomic. A plain `atomic_store` on the user object never signals the proxy, so a
store alone cannot unblock a genuinely-sleeping fallback waiter; with 1.1/1.2 resolved there is
no busy-spin poll left to observe the store either. So the "wakeup triggered by a store"
behaviour is effectively absent on this path. (Treating NOTE 1 as advisory softens this to an
under-specification — see §2.)

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

## 4. Test coverage (updated after the test-suite review)

Still deliberately uncovered / noted (the behaviour is only advisory, or not deterministically
triggerable by a portable unit test):
- **Store-alone wake-up** (proposal NOTE 1 is advisory): a bare value store does **not** wake a
  fully-parked waiter on the non-Windows compute-and-wait backends, so this is not asserted as a
  standalone test.
- **Strict notify cap > 1** is not assertable on macOS (any `max > 1` degenerates to wake-all).
  Likewise, a genuinely-negative `atomic_wait_expected` return (test item 4 in
  `atomic_wait_expected_test.c`) is only produced on a backend synchronization failure that a
  portable unit test cannot deterministically trigger.

---

## 5. Summary

| Issue | Severity | Basis / Location |
|-------|----------|------------------|
| Store/exchange wakeups absent on fallback path | Medium | §7.17.7.7 / §7.17.7.10 NOTE 1 |
| CAS-success-with-no-waiters returns `1` | Low | §7.17.7.11 (under-specified) |

