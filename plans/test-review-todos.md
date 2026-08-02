# Test Suite Review: Problematic Patterns and Missing Coverage

Source of truth: `docs/proposal.md` (§7.17.7.7–7.17.7.11) for API semantics,
together with the library's concurrency/correctness constraints and validation
plan (previously captured in `plans/wg14_atomic_waits-reference-library.md`,
which has been removed).

Reviewed files: `test/test_common.h`, `test/atomic_wait_test.c`,
`test/atomic_wait_expected_test.c`, `test/atomic_notify_test.c`,
`test/benchmark_atomic_wait_test.c`, `test/header_only_test*.cpp`,
`test/compile_fail/*`, `test/CMakeLists.txt`.

---

## A. Problematic test patterns (fix / remove)

- [ ] **Remove sleep-based thread synchronisation in `atomic_wait_test.c`.**
  `thrd_sleep_ms(50)` before each notify illegally assumes the waiter has
  already parked. This is the exact anti-pattern forbidden by `AGENTS.md`
  rule 5 ("Never, EVER use sleeps alone to synchronise between threads")
  and is the primary source of CI flakiness. Add a `g_parked`/"ready"
  handshake flag (waiter sets a `_Atomic int parked = 1` immediately before
  calling `atomic_wait`; main spins on it with `atomic_load_explicit`
  `acquire` before storing/notifying).
- [ ] **Delete dead, misleading code in `atomic_wait_test.c`** (the
  `value = 0; atomic_notify_one(&value);` sequence at lines ~37–39 that runs
  before `thr2` is even created and notifies with no waiter parked). It adds
  an unexplained mutation + notify that cannot affect `thr2` and obscures the
  second sub-test.
- [ ] **Fix `atomic_notify_test.c` "wakes exactly one" sub-test**: the
  comment claims `atomic_notify_one` is verified to wake exactly one waiter,
  but the test actually drives the waiters with `atomic_notify_all` and never
  checks how many were woken. Either (a) rewrite it so a single
  `atomic_notify_one` is issued against an independent atomic object with `N`
  parked waiters and assert exactly one observed the wake, or (b) drop the
  misleading comment. Add a genuine `atomic_notify_one`-exactly-one test.
- [ ] **Fix `atomic_notify_test.c`/`atomic_wait_test.c` readiness logic** to
  use proper synchronisation. `notify_one_func` busy-spins on `g_ready` with
  `thrd_sleep_ms(1)`; the sleep-with-flag pattern is permitted under rule 5,
  but ensure every waiter is guaranteed parked before the notify (use the same
  parked-count handshake as above), otherwise the test is racy.
- [ ] **Make `benchmark_atomic_wait_test.c` a real benchmark or remove it.**
  It is currently a stub: `consumer_func` is never spawned, nothing waits,
  nothing notifies, and the main body is `(void)consumer_func; return 0;`.
  It exercises none of the library. It should stress
  `N` producers / `M` consumers and remain `EXCLUDE_FROM_ALL` + excluded from
  `ctest` with `-E benchmark`. Do not let a placeholder masquerade as a
  benchmark.
- [ ] **Remove `#include <string.h>` where unused** (it is unused in
  `atomic_wait_test.c`, `atomic_notify_test.c`, `benchmark_atomic_wait_test.c`),
  and remove the redundant `#include <string.h>` in
  `atomic_wait_expected_test.c`.
- [ ] **Assert on `thrd_join` results in `atomic_notify_test.c`** — currently
  `thrd_join(waiters[i], &r)` ignores the return status and `r`, so a crashed
  or failed waiter thread would go undetected.
- [ ] **`atomic_wait_test.c` uses a local `atomic_int value`** shared with
  threads; fine, but it is (wrongly) not `_Atomic`-qualified post-typedef in
  the cast helpers. Verify `waiter_func`/`waiter_explicit_func` return the
  loaded value through a legal atomic read (`atomic_load_explicit`) rather
  than `return *value;` (plain non-atomic deref of an `_Atomic` object reads
  the raw object bits, which is relying on implementation layout).
