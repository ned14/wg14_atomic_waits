# Combined Implementation Analysis — wg14_atomic_waits

The single source of truth is **`docs/proposal.md`** (proposed §7.17.7.7–7.17.7.11).

Findings below are categorised as:

- **§1 Deviations** — places where the implementation does not meet what the
  proposal requires, or where the implementation's behaviour is observably
  wrong. Severity is tagged (High/Medium/Low). Items marked **[verified]**
  were reproduced empirically on this machine against the macOS native
  (`build/`) and forced-pthreads (`build_macos/`, `ALWAYS_USE_PTHREADS_BACKEND=ON`)
  builds.
- **§2 Under-specified** — wording is loose; the implementation is defensible.
- **§3 Build / CI coverage gaps** — configurations that are documented but
  never exercised (or cannot be built).
- **§4 Test coverage** — still deliberately uncovered.

---

## 1. Deviations from the proposal





### 1.4 (Medium) Store/exchange wakeups (NOTE 1) not implemented on the fallback path

**Proposal basis:** §7.17.7.7 / §7.17.7.10 NOTE 1: "a wakeup is possibly triggered
by either a notifying operation or by an atomic store or exchange."

**Implementation:** on the hash-table fallback path the waiter parks on the
*proxy* object, not the user's atomic. A plain `atomic_store` on the user object
never signals the proxy, so a store alone cannot unblock a genuinely-sleeping
fallback waiter; with 1.1's re-check the store is not observed either. So the
"wakeup triggered by a store" behaviour is effectively absent on this path.
(Treating NOTE 1 as advisory softens this to an under-specification — see §2.)




### 1.10 (Low) `atomic_notify` with `max_threads_to_wake == 0` returns 0 without performing the exchange

**Proposal basis:** §7.17.7.11 Description says the CAS is performed and "unblocks
at least `max_threads_to_wake` threads" (trivially true for 0); Returns says a
successful exchange returns positive.

**Implementation:** `atomic_wait_common.ipp.ipp:764-767` short-circuits
`max_threads_to_wake == 0` with `return 0` *before* the CAS, leaving the value
unchanged and `*expected` untouched. The suite codifies this
(`atomic_notify_more_test.c:35-39` asserts `nr == 0` and value unchanged). A
literal reading would still exchange the value and return positive. Ambiguous
wording, but the no-op behaviour is a defensible reading ("notified" implies
waking someone) — flagging for the proposal to clarify.


### 1.13 (Low) errno hygiene: `atomic_notify` wake failure and `atomic_wait_generic` success path

