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


### 1.2 (High) Pthreads backend: spurious `atomic_notify_all` busy-spins through `INT_MAX` pending tokens instead of re-suspending **[verified]**

**Proposal basis:** §7.17.7.7 "if when woken up the value still compares equal to
`expected` the thread is suspended again" — the re-suspension must actually
suspend, not burn CPU.

**Implementation:** `pthread_proxy_wake` (`atomic_wait_pthreads.c.ipp:213-215`)
sets `p->pending = INT_MAX` for a wake-all (`max_threads_to_wake == -1`).
`pthread_proxy_wait` (`atomic_wait_pthreads.c.ipp:136-141`) consumes a token and
returns *immediately* whenever `pending > 0`. A waiter woken by a spurious
`atomic_notify_all` (value unchanged) therefore re-checks, re-parks, consumes the
next token, … — a tight spin of up to 2³¹-1 iterations (measured ~50 M token
consumptions/s, i.e. tens of seconds to a couple of minutes of one core) before
it ever blocks again. It makes progress and will exit the moment the value
changes, so it is a livelock/perf bug, not a hang, but a single spurious
`atomic_notify_all` pegs a core for a long time.

Reproduced on the forced-pthreads macOS build: a parked 8-bit waiter given one
spurious `atomic_notify_all` with the value unchanged burned **0.302 s CPU in a
300 ms window** (~100 % of a core). The same probe against the native build
burned 0.000 s (the generation-counter proxy bumps by one per notify and the
waiter re-parks on the new generation — bounded).

The unbounded `pending` pile is unique to the pthreads proxy: the default proxy
bumps a monotonic counter (bounded), and single-token notifies are bounded. Only
the `INT_MAX` wake-all token dump is pathological. Cap `pending` at the number of
waiters (or use a generation counter like the default proxy).

### 1.3 (Medium) Hash-table fallback: timed-out / errored `atomic_wait_expected` leaks its proxy node **[verified]**

**Proposal basis:** none (resource hygiene), but §7.17.7.10 requires the function
to return *and* `*expected` to be usable on timeout; the leaked node makes every
subsequent wait/notify on that address pay a spurious stale-node lookup.

**Implementation:** `atomic_wait_generic` increments `use_count` when it creates
the proxy (`atomic_wait_common.ipp.ipp:431-459`) and decrements/removes it only
on the success `break` path (`atomic_wait_common.ipp.ipp:492-505`). The two early
returns bypass the cleanup:

- duration timeout: `atomic_wait_common.ipp.ipp:466-470` (`errno = ETIME; return 0;`), and
- hard wait error: `atomic_wait_common.ipp.ipp:485-490` (`errno = -ret2; return -1;`).

Each such exit leaves the proxy in the table with `use_count == 1` forever.
Reproduced on the forced-pthreads build: **50,000 timed-out `atomic_wait_expected`
calls leaked ~11 MB (≈227 B per call — proxy + mutex/condvar + bucket)**; the
native 32-bit path (no hash table) leaked nothing measurable (4 B/leak allocator
noise). The suite's own "full-duration timeout", "zero timeout", and "negative
duration" sections all leak one node per call on the pthreads backend; they pass
because the process exits. Fix: decrement/remove on all exits, or move the
cleanup to a common tail.

### 1.4 (Medium) Store/exchange wakeups (NOTE 1) not implemented on the fallback path

**Proposal basis:** §7.17.7.7 / §7.17.7.10 NOTE 1: "a wakeup is possibly triggered
by either a notifying operation or by an atomic store or exchange."

**Implementation:** on the hash-table fallback path the waiter parks on the
*proxy* object, not the user's atomic. A plain `atomic_store` on the user object
never signals the proxy, so a store alone cannot unblock a genuinely-sleeping
fallback waiter; with 1.1's re-check the store is not observed either. So the
"wakeup triggered by a store" behaviour is effectively absent on this path.
(Treating NOTE 1 as advisory softens this to an under-specification — see §2.)


### 1.7 (Low) Native and generic paths disagree about which load's value is returned in `*expected`

**Proposal basis:** §7.17.7.10 Returns: "In all cases, `*expected` on return was
the value of `*object` when most recently loaded."