- [ ] **Keep `main()` thin**: tests call their `*_test`/`*_main` helper then
  `return ret;` but a failed `CHECK` only increments `ret` — ensure `main`
  actually returns nonzero on failure so ctest fails. (Current `main`s do
  `return atomic_wait_test();` etc., which is correct; keep this pattern in
  any new tests.)

---

## B. Missing runtime tests (add)

Coverage today only exercises 4-byte objects (`atomic_int`,
`atomic_uint_least32_t`). The following are untested and must be added,
mirroring `docs/proposal.md` semantics and the sub-native-width hash-table
fallback:

### B1. `atomic_wait` / `atomic_wait_explicit`
- [ ] Immediate return when `*object != expected` (no suspension).
- [ ] Suspend-then-notify positive path that does NOT rely on sleep
  (use a parked-handshake; assert waiter observed the notified value).
- [ ] Spurious-wake loop: inject a notify before the waiter parks (or a
  dummy notify while parked with `*object` still `== expected`) and assert
  the thread re-compares, re-parks, and only returns once the value differs.
- [ ] **Reveal the PRIMARY RACE (proxy flag never reset → lost wake/busy-spin).**
  Drive the **hash-table fallback path** (sub-native widths — `atomic_uint_least8_t`
  / `atomic_uint_least16_t` on every backend, or `atomic_uint_least64_t` on Linux —
  not the 4-byte kernel fast path) through repeated **wait → spurious-notify
  (value still `== expected`) → re-park → real store + notify** cycles on the *same*
  object, so the wait-queue node stays alive across cycles. Assert the waiter
  actually **blocks/sleeps** between re-parks and only returns once the value
  really differs, i.e. it does **not** busy-spin at 100% CPU and does **not**
  return early. Detection that is not masked by poll-based correctness: after a
  spurious notify with the value unchanged, check the waiter has not returned AND
  that it yields to other threads (a starved cooperative tick/time-budget counter,
  or elapsed-time/CPU-time bounds), because with the never-reset 0/1 flag the
  re-park `wait_on_address(..., 0)` hits an already-`1` flag and returns `EAGAIN`
  immediately, turning the wait into a tight spin and effectively losing the sleep/
  wake contract for that node.
- [ ] `atomic_wait_explicit` with a non-`seq_cst` order compiles and behaves
  (e.g. `memory_order_acquire`).
