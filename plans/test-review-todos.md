# Test Suite Review: Exposure Tests for `plans/combined-analysis.md`

Companion to `plans/combined-analysis.md`. For **every** item in that document
(§1 deviations 1.1–1.11, §2 under-specified wording, §3 CI coverage gaps) this
file states whether a test can be written to expose the issue, and gives the
full test design (file, mechanism, assertions, gating, runtime, expected status
on today's code, caveats).

Verdicts in the mapping table:

| Item | Verdict | Test ref |
|---|---|---|
| 1.1 Fallback-path lost-wake deadlock | Testable **only with a test hook**; flaky without it | A1.1 |
| 1.2 Pthreads `INT_MAX` busy-spin | **Testable, deterministic** (CPU measurement) | A1.2 |
| 1.3 Proxy-node leak on timeout/error | **Testable, deterministic, fast** (negative-duration + RSS) | A1.3 |
| 1.4 Store/exchange wakeups absent on fallback | Not testable portably (advisory; backend-dependent) | — |
| 1.5 FreeBSD backend cannot build | **Testable** (stub-header compile test, runs on any OS) | A1.5 |
| 1.6 FreeBSD wake returns 0 on error | **Testable** (white-box mock of `_umtx_op`) | A1.6 |
| 1.7 Native vs generic `*expected` divergence | Not testable (needs an intra-function race) | — |
| 1.8 `success > failure` numeric enum compare | Not testable on supported toolchains | — |
| 1.9 `monotonic_now` ignores `clock_gettime` failure | Not testable portably (fix first) | — |
| 1.10 `atomic_notify(max=0)` no-op | **Testable**, but expectation encodes ambiguous wording | A1.10 |
| 1.11 Quadratic probing / grow item drop | Not testable practically (internal; collisions) | — |
| §2 under-specified bullets | No exposure tests (characterization only) | D |
| §3 CI gaps | Not unit tests — CI additions (one doubles as A1.5) | E |

---

## A. Deterministically testable — implement these

### A1.1 (item 1.1) Lost-wake deadlock on the hash-table fallback path

**Verdict:** a public-API test can only expose this probabilistically (the
window is the ~10 ns between `hash_table_unlock` at
`atomic_wait_common.ipp.ipp:457` and the proxy-generation load inside
`PROXY_WAIT` at `:481`). A deterministic test requires a tiny test-only hook.

**Test A — stress (no library change, flaky):** `test/lost_wake_stress2.c`

- Uses an `atomic_uint_least8_t` (fallback width on every POSIX backend).
- For K iterations (e.g. 200):
  1. `value = 0`; spawn waiter that sets `parked = 1` then
     `atomic_wait(&value, 0)`.
  2. Main: `test_wait_until(parked)`; then **exactly one** `atomic_store(value, 1)`
     and **exactly one** `atomic_notify_one(&value)` — no retry, no `notify_all`.
  3. Bounded join: spin on a `returned` flag (set by the waiter after
     `atomic_wait` returns) up to ~2 s. If it has not returned, record a failure,
     then issue a `notify_all` **as cleanup** (so the process can exit and the
     test reports a clean failure instead of hanging), then join.
  4. `CHECK(waiter returned within the deadline)` — i.e. one store+notify pair
     sufficed.
- Control: repeat the same loop on an `atomic_uint_native_wait_notify_t`
  (native 4-byte path) object — this must never fail.
- Expected status: passes on the native control and on the pthreads backend;
  occasionally fails on fallback widths (rare race, so the failure rate is low
  and the test is flaky-to-pass). That flakiness is exactly why the hook-based
  test below is preferred.

**Test B — deterministic (requires a one-line test hook):**
`test/lost_wake_hook_test.c` plus a guarded hook in
`atomic_wait_common.ipp.ipp` between lines 457 and 481:

```c
#ifdef WG14_ATOMIC_WAITS_TEST_WIDEN_PARK_WINDOW
  WG14_ATOMIC_WAITS_PREFIX(test_park_window_hook)();
#endif
```

- Declare a weak no-op default so production builds are unaffected.
- The test TU includes the backend `.ipp` directly (header-only include) with
  `-DWG14_ATOMIC_WAITS_TEST_WIDEN_PARK_WINDOW` and defines the hook to:
  1. `atomic_store(&in_window, 1)`; 2. spin until `atomic_load(&release) == 1`.
- Main thread: wait for `in_window` (the waiter is now provably *between* its
  locked re-check and its proxy-generation load), then perform **one**
  store+notify, then set `release`, then bounded-join with the same 2 s deadline
  and `notify_all` cleanup as in Test A.
- With the hook holding the window open, the notify's generation bump provably
  lands before the waiter's generation load, so the current implementation parks
  the waiter on the post-bump generation and it does **not** return until the
  cleanup `notify_all` → the deadline `CHECK` fails 100 % of the time. After the
  fix it returns immediately.
- Gating: the hook test must only drive the **fallback** width; the native 4-byte
  object is immune (futex word == object) and would falsely pass.
- Note: this is the strongest exposure test in this set — add the hook.

### A1.2 (item 1.2) Pthreads spurious `atomic_notify_all` busy-spin

**Verdict:** testable deterministically; no library change needed.

**Test:** `test/spurious_notify_all_spin_test.c`

- An `atomic_uint_least8_t` `value`; waiter sets `parked = 1`, records its thread
  CPU (`cpu_seconds()`, copy the Apple/Linux helper from
  `atomic_wait_race_test.c:26-51`, returns −1 on unsupported platforms), then
  `atomic_wait(&value, 0)`.
- Main: `test_wait_until(parked)`, then for ≈250 ms repeatedly:
  `thrd_sleep_ms(1); atomic_notify_all(&value);` with the value **unchanged**.
  (The notify-all loop both guarantees the notify lands once the proxy exists and
  defines the measurement window; the sleeps are not used for synchronisation.)
- Release: `atomic_store(value, 1); atomic_notify_all(&value);` join.
- Assertions:
  - waiter `returned == 1`;
  - if `cpu_seconds()` is supported: waiter CPU burned over the window
    `< 0.05 s`.
- Expected status on today's code: **fails** on the forced-pthreads build
  (≈0.25 s CPU in the 250 ms window — measured 0.302 s in 300 ms); **passes** on
  the native build (the generation-counter proxy gives ~one wake/re-park per
  notify, ≈ms of CPU for 250 wakeups).
- Caveats: the 0.05 s threshold separates the two cleanly (~0.25 s vs ~0.4 ms);
  re-verify under ASan/TSan (both inflate spin CPU, keeping the separation).

### A1.3 (item 1.3) Proxy-node leak on timed-out / errored `atomic_wait_expected`

**Verdict:** testable deterministically and *fast* — no sleeping required.

**Key insight:** on the generic path a **negative** duration (`tv_sec = −1`) makes
`atomic_wait_generic` create the proxy (`atomic_wait_common.ipp.ipp:434`) and then
return immediately at the duration check (`:466-470`) with no decrement — one
leaked node per call, each call effectively instant. The native 32-bit path has
no hash table, so it leaks nothing.

**Test:** `test/wait_expected_leak_test.c`

- Allocate an array of N = 50,000 distinct `atomic_uint_least32_t` objects.
- RSS helper with three platform implementations (return 0 / skip the assertion
  if unavailable): macOS `mach_task_basic_info.resident_size`; Linux read
  `/proc/self/statm`; Windows `GetProcessMemoryInfo`.
- Loop: `struct timespec d = {.tv_sec = -1, .tv_nsec = 0};`
  `atomic_wait_expected(&objs[i], &e, &d, seq_cst, seq_cst);` for each object.
- Assert `(rss_after - rss_before) < 100 bytes × N` (< 5 MB for 50k).
- Expected status on today's code: **fails** on the forced-pthreads build
  (measured ≈227 B/call + ~32 B/addr of legitimate retained table → ≈13 MB);
  **passes** on the native 32-bit path (no table at all → ≈0).
- Runtime: <100 ms (no waits, just hash-table ops).
- Caveats: the legitimate retained bucket table is ~16 B/bucket × 2N buckets
  ≈ 32 B/addr; the 100 B/addr threshold sits above that and well below the
  ~260 B/addr leaky total; ASan redzones inflate the leak signal, improving the
  margin. If the fix lands, re-run to confirm ≈32 B/addr.


### A1.10 (item 1.10) `atomic_notify` with `max_threads_to_wake == 0`

**Verdict:** testable, but the expectation encodes an *interpretation* of
ambiguous proposal wording, and the current suite already encodes the opposite
interpretation (`atomic_notify_more_test.c:35-39` asserts the no-op). Do **not**
add both variants.

**Test:** `test/atomic_notify_zero_max_test.c` (only after a WG14 decision)

- Variant A — literal reading (§7.17.7.11 Description: the CAS is performed,
  "unblocks at least `max_threads_to_wake`" is trivially true for 0): `value = 0,
  expected = 0, desired = 5, max = 0` → assert `value == 5` and return `> 0`.
  This **fails** on today's code (returns 0, value unchanged) and **contradicts**
  the existing suite assertion.
- Variant B — current behaviour: assert no-op (`return 0`, value unchanged) —
  already covered; would need deleting from `atomic_notify_more_test.c` if the
  proposal adopts Variant A.
- Action: flag the wording to the proposal, pick one variant, and keep the two
  files consistent.

---

## B. Testable only with instrumentation / probabilistically

- **1.7 (native vs generic `*expected`).** Both paths return different values
  only when the object changes between the failure-ordered load and the
  success-ordered reload *inside* `atomic_wait_expected` — an intra-function race
  no public-API test can force. A hook between the two loads (like A1.1's) could
  expose it, but the right fix is making the two paths agree; no test worth the
  hook.
- **1.1 without the hook.** Stress test A1.1 above; low per-iteration hit rate,
  so it is flaky-to-pass. Prefer the hook test.

---

## C. Not testable — document, don't chase

- **1.4 Store/exchange wakeups on the fallback path.** A bare store waking a
  parked waiter is backend-dependent (Windows `WaitOnAddress` wakes on a write;
  the POSIX futex/ulock paths do not), NOTE 1 is advisory, and the proposal
  imposes no "store must not wake" nor "store must wake" requirement. A portable
  positive or negative assertion is impossible. Existing B4 guidance below
  stands: make no store-wake assertion.
- **1.8 `success > failure`.** The numeric `memory_order` encoding is identical
  across clang/gcc/MSVC (relaxed=0 … seq_cst=5), so on every supported toolchain
  the comparison is correct and no observable divergence exists to assert. A
  portability hazard only; fix in code (order helper), no test.
- **1.9 `monotonic_now` ignoring `clock_gettime` failure.** Requires a POSIX
  platform without `CLOCK_MONOTONIC`. A Linux `-Wl,--wrap=clock_gettime` trick
  could force the failure, but the code currently has *no defined* failure
  behaviour to assert against. Fix the code first (propagate an error / fall
  back), then add a test.
- **1.11 Quadratic probing / `hash_table_grow` silent drop.** The 50%-reachable
  probe sequence and the rehash drop are internal to the hash table and only
  bite under pathological collisions, which cannot be forced portably (the FNV
  hash keys on the low 32 bits of real pointers). Fix in code (fallback to a
  second probe strategy / assert on failed rehash), no test.

---

## D. §2 under-specified items — characterization only, no exposure tests

- **`atomic_notify` positive with no waiters parked.** Defensible either way;
  the suite already covers CAS-success-with-a-waiter. Optionally add a one-line
  characterization assert (`atomic_notify` on an unwaited object returns > 0) —
  it documents the choice but does not expose a bug.
- **`atomic_wait_expected` positive on intent-to-park only.** Race-dependent
  (value changes between load and syscall); not deterministically triggerable.
  No test.
- **`atomic_notify` under-reporting the woken count.** Compliant with "at least";
  no test.
- **Store-triggered wakeup / native lost-wake-before-park.** Already covered by
  `notify_before_park_test` and `lost_wake_stress_test` in
  `atomic_wait_more_test.c`. No new test.
- **`max_threads_to_wake > 1` wake-all degeneracy.** Documented in
  `atomic_notify_more_test.c` (cap test asserts `k = 1` only). No new test.

---

## E. §3 CI coverage gaps — CI changes, not unit tests

- **`ALWAYS_USE_PTHREADS_BACKEND` on Windows/MSVC.** Cannot build (no
  `<pthread.h>`, no `pthread` lib). Options: (a) add a CI leg using the existing
  `cmake/toolchain-windows-mingw.cmake` with winpthreads; (b) document the option
  as POSIX-only and hard-fail the Windows configure path with a clear message.
- **Windows x86 (32-bit).** Add an x86 MSVC (or mingw) CI leg to exercise the
  8-byte hash-table fallback (`HAVE_WAIT_ON_ADDRESS_64` is `_WIN64`-gated,
  `atomic_wait_windows.c.ipp:142`).
- **Header-only + pthreads combination.** Trivial: add
  `-DHEADER_ONLY_BUILD=ON -DALWAYS_USE_PTHREADS_BACKEND=ON` to one existing
  matrix leg; the existing `header_only_test` (multi-TU ODR) then covers the
  weak-symbol coalescing of the pthreads proxy.
- **C23 `<stdatomic.h>` name collision.** Not testable until a C23 implementation
  ships the same function names; then a compile test including both headers would
  expose the macro redefinition (`atomic_wait.h:265-384`). Record as forward-compat
  constraint (already in Readme).

---

## F. Original open item (kept)

### F4. Wake-by-store and negative `atomic_wait_expected` return are intentionally untested

Per `combined-analysis.md` §1.4/§4, these are deliberately NOT asserted as
positive tests, and a future reviewer should not "complete" them naively:

- **Store-triggered wake**: `atomic_wait` may return after a value-changing store on any
  backend (a waiter in its load/re-check/park window observes the new value, and Windows
  native wakes every parked waiter). Proposal NOTE 1 is advisory. The suite therefore makes
  **no** negative "store does not wake" assertion; it only asserts notify-driven
  wake counts.
- **Negative `atomic_wait_expected` return**: only reachable on a genuine backend
  failure (wait_on_address hard error / hash-table allocation failure), not
  deterministically triggerable by a portable unit test. The suite asserts `0` on a
  negative `duration` (advisory) instead (`atomic_wait_expected_test.c`).

Note: the negative-`duration` *return value* is covered by the suite, but the
negative-`duration` *leak* is the new A1.3 test — these are separate behaviours.

---

## G. Implementation order

1. **A1.2** and **A1.3** — no library changes; immediately turn CI red on the
   pthreads backend legs, confirming the two High/Medium bugs.
3. **A1.1 hook + Test B** — the one-line guarded hook in
   `atomic_wait_common.ipp.ipp`; deterministic lost-wake exposure.
4. **A1.10** — wait for the WG14 wording decision before committing either variant.
5. **§3 CI legs** — pthreads-on-Windows, Win32-x86, header-only+pthreads.
