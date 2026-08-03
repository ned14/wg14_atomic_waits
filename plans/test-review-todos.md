# Test Suite Review: Problematic Patterns and Missing Coverage

Source of truth: `docs/proposal.md` (§7.17.7.7–7.17.7.11) for API semantics,
together with `plans/combined-analysis.md` (implementation deviations) and the
concurrency/correctness constraints from `AGENTS.md`.

---


## B. Open / not-fully-implemented items

### B1. [ ] PRIMARY RACE is still not fixed (`atomic_wait_race_test` is RED)

Regression test `test/atomic_wait_race_test.c` was added and correctly *detects*
the busy-spin (prints "PRIMARY RACE detected: waiter busy-spun ~0.30 s CPU …"),
but against the current library it **fails** on every hash-table-fallback backend:

- Fails on Linux/macOS/FreeBSD **default** backends for the 1/2/8-byte widths
  (the 1-byte width this test uses routes through the hash-table proxy).
- Passes on the **pthreads** backend (its mutex/condvar/token proxy suspends
  correctly) and on **Windows** (all widths native `WaitOnAddress`, real
  suspension).

Verified locally: fails on macOS default, passes with `-DALWAYS_USE_PTHREADS_BACKEND=ON`.
Because it is wired into `ctest`, this currently makes the Linux (pthreads OFF)
and macOS (pthreads OFF) CI matrix legs fail, and would also fail FreeBSD default.

Root cause is unchanged from `combined-analysis.md` §1.1/§1.2: the default proxy
flag (`atomic_wait_common.ipp.ipp:41-49` and the re-park at `:220-222`) is set to
`1` on wake and **never re-armed** (reset to `0`), so the re-park
`wait_on_address32(..., 0)` hits an already-`1` flag, returns `EAGAIN` immediately,
and the waiter busy-spins instead of suspending (proposal §7.17.7.7 requirements).

Action: fix the library (re-arm the proxy flag to `0` on re-park — reset under the
hash-table lock before parking), or gate/XFAIL/`BOOST`-style-exclude this test from
`ctest` until the library is fixed so CI is not red by default.


### B4. [ ] Wake-by-store (B1) and negative-return (B2) are intentionally untested — document, don't chase

Per `combined-analysis.md` §1.4/§4, these are deliberately NOT asserted as positive
tests, and a future reviewer should not "complete" them naively:

- **Store-triggered wake**: `atomic_wait` may return after a value-changing store on any
  backend (a waiter in its load/re-check/park window observes the new value, and Windows
  native wakes every parked waiter). Proposal NOTE 1 is advisory. The suite therefore makes
  **no** negative "store does not wake" assertion (see B2); it only asserts notify-driven
  wake counts.
- **Negative `atomic_wait_expected` return**: only reachable on a genuine backend
  failure (wait_on_address hard error / hash-table allocation failure), not
  deterministically triggerable by a portable unit test. The suite asserts `0` on a
  negative `duration` (advisory) instead (`atomic_wait_expected_test.c`).

These are fully covered by existing tests as far as is portable; leave as documented
behaviour rather than adding a flaky/wrong assertion.

### B5. [ ] Lost-wake stress covers only the hash path (small gap, NEW)

`lost_wake_stress_test` (`atomic_wait_more_test.c`) drives only the 8-bit
hash-table path. The original item asked to stress **both** the hash fallback and
the native 4-byte fast path, since the lost-wake interleaving differs per path.
Add a 4-byte (`atomic_uint_native_wait_notify_t`) variant of the same
notify-before-park → store → notify_all cycle.

### B6. RESOLVED — Linux futex wake-all only woke one waiter (fixed 2026-08-03)

`atomic_wait_widths_test` and `atomic_notify_more_test` timed out on the Linux
futex backend. Root cause: `wake_by_address32` in `atomic_wait_linux.c.ipp`
passed `(int) max_threads_to_wake` straight into `FUTEX_WAKE`. The wake-all
call sites (`atomic_notify_all_4` and the hash-proxy wake-all) pass
`(unsigned)-1`, which becomes `-1`, and the kernel's `futex_wake()` loop
(`if (++ret >= nr_wake) break`) breaks after waking exactly **one** waiter.
With multiple parked waiters (2-byte/8-byte roundtrips on Linux use the hash
path; cap/concurrent notify tests park 6 on the native 4-byte path), all but one
waiter stayed parked forever and `thrd_join` hung.

Fixed by clamping the unsigned count to `INT_MAX` before the syscall
(`max_threads_to_wake > INT_MAX ? INT_MAX : (int) max_threads_to_wake`), so
wake-all performs `FUTEX_WAKE(INT_MAX)`, matching how libstdc++/Boost wake-all.
Note: the macOS (WAKE_ALL), Windows (WakeByAddressAll) and pthreads (broadcast)
backends already special-cased the wake-all count; only the Linux futex pass-through
was broken.

Unrelated, still-open Linux observation (not the reported hang): `wait_on_address32`
passes the *relative* `ts_remaining` from `atomic_wait_expected_32`/`atomic_wait_generic`
straight to `FUTEX_WAIT`, which expects an **absolute** CLOCK_MONOTONIC deadline
(FreeBSD's backend likewise treats it as absolute), so timed waits busy-spin until
their deadline instead of sleeping. This does not affect the non-timed
wait/notify tests; fixing it requires standardising the timeout contract across
the backends.