**Implementation:** when the wait exits with a positive (suspended) result and
`success > failure`, the native path (`atomic_wait_common.ipp.ipp:697-715`)
updates `*expected` from the *failure-ordered* load and **discards** the
success-ordered reload (comment at `:709-713`), while the generic path
(`:418-428`) copies the success-ordered load into `*expected`. In the narrow race
where the object changes between the two loads, the two paths therefore return
different values, and the native path returns a value that is *not* the most
recently loaded. The native choice (return "the value which caused the wait to
exit") is deliberate and defensible, but the two paths should agree; the proposal
would benefit from stating which value is authoritative.

### 1.8 (Low) `success > failure` relies on the numeric `memory_order` encoding

**Proposal basis:** §7.17.7.10 requires the final load to use the success
ordering "when the failure ordering already provides at least as much
load-ordering" is *not* the case.

**Implementation:** `atomic_wait_common.ipp.ipp:418` and `:702` compare the two
order enums with `>`. The C standard does not fix the enum values; this only
works because clang/gcc/MSVC all encode `relaxed < consume < acquire < release <
acq_rel < seq_cst` as 0..5. Portable code should compare orderings explicitly
(e.g. a small helper), and passing `release`/`acq_rel` as `success` (UB for a
load anyway) would be mis-ordered by the numeric test in a C++ compile where
`std::memory_order` could theoretically differ.

### 1.9 (Low) `monotonic_now` ignores `clock_gettime` failure on non-Windows platforms lacking `CLOCK_MONOTONIC`

**Proposal basis:** timeout correctness.

**Implementation:** `atomic_wait_common.ipp.ipp:379` calls
`clock_gettime(CLOCK_MONOTONIC, ts)` and ignores the return value; on a POSIX
platform without `CLOCK_MONOTONIC` the timespec is garbage and every timeout
breaks. The pthreads backend correctly guards its own clock choice
(`atomic_wait_pthreads.c.ipp:50-54`) but the common code does not. Academic on
POSIX.1-2008 systems, but the backend already handles the case inconsistently.

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

### 1.11 (Low) Quadratic probing on a power-of-two table only reaches half the slots; `hash_table_grow` silently drops items if a rehash probe fails

**Proposal basis:** n/a (robustness).

**Implementation:** `(h + step*step) & mask` (`atomic_wait_common.ipp.ipp:221`,
`:179`) with a power-of-two table visits only slots ≡ h or h+1 (mod 4), i.e. half
the table; growth therefore triggers at ~50 % load instead of ~90 % (correct, just
memory-hungrier). In `hash_table_grow` (`:177-187`) the rehash loop has no
"slot not found" fallback — if a full probe fails the item is silently dropped,
which would strand a waiter's proxy and lose its notify. Unreachable in
practice (growth is triggered well below saturation) but it is a silent-loss
hazard with no diagnostic.

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
  (or `INT_MAX` tokens), and the returned "1 + woken" count is fabricated
  (`atomic_wait_macos.c.ipp:130`, `atomic_wait_windows.c.ipp:185`). "At least
  `max_threads_to_wake`" makes this compliant; the exact count is implementation-defined.

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
- **Header-only mode is not CI-tested in combination with
  `ALWAYS_USE_PTHREADS_BACKEND`** (the pthreads proxy's `pthread_mutex_t`/`cond`
  state is embedded in `proxy_waiter_t`; a weak `shared_global_hash_table`
  coalesced across TUs is the only cross-TU sharing — fine, but untested).
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
  - §1.2: no test drives a *spurious* `atomic_notify_all` (value unchanged) on the pthreads
    backend, which is the trigger for the `INT_MAX`-token busy-spin. `atomic_wait_race_test.c`
    uses `atomic_notify_one` (bounded) on the fallback widths only.
  - §1.3: the timeout/error proxy leak is invisible to the suite (process exits); a long-running
    process doing many timed-out waits on distinct addresses on the pthreads backend grows
    memory without bound.

---

## 5. Summary

| # | Issue | Severity | Basis / Location |
|---|-------|----------|------------------|
| 1.2 | Pthreads: spurious `atomic_notify_all` busy-spins through `INT_MAX` pending tokens (tens of s–minutes/core) **[verified]** | High | §7.17.7.7 re-suspend; `atomic_wait_pthreads.c.ipp:213-215,136-141` |
| 1.3 | Fallback: timed-out/errored `atomic_wait_expected` leaks proxy nodes (~227 B/call) **[verified]** | Medium | `atomic_wait_common.ipp.ipp:466-470,485-490` |
| 1.4 | Store/exchange wakeups absent on fallback path | Medium | §7.17.7.7 / §7.17.7.10 NOTE 1 |
| 1.7 | Native vs generic `*expected` returned from different loads (success>failure) | Low | §7.17.7.10 Returns; `atomic_wait_common.ipp.ipp:697-715` vs `:418-428` |
| 1.8 | `success > failure` numeric enum comparison is non-portable | Low | `atomic_wait_common.ipp.ipp:418,702` |
| 1.9 | `monotonic_now` ignores `clock_gettime` failure where `CLOCK_MONOTONIC` missing | Low | `atomic_wait_common.ipp.ipp:379` |
| 1.10 | `atomic_notify` max=0 returns 0 without performing the exchange | Low | §7.17.7.11; `atomic_wait_common.ipp.ipp:764-767` |
| 1.11 | Quadratic probing reaches half the table; grow can silently drop items | Low | `atomic_wait_common.ipp.ipp:177-187,221` |
| §3 | pthreads-on-Windows / Win32-x86 never CI-tested | Medium (risk) | `.github/workflows/ci.yml` |