- [ ] Wake-by-store (no explicit notify): a plain `atomic_store` (per
  `proposal.md` NOTE 1 "a wakeup is ... triggered by ... an atomic store or
  exchange") resulting in value change must unblock the waiter.
- [ ] **Notify-without-store / notify-before-park lost-wake stress test**
  (step3.7 §1.4/§2.2). Stress the classic futex-style race: notifier calls
  `atomic_notify_one`/`atomic_notify_all` *before* the waiter has parked and
  *without* changing the value, while a second store then changes the value
  shortly after. Assert the waiter is reliably woken **within a bounded time**
  and observes the final value — i.e. no permanent hang/lost wake — across many
  iterations. Run it on **both** the native 4-byte fast path and the hash-table
  fallback widths, since the race is distinct in each (native: wait parks on an
  unchanged value with no later notify; hash table: notify token set before park).
  Because this is inherently an interleaving race, use a parked-handshake plus a
  bounded wall-clock deadline (not sleep-only) so a lost wake fails fast rather
  than hanging CI.
- [ ] **Spurious wake in the hash-table path (explicit)** (step3.7 §2.4). Unlike
  the generic spurious-wake item above, drive a **sub-native-width / 8-byte-on-Linux**
  object through the hash-table proxy and inject dummy notifies *while parked* with
  the value unchanged, asserting the waiter re-parks correctly and only returns when
  a real store + notify changes the value. This specifically exercises the proxy's
  re-compare-and-re-park loop (and complements the PRIMARY RACE item).

### B2. `atomic_wait_expected`
- [ ] Positive return when the thread was actually suspended at least once.
- [ ] Real `timespec` timeout that BLOCKS then times out: assert return `0`
  and that `*expected` on return still equals the value on entry (proposal:
  "in the case of duration timeout, it will be the same as on entry").
- [ ] Wait-then-notify with `*expected` reload: after wake, assert `*expected`
  is updated to the most-recently-loaded object value.
- [ ] Negative / error return path (e.g. invalid `duration`, such as
  negative `tv_sec`/`tv_nsec`) returns a negative value.
- [ ] Sub-second and multi-second durations to exercise the macOS
  `UINT32_MAX`-ns cap-and-loop and ceiling-conversion paths.
- [ ] Use the specified `atomic_uint_native_wait_notify_t` type directly
  (the typedef is the proposal's canonical operand type and is currently
  never exercised).

### B3. `atomic_notify` and `atomic_notify_one` / `atomic_notify_all`
- [ ] `atomic_notify_one` wakes exactly one of `N` parked waiters (assert
  via a woken counter).
- [ ] `atomic_notify_all` wakes all `N` parked waiters (already started in
  `atomic_notify_test.c` but never asserted — assert the woken count).
- [ ] `atomic_notify` success path: CAS succeeds, returns positive, and the
  value becomes `desired`.
- [ ] `atomic_notify` with `max_threads_to_wake == 0` returns 0 with no side
  effects (plan MAY/fast-path).
- [ ] `atomic_notify` respects the cap: with `M` parked waiters and
  `max_threads_to_wake = k < M`, assert exactly `k` (or fewer) wake, never
  more than `max_threads_to_wake`.
- [ ] `atomic_notify` CAS failure already exists (`expected != *object`
  returns 0) — keep and extend to assert `*expected` on return equals the
  observed value (currently asserted in `atomic_wait_expected_test.c`).
- [ ] Concurrent notifies from multiple threads (concurrency constraint from
  the reference plan).

### B4. Width variants (exercise hash-table fallback + native widths)
- [ ] 1-byte object: `atomic_uint_least8_t` wait/notify_one/notify_all
  (uses hash table on all POSIX backends).
- [ ] 2-byte object: `atomic_uint_least16_t` same set.
- [ ] 8-byte object: `atomic_uint_least64_t` wait/notify_one/notify_all
  (Linux falls back to hash table; macOS/FreeBSD/Windows bypass).
- [ ] Object isolation: waiting on object A is not woken by
  `atomic_notify` on object B.

---

## C. Compile/typedef/header-only coverage gaps

- [ ] `header_only_test1.cpp`/`header_only_test2.cpp` define `notify_fn` /
  `wait_all_fn` but never call them — acceptable for an ODR/linkage test, but
  add a `main` (or call site) that invokes them once so the header's inline
  definitions are actually pulled in and executed in the header-only build.
- [ ] Compile-fail coverage for all six APIs in both C and C++ exists (12
  tests) — verify they are actually wired into `ctest` and that the expected
  diagnostic regexes match current compiler output for gcc, clang, and MSVC
  (the `WIDTH_DIAGNOSTIC`/`NATIVE_DIAGNOSTIC*` strings are brittle across
  compilers).

---

## D. Validation

- [ ] Re-run `cmake --build . && ctest --output-on-failure --timeout 300
  -E benchmark` after changes (both normal and `-DHEADER_ONLY_BUILD=ON`),
  per the library's Validation Plan (now in CI / the removed reference plan).
- [ ] Confirm all new/changed `.c` files pass `clang-format -n` (AGENTS.md
  rule 2) and stay C11-compatible (AGENTS.md rule 1).
- [ ] Confirm no new sleep-only synchronisation is introduced (AGENTS.md
  rule 5).
