# Test Suite Review: Exposure Tests for `plans/combined-analysis.md`

Companion to `plans/combined-analysis.md`. For **every** item in that document
(§1 deviations 1.1–1.11, §2 under-specified wording, §3 CI coverage gaps) this
file states whether a test can be written to expose the issue, and gives the
full test design (file, mechanism, assertions, gating, runtime, expected status
on today's code, caveats).

Verdicts in the mapping table:

| Item | Verdict | Test ref |
| 1.4 Store/exchange wakeups absent on fallback | Not testable portably (advisory; backend-dependent) | — |
| 1.10 `atomic_notify(max=0)` no-op | **Testable**, but expectation encodes ambiguous wording | A1.10 |
| §2 under-specified bullets | No exposure tests (characterization only) | D |
| §3 CI gaps | Not unit tests — CI additions (one doubles as A1.5) | E |

---

## A. Deterministically testable — implement these



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


---

## C. Not testable — document, don't chase

- **1.4 Store/exchange wakeups on the fallback path.** A bare store waking a
  parked waiter is backend-dependent (Windows `WaitOnAddress` wakes on a write;
  the POSIX futex/ulock paths do not), NOTE 1 is advisory, and the proposal
  imposes no "store must not wake" nor "store must wake" requirement. A portable
  positive or negative assertion is impossible. Existing B4 guidance below
  stands: make no store-wake assertion.

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
negative-`duration` *leak* is covered by the A1.3 `wait_expected_leak_test.c` —
these are separate behaviours.

---

## G. Implementation order

4. **A1.10** — wait for the WG14 wording decision before committing either variant.
5. **§3 CI legs** — pthreads-on-Windows, Win32-x86, header-only+pthreads.
