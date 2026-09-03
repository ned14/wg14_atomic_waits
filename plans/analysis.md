# Combined Implementation & Wording Analysis — wg14_atomic_waits

The single source of truth is **`docs/proposed-wording.md`** (proposed §7.17.7.7–7.17.7.11,
diffed against N3886, a copy of which lives in `reference/n3886.pdf`).

This file is a list of open action items.

- **[W#]** Proposed-wording items (2026-08-24 review against N3886). Items marked ✅
  have already been applied to `docs/proposed-wording.md`; unmarked items need an
  editorial/WG decision before the next draft.
- **§1** Implementation deviations from the proposal (severity tagged; **[verified]**
  items were reproduced on this machine against `build/` and
  `build_macos/` (`ALWAYS_USE_PTHREADS_BACKEND=ON`)).
- **§2** Under-specified behaviour (implementation defensible; wording loose).
- **§3** Build / CI coverage gaps.
- **§4** Test coverage gaps.

---

## W. Proposed-wording action items (review vs N3886)

### Style / tone / terminology fixes applied

- [x] **W1 ✅ Define waiting/notifying vocabulary once.** The old text used "notifying
  operation" and "waiting operation" in the NOTEs and the synchronisation paragraph and
  the glob spellings `atomic_notify*(object)` / `atomic_wait_*()` in normative text,
  none defined anywhere in N3886. Added definitions of *waiting operation*,
  *notifying operation*, *blocked on M*, *woken up*, and *wakeup* as an appended
  paragraph to 7.17.7.1 General; removed all glob spellings from normative text.
- [x] **W2 ✅ Abstract machine.** The synchronisation paragraph said side effects
  "synchronize with the waiting thread before the waiting operation returns".
  Per 5.2.2.5, "synchronizes with" relates *evaluations/operations*, not threads, and
  "before ... returns" is not an abstract-machine notion. Rewritten (both 7.17.7.7 and
  7.17.7.10) to: "All side effects to the atomic object M that happen before a call N to
  a notifying operation on M synchronize with the completion of any waiting operation W
  on M that is unblocked by N." — using the happens-before/synchronizes-with machinery
  of 5.2.2.5 directly. This is the one place where the proposed wording needed
  rewriting in terms of the abstract machine; the remaining physical notions
  ("blocked", timing) follow the precedent of the 7.30 threads clause (cnd_wait,
  cnd_timedwait), which also words blocking physically.
- [x] **W3 ✅ suspended → blocked.** N3886's threads clause consistently uses
  "blocks"/"unblocks"/"is woken up"; "suspended" appears nowhere relevant. Replaced
  throughout 7.17.7.7–7.17.7.11.
- [x] **W4 ✅ NOTE style.** "**NOTE 1:** Note that ..." → "NOTE 1 A wakeup can be
  triggered by ..." (no colon, no leading "Note that", "can" instead of "possibly"),
  matching e.g. 5.2.2.5 NOTE 2 and 7.17.3.1 NOTE 1.
- [x] **W5 ✅ shall/must.** "`sizeof(C)` must equal ..., or this generic function shall
  be undefined" is not C-standard idiom (the standard never uses "must" for
  requirements, and constraint violations are UB via 4p2 without restating it).
  Reworded as a Requires-style first paragraph: "The sizeof(C) shall equal the
  sizeof(uint_native_wait_notify_t)." (cf. "The order argument shall not be ..."
  in 7.17.7.2).
- [x] **W6 ✅ Returns phrasing.** "the memory synchronization ordering will be failure"
  → "memory is affected according to the value of *failure*", matching every Returns /
  Description in 7.17.7.2–7.17.7.6.
- [x] **W7 ✅ Removed non-standard hedging.** Deleted "(possibly one plus the number of
  threads woken on some platforms)" — "on some platforms" is not standards vocabulary;
  kept "possibly one plus the number of threads woken". Deleted "synchronizing as per
  the description of atomic_wait()" in favour of "synchronizing as described in
  7.17.7.7". Fixed "--" em-dash and was/will-be tense mixing in the Returns paragraphs.