**Proposal basis:** §7.17.7.11 Returns ("If unsuccessful, this function returns a
negative value"); §7.17.7.7/7.17.7.10 error return. The proposal does not mandate
errno, so this is a consistency concern, not a violation.

**Implementation:** on a genuine wake-by-address failure the Linux/FreeBSD/macOS
`wake_by_address32/64` restore `save_errno` before returning `-e`
(`atomic_wait_linux.c.ipp:116-122`, `atomic_wait_freebsd.c.ipp:156-161`,
`atomic_wait_macos.c.ipp:131-137`), so `atomic_notify_32` returns negative but
leaves the caller's `errno` unchanged — the caller cannot decode the returned
`-errno`. The *wait* paths do set `errno` on hard error
(`atomic_wait_common.ipp.ipp:613`, `:875`). So the error signal differs between
`atomic_notify` and `atomic_wait*`. Minor, since futex/ulock/wake failures on a
valid address are effectively unreachable.

Separately, `atomic_wait_generic`'s success path captures `save_errno` only *after*
the wait loop (line 622), i.e. after the internal `find_or_create` (which calls
`calloc`) may have altered errno, so a successful generic wait does not guarantee a
pristine errno. Cosmetic.

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
  `ret = 1` before the first *actual* kernel suspension (`atomic_wait_common.ipp.ipp:458`),
  so a spurious return before a real park (e.g. `wait_on_address` returns `-EAGAIN` because
  the value changed in the load-to-syscall window) can be reported positive. Loosely worded in
  the proposal; the intent-to-suspend reading is defensible.
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
  value-changing store more explicitly. §1.1 was the *value-changing* analogue on
  the fallback path; it is now closed (the proxy generation is captured under the
  hash-table lock before the re-check, see §1.1), so this inherent
  notify-before-park limitation is the only lost-wake case left and it applies
  uniformly across backends.
- **`atomic_notify` return when the exchange succeeds but only a sub-range of waiters is
  woken.** On macOS/Windows/pthreads any `max_threads_to_wake > 1` degenerates to wake-all
  (and the pthreads token pile is now capped at the waiter count, see §1.2), and the returned
  "1 + woken" count is fabricated
  (`atomic_wait_macos.c.ipp:130`, `atomic_wait_windows.c.ipp:185`). "At least
  `max_threads_to_wake`" makes this compliant; the exact count is implementation-defined.
- **macOS `__ulock_wait` treats a `timeout` of 0 as "wait indefinitely".**
  `wait_on_address32/64` pass `timeout_us = 0` for a `NULL` duration and only force
  `timeout_us = 1` for a non-NULL zero-duration (`atomic_wait_macos.c.ipp:59-69`), i.e. the
  code *implicitly assumes* `__ulock_wait`'s 0 means infinite. This is an undocumented
  kernel-contract assumption; it is confirmed by every passing macOS native test but would
  silently break (immediate-return busy-spin, or infinite hang) on an implementation that
  treats 0 as "expire immediately".
- **`uint_native_wait_notify_t` is hard-coded to `uint_least32_t` (4 bytes).** The proposal
  defines it as "the smallest `uint_leastN_t` with N ≥ 32 for which atomic waits have least
  overhead" and constrains `atomic_wait_expected`/`atomic_notify` to `sizeof == 4`
  (`atomic_wait.h:76-89`). On every supported platform `uint_least32_t` is 32-bit, but on a
  hypothetical platform where the least 32-bit type is wider, the header's blanket
  "`sizeof(...) == 4`" static_assert and native-dispatch macros would be wrong.

---

## 3. Build / CI coverage gaps

These configurations are documented (or fall out of the code) but are not
exercised by `.github/workflows/ci.yml`:

- **`ALWAYS_USE_PTHREADS_BACKEND=ON` on Windows/MSVC** — the CMake source/link
  condition picks `atomic_wait_pthreads.c` and links `pthread`
  (`CMakeLists.txt:30,34`), but MSVC has neither `<pthread.h>` nor a `pthread`
  library. The option is documented as "every platform" but cannot build there.
- **Windows x86 (32-bit)** — `HAVE_WAIT_ON_ADDRESS_64` is guarded by `_WIN64`
  (`atomic_wait_windows.c.ipp:142`), so the 8-byte hash-table fallback is only
  exercised on 32-bit Windows, which CI never builds (x64 only).
- **C23 semantics** — the public macros `atomic_wait`/`atomic_notify` etc. are
  defined bare in the header (`atomic_wait.h:265-384`). Once real C23
  `<stdatomic.h>` ships the same names, including this header alongside it will
  collide. The Readme already warns to use this header *instead of* C23
  `<stdatomic.h>`; the analysis doc records it as a forward-compat constraint.

---

## 4. Test coverage (updated after the implementation review)

Still deliberately uncovered / noted (the behaviour is only advisory, or not deterministically
triggerable by a portable unit test):

- **Store-alone wake-up** (proposal NOTE 1 is advisory): a bare value store does **not** wake a
  fully-parked waiter on the non-Windows compute-and-wait backends, so this is not asserted as a
  standalone test.
- **Strict notify cap > 1** is not assertable on macOS/Windows/pthreads (any `max > 1`
  degenerates to wake-all). Likewise, a genuinely-negative `atomic_wait_expected` return (test item 4 in
  `atomic_wait_expected_test.c`) is only produced on a backend synchronization failure that a
  portable unit test cannot deterministically trigger.
- **Newly identified, previously untested behaviours that §1 turned up:**
  - §1.2: now covered by `spurious_notify_all_busy_spin_test` in
    `atomic_wait_race_test.c`, which drives a *spurious* `atomic_notify_all`
    (value unchanged) on the pthreads backend and asserts the fallback waiter
    re-suspends (bounded CPU burn).

---

## 5. Summary

| # | Issue | Severity | Basis / Location |
|---|-------|----------|------------------|
| 1.4 | Store/exchange wakeups absent on fallback path | Medium | §7.17.7.7 / §7.17.7.10 NOTE 1 |
| 1.10 | `atomic_notify` max=0 returns 0 without performing the exchange | Low | §7.17.7.11; `atomic_wait_common.ipp.ipp:764-767` |
| 1.13 | errno hygiene: `atomic_notify` wake-failure returns negative with errno unchanged; generic success path may clobber errno | Low | `atomic_wait_linux.c.ipp:116-122`; `atomic_wait_common.ipp.ipp:622` |
| §2 | macOS `__ulock_wait` 0==infinite is an implicit contract; `uint_native_wait_notify_t` 4-byte hard-code assumption | Low (portability) | `atomic_wait_macos.c.ipp:59-69`; `atomic_wait.h:76-89` |
| §3 | pthreads-on-Windows / Win32-x86 never CI-tested (header-only+pthreads now covered) | Medium (risk) | `.github/workflows/ci.yml` |