- [x] **W8 ✅ Introduction p5 grammar.** "which is the smallest uint_leastN_t types where
  N is at least thirty-two for which ..." (number–noun disagreement, double "for",
  spelled-out number) → "which is the uint_leastN_t type with the smallest *N*, where
  *N* is at least 32, for which atomic waiting and notifying operations have the least
  overhead on this implementation", inserted into the p5 type list with the list
  punctuation N3886 uses (semicolon-separated clauses ending "; and several atomic
  analogs of integer types.").
- [x] **W9 ✅ 7.17.6 edit target.** The append is now explicitly a row appended to
  Table 7.6 (that is what 7.17.6p1 enumerates), not prose appended to the subclause.
- [x] **W10 ✅ memcmp reference.** "as if by a call to `memcmp()`" → "as if by the
  memcmp function", matching how N3886 refers to library functions in prose.

### Open wording questions (need decision before forwarding)

- [ ] **W11 Which memory_order governs the initial check in `atomic_wait_expected`?**
  The proposal never says what order the pre-block load uses (presumably *success*,
  with *failure* only on timeout/mismatch exit — see Returns). An implementation cannot
  be written faithfully without deciding. Either state it ("as if by
  atomic_load_explicit(object, success)") or state that it is unspecified which load
  ordering is used for the checks.
- [ ] **W12 Order restrictions not stated.** `atomic_wait_explicit`'s *order* should
  inherit the load restriction (shall not be memory_order_release nor
  memory_order_acq_rel), and *success*/*failure* in `atomic_wait_expected` and
  `atomic_notify` should inherit the compare-exchange restrictions (failure not release
  nor acq_rel; failure not stronger than success). They are currently inherited only
  implicitly through the as-if formulations. Decide whether to restate them explicitly
  (recommended for atomic_wait_explicit, which has no as-if CAS to lean on).
- [ ] **W13 Negative returns are unspecified.** "If the request could not be honored"
  is borrowed from cnd_signal/cnd_broadcast, but nothing defines *when* the request
  cannot be honored (resource exhaustion? signal interruption? no thread support?). At
  minimum decide: may these functions return negative because of signals (EINTR-like)?
  Must they return negative rather than block forever if wait state cannot be allocated?
- [ ] **W14 Timeout clock basis unspecified.** "Total accumulated time ... shall be at
  least \*duration" makes *duration* a relative interval, but never names a clock.
  cnd_timedwait uses "TIME_UTC-based calendar time"; thrd_sleep uses TIME_UTC. The
  header documents a monotonic relative interval. Pick one (recommend: measured against
  the system-wide monotonic clock, or align with TIME_MONOTONIC via timespec_get) and
  confirm that "total accumulated time across all re-blockings" (not per-blocking) is
  the intended semantics.
- [ ] **W15 Applicable types unstated.** Nothing scopes the new generic functions to a
  set of A/C pairs. 7.17.7.1 covers "all atomic types other than atomic_flag", which
  would include atomic_bool and lock-free-free atomics; the sizeof constraint then
  excludes everything except uint_native_wait_notify_t-sized types for 7.17.7.10/11,
  but plain atomic_wait remains unconstrained. Confirm intent (e.g. waiting permitted
  on any atomic object, including non-lock-free ones?) and consider saying so in the
  7.17.7.1 General addition.
- [ ] **W16 Threads support prerequisite.** 5.2.2.5 makes threads optional
  (__STDC_NO_THREADS__; freestanding). Waiting/blocking presumes multiple threads;
  state the behaviour when the execution environment does not support threads
  (negative return? undefined?). Also decide freestanding status of the new functions.
- [ ] **W17 Positive return values unconstrained.** The positive return of
  `atomic_wait_expected` and the count returned by `atomic_notify` have no stated range
  or meaning beyond sign. Fine, but consider "returns an implementation-defined positive
  value" to stop readers inferring a documented encoding.
- [ ] **W18 `atomic_notify` with max_threads_to_wake == 0.** As worded, a successful
  exchange still occurs and the function returns positive even though it trivially
  "unblocks at least 0" waiters; the implementation short-circuits before the CAS and
  returns 0 (see §1.10). Either change the code or add wording ("If
  max_threads_to_wake is zero, the function performs no exchange and returns zero" or
  similar).
- [ ] **W19 restrict qualifiers.** The synopses for 7.17.7.10/7.17.7.11 use restrict;
  no other synopsis in 7.17 does (though 7.30.3.5 etc. do). Confirm this is deliberate
  API design and keep, or drop for consistency within the clause.
- [ ] **W20 memcmp-equality caveat worth a NOTE?** Comparison "as if by memcmp"
  compares representations: padding bits and, for the _expected_ form, any mismatch
  between value equality and representation equality will surprise. Consider a NOTE
  pointing out that two values equal under == need not compare equal here (and vice
  versa), mirroring 7.17.7.5 NOTE 1's explicit memcmp example.
- [ ] **W21 Waiter lifetime / address reuse.** Implementations key waiter state on the
  object address; nothing addresses destroying or reallocating an object while threads
  are blocked on it (cf. mtx_destroy's undefined-behaviour sentence). Consider a
  requirement or NOTE.
- [ ] **W22 Optional polish:** note that while blocked the thread performs no evaluations
  and blocking is observable only through timing, tying the physical blocking language
  back to the abstract machine more tightly.

---

## 1. Deviations from the proposal

### 1.4 (Medium) Store/exchange wakeups (NOTE 1) not implemented on the fallback path

**Proposal basis:** §7.17.7.7 / §7.17.7.10 NOTE 1: "a wakeup is possibly triggered by
either a notifying operation or by an atomic store or exchange."

**Implementation:** on the hash-table fallback path the waiter parks on the *proxy*
object, not the user's atomic. A plain `atomic_store` on the user object never signals
the proxy, so a store alone cannot unblock a genuinely-sleeping fallback waiter; with
1.1's re-check the store is not observed either. So the "wakeup triggered by a store"
behaviour is effectively absent on this path. (Treating NOTE 1 as advisory softens this
to an under-specification — see §2.)

→ **Action item:** either implement store-triggered wakeups on the fallback path or
record that NOTE 1 (now "A wakeup **can** be triggered by ...") is advisory and close.

### 1.10 (Low) `atomic_notify` with `max_threads_to_wake == 0` returns 0 without performing the exchange

**Proposal basis:** §7.17.7.11 Description says the CAS is performed and "unblocks at
least `max_threads_to_wake` threads" (trivially true for 0); Returns says a successful
exchange returns positive.

**Implementation:** `atomic_wait_common.ipp.ipp:764-767` short-circuits
`max_threads_to_wake == 0` with `return 0` *before* the CAS, leaving the value unchanged
and `*expected` untouched. The suite codifies this (`atomic_notify_more_test.c:35-39`
asserts `nr == 0` and value unchanged). A literal reading would still exchange the value
and return positive. Ambiguous wording, but the no-op behaviour is a defensible reading
("notified" implies waking someone).

→ **Action item:** tracked as W18 above; resolve wording and code together.

---

## 2. Where the proposal is under-specified (implementation is defensible but the wording is loose)

These are cases where the implementation deviates from a *literal* reading or where the
proposal leaves behaviour open; the flag here is "document for the reader", not "fix the
code".

- **`atomic_notify` return when CAS succeeds but no waiters are parked.** Now worded
  "possibly one plus the number of threads woken", which permits the implementation's
  `1 + ret`. Closed by W7 unless WG wants an explicit statement (fold into W17).
- **`atomic_wait_expected` positive result when only "intent to park" occurred.**
  §7.17.7.10 ties positive to "was blocked at least once". `atomic_wait_generic` pre-sets
  `ret = 1` before the first *actual* kernel suspension
  (`atomic_wait_common.ipp.ipp:458`), so a spurious return before a real park can be
  reported positive. Loosely worded; the intent-to-suspend reading is defensible.
- **`atomic_notify` under-reports the woken count when no proxy node is registered.**
  `atomic_notify_generic` returns `0` when `hash_table_find_or_create(..., false)` finds
  nothing. The wording only requires "at least max_threads_to_wake … unblocked" after a
  successful exchange, so this is consistent.
- **Store-triggered wakeups and the lost-wake-before-park (native) case.** §7.17.7.8's
  "prior to the call" phrasing means a notify that precedes a waiter's park is not
  guaranteed to wake it. On the native fast path a waiter that loads
  `*object == expected`, is preempted while a value-unchanging notify fires, then parks,
  can block with no further notify. Inherent futex-style limitation; §1.1's value-changing
  analogue is closed (proxy generation captured under the hash-table lock before the
  re-check). Consider a NOTE (fold into W21/W22).
- **`atomic_notify` return when the exchange succeeds but only a sub-range of waiters is
  woken.** On macOS/Windows/pthreads any `max_threads_to_wake > 1` degenerates to
  wake-all, and the returned "1 + woken" count is fabricated
  (`atomic_wait_macos.c.ipp:130`, `atomic_wait_windows.c.ipp:185`). Compliant per the
  wording; exact count is implementation-defined.
- **macOS `__ulock_wait` treats a `timeout` of 0 as "wait indefinitely".**
  `wait_on_address32/64` pass `timeout_us = 0` for a `NULL` duration and only force
  `timeout_us = 1` for a non-NULL zero-duration (`atomic_wait_macos.c.ipp:59-69`).
  Undocumented kernel-contract assumption; confirmed by tests but silently breaks on a
  kernel that treats 0 as "expire immediately".
- **`uint_native_wait_notify_t` is hard-coded to `uint_least32_t` (4 bytes).** The
  definition ("smallest uint_leastN_t with N ≥ 32 for which waits have least overhead")
  plus the blanket `sizeof(...) == 4` static_assert and native-dispatch macros
  (`atomic_wait.h:76-89`) coincide on every supported platform, but diverge on a
  hypothetical platform where the least 32-bit type is wider.

---

## 3. Build / CI coverage gaps

- **`ALWAYS_USE_PTHREADS_BACKEND=ON` on Windows/MSVC** — the CMake source/link condition
  picks `atomic_wait_pthreads.c` and links `pthread` (`CMakeLists.txt:30,34`), but MSVC
  has neither `<pthread.h>` nor a `pthread` library. The option is documented as "every
  platform" but cannot build there.
- **Windows x86 (32-bit)** — `HAVE_WAIT_ON_ADDRESS_64` is guarded by `_WIN64`
  (`atomic_wait_windows.c.ipp:142`), so the 8-byte hash-table fallback is only exercised
  on 32-bit Windows, which CI never builds (x64 only).
- **C23 semantics** — the public macros `atomic_wait`/`atomic_notify` etc. are defined
  bare in the header (`atomic_wait.h:265-384`). Once real C23 `<stdatomic.h>` ships the
  same names, including this header alongside it will collide. The Readme warns to use
  this header *instead of* C23 `<stdatomic.h>`; record as a forward-compat constraint
  (and note for W15: real C23 may eventually standardise the same generic function
  names differently).

---

## 4. Test coverage (updated after the implementation review)

Still deliberately uncovered / noted (advisory behaviour, or not deterministically
triggerable by a portable unit test):

- **Store-alone wake-up** (proposal NOTE 1 is advisory): a bare value store does **not**
  wake a fully-parked waiter on the non-Windows compute-and-wait backends (see §1.4).
- **Strict notify cap > 1** is not assertable on macOS/Windows/pthreads (any `max > 1`
  degenerates to wake-all). Likewise, a genuinely-negative `atomic_wait_expected` return
  is only produced on a backend synchronization failure that a portable unit test cannot
  deterministically trigger.
- **§1.2** is covered by `spurious_notify_all_busy_spin_test` in `atomic_wait_race_test.c`,
  which drives a *spurious* `atomic_notify_all` (value unchanged) on the pthreads backend
  and asserts the fallback waiter re-suspends (bounded CPU burn).

---

## 5. Summary of open action items

| # | Item | Severity | Where |
|---|------|----------|-------|
| W11 | Initial load order in `atomic_wait_expected` unspecified | High (implementability) | 7.17.7.10 |
| W12 | Order restrictions not restated (load orders, success/failure constraints) | Medium | 7.17.7.7/10/11 |
| W13 | Negative-return conditions unspecified | High (implementability) | 7.17.7.10/11 Returns |
| W14 | Timeout clock basis and accumulation semantics unspecified | High (implementability) | 7.17.7.10 |
| W15 | Applicable A/C types unstated | Medium | 7.17.7.1 General |
| W16 | Behaviour without thread support unspecified | Medium | General |
| W18 | `atomic_notify(max=0)` wording vs code mismatch | Low | 7.17.7.11; `atomic_wait_common.ipp.ipp:764-767` |
| W19 | restrict qualifiers deviate from clause style | Low | 7.17.7.10/11 |
| W20–W22 | memcmp NOTE, waiter-lifetime NOTE, abstract-machine polish | Low | various |
| 1.4 | Store/exchange wakeups absent on fallback path | Medium | NOTE 1 |
| §2 | macOS `__ulock_wait` 0==infinite implicit contract; 4-byte hard-code assumption | Low (portability) | `atomic_wait_macos.c.ipp:59-69`; `atomic_wait.h:76-89` |
| §3 | pthreads-on-Windows / Win32-x86 never CI-tested (header-only+pthreads now covered) | Medium (risk) | `.github/workflows/ci.yml` |
