# Deviations from Plan and Proposal

Tracked deviations found by comparing the implementation against
`docs/proposal.md` and `plans/wg14_atomic_waits-reference-library.md`.

Each item is a todo: mark `[ ]` when open, `[x]` when fixed.

---

## [BUG] Linux `atomic_notify_64` incorrectly calls 32-bit futex on an 8-byte object

**File:** `include/wg14_atomic_waits/detail/impl/atomic_wait_linux.c.ipp:453-479`

Linux futex is 32-bit only. The plan (Step 7, lines 270-289) and the bypass
table are unambiguous: both wait and notify for 8-byte objects on Linux must
use the hash table. `atomic_wait_expected_64` is correctly delegated to
`atomic_wait_expected_generic` (hash table), but `atomic_notify_64` calls
`futex_wake((const volatile _Atomic(uint_least64_t) *) object, 1)`, which
casts an 8-byte address to a 32-bit futex operand. The waker wakes waiters
parked on the *first four bytes* of the 8-byte atomic, not on the 8-byte
value — incorrect inter-thread handoff semantics.

Compare to macOS (`.ipp:583-585`), FreeBSD (`.ipp:611-618`), and Windows
(`.ipp:452-460`), where the 64-bit notify functions loop over the native
primitive directly; those are correct because their native primitives
support 8-byte operands.

- [x] Fix `atomic_notify_64` in `atomic_wait_linux.c.ipp` to delegate to `atomic_notify_generic` (hash table) instead of calling `futex_wake` directly on an 8-byte address.

---

## [DEVIATION] `_64` implementations exist in backends but are unreachable from the public API

**Files:**
- `include/wg14_atomic_waits/atomic_wait.h:77-88, 195-211` (public declarations/dispatch)
- `include/wg14_atomic_waits/detail/impl/atomic_wait_linux.c.ipp:444-479`
- `include/wg14_atomic_waits/detail/impl/atomic_wait_macos.c.ipps:475-586`
- `include/wg14_atomic_waits/detail/impl/atomic_wait_freebsd.c.ipp:509-619`
- `include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp:367-460`
- `include/wg14_atomic_waits/detail/impl/atomic_wait_pthreads.c.ipp:280-309`

The plan (Step 3, line 148) explicitly fixes `uint_native_wait_notify_t` as
always `uint_least32_t` (4 bytes). Under that choice, the public API must
restrict `atomic_wait_expected`/`atomic_notify` to 4-byte `C` types, and the
current dispatch macros do this correctly via `sizeof(*(object))` assertion.

However, every backend `.ipp` file also implements `atomic_wait_expected_64`
and `atomic_notify_64` — functions that accept 8-byte operands. These `_64`
functions are never declared in the public header and are never called from
any reachable code path. They are dead code.

This dead code is the direct cause of deviation #1 (the Linux
`atomic_notify_64` bug): an unreachable function that was never exercised,
never reviewed in the context of the platform's actual constraints, and
contains the wrong primitive (32-bit futex) for its 8-byte signature.

On macOS, FreeBSD, and Windows the `_64` functions would be correct native
implementations if they were ever made reachable — but they are not.

- [x] Remove all `atomic_wait_expected_64` and `atomic_notify_64`
      implementations from the backend `.ipp` files, since `uint_native_wait_notify_t`
      is always 32-bit and the public API never calls them. This also eliminates
      the Linux `atomic_notify_64` bug by removing the buggy function entirely.

---

## [DEVIATION] `int_native_wait_notify_t` types visible in both C and C++; header restructure complete

**File:** `include/wg14_atomic_waits/atomic_wait.h`

The types `int_native_wait_notify_t`, `uint_native_wait_notify_t`,
`atomic_int_native_wait_notify_t`, `atomic_uint_native_wait_notify_t` are
now declared unconditionally (removed the `#ifndef __cplusplus` guard).
The `extern "C"` block wraps actual declarations in both C and C++ mode,
and the C++ dispatch macros now call the same width-specific C functions
(`atomic_wait_1/2/4/8`, `atomic_notify_one_1/2/4/8`, etc.) rather than
no-ops. All three related deviations are resolved by the same restructure.

- [x] Remove the `#ifndef __cplusplus` guard around the
      `int_native_wait_notify_t`/`uint_native_wait_notify_t` typedefs in
      `atomic_wait.h`.
- [x] Restructure `atomic_wait.h` so the `extern "C"` block contains actual
      declarations in C++ mode (no longer vacuous).
- [x] C++ dispatch macros now call real C implementations; C and C++ share
      the same code path. The macros rely on `<stdatomic.h>` being available
      in C++ via `config.h`. If a C++ configuration cannot provide C11 atomics
      (e.g. a pre-C++11 compiler), the build will fail with clear errors
      rather than silently producing no-ops.

---

## [DEVIATION] Hash table is fixed-size; plan specifies growable

**Plan** (Step 7, lines 296-304): *"Fixed power-of-2 bucket count (e.g., 1024
initially, growable via realloc if load factor exceeds threshold)."*

**Implementation** (`include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp:51`):
```c
#define WG14_ATOMIC_WAITS_HASH_BUCKETS 1024
```
There is no growth path — `hash_table_find_or_create` returns `NULL` after
probing all 1024 slots without finding an empty bucket (`:114-116`) rather
than reallocating. For workloads with many distinct atomic objects this
silently drops waiters.

- [x] Implement growth in the hash table when the probe sequence exhausts
      all buckets, or at minimum document the fixed-size limitation and
      consider increasing the default bucket count.

---

## [DEVIATION] `volatile` stripped inconsistently between Windows bypass path and hash-table path

**File:** `include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp:42-46`

The public `atomic_wait` dispatch macro casts `object` preserving `volatile`,
but the Windows `.ipp` passes the address to `WaitOnAddress` via
`(PVOID)(uintptr_t)object`, which drops `volatile`. The proposal requires the
object to be accessed only via atomic operations; the `volatile` qualifier is
part of the signature. The plan's language (Step 3, lines 179, Step 7 line
304) specifically calls out that the cast discards volatile for hash keys.
For the Windows bypass path, the pointer is passed to a kernel API that
*dereferences* it indirectly (the kernel reads the memory for comparison), so
the volatile qualifier is not semantically present — same as the hash-table
path, but the inconsistency between the two paths is worth noting.

- [x] Document that the Windows bypass intentionally strips `volatile`
      when passing the address to `WaitOnAddress`/`WakeByAddressSingle`/
      `WakeByAddressAll`, because these kernel primitives provide their own
      atomic memory-ordering guarantees. This matches how the hash-table
      path discards `volatile` for address-based keys (plan Step 3, Step 7).

---

## [DEVIATION] Header is not usable from C++; all public macros are no-ops and types are undefined

**File:** `include/wg14_atomic_waits/atomic_wait.h` — **FIXED**

C++ mode now:
- Sees the public type definitions (`int_native_wait_notify_t`, etc.) because
  they are no longer guarded by `#ifndef __cplusplus`.
- Calls the real C implementations via the same width-dispatching macros used
  in C (`atomic_wait_1/2/4/8`, `atomic_notify_one_1/2/4/8`, etc.).
- Has `<stdatomic.h>` available in C++ mode via `config.h`, which provides
  `atomic_load_explicit`, `_Atomic`, `memory_order`, etc.

The C++ compile-test files (`test/header_only_test.cpp`,
`test/header_only_test1.cpp`, `test/header_only_test2.cpp`) should be
updated to exercise real wait/notify semantics rather than the no-op path,
so that this gap is caught if ever reintroduced.

- [x] Provide real C++ implementations of all six public macros. The C++
      editions now call through to the same width-specific C functions rather
      than a separate code path.
- [x] Types are unconditionally visible in both C and C++.
- [x] A C++ configuration that cannot compile the C path (e.g. a pre-C++11
      compiler without `<stdatomic.h>`) will fail at compile time with
      diagnostics rather than silently producing no-ops.

---

## [MINOR] `extern "C"` block in `atomic_wait.h` wrapped empty C++ branches — **FIXED**

**File:** `include/wg14_atomic_waits/atomic_wait.h`

The type definitions and function declarations now appear directly inside the
`extern "C"` block in both C and C++ mode. The wrapper is no longer vacuous
in C++.

- [x] Restructure `atomic_wait.h` so the `extern "C"` block contains actual
      declarations in C++ mode, or remove the vacuous outer wrapper.

---

## [MINOR] Windows converts `timespec` to milliseconds with floor division; proposal requires ceiling

**Plan** (Step 3): *"Use ceiling conversion from *duration … ensuring total
accumulated wait is at least *duration."*  
**Proposal**: *"Total accumulated time in this function shall be at least
*duration."*

**Implementation** (`include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp:334-336`):
```c
ULONGLONG ms = (ULONGLONG) duration->tv_sec * 1000ull + duration->tv_nsec / 1000000ull;
```
`tv_nsec / 1000000ull` is floor division. A duration of 1ns converts to 0ms,
which `WaitOnAddress` treats as `0` (return immediately), not as "at least 1ns".
macOS and FreeBSD use absolute-time comparisons via the kernel (no rounding
issue), and the hash-table path uses `clock_gettime` deadlines. Linux avoids
this through its absolute-time futex path. Only Windows has this
floor-truncation bug.

- [x] Change the Windows `timespec`-to-milliseconds conversion to ceiling
      semantics, e.g., `(duration->tv_nsec + 999999) / 1000000ull`.

---

## [MISSING] No Doxygen-format API documentation for public macros and functions

**File:** `include/wg14_atomic_waits/atomic_wait.h`

The project ships a `Doxyfile` and a `doc/` output directory, but the public
header contains no Doxygen comment blocks. Every public macro and function
described in `docs/proposal.md` is undocumented:
`int_native_wait_notify_t`, `uint_native_wait_notify_t`,
`atomic_int_native_wait_notify_t`, `atomic_uint_native_wait_notify_t`,
`atomic_wait`, `atomic_wait_explicit`, `atomic_notify_one`,
`atomic_notify_all`, `atomic_wait_expected`, `atomic_notify`.

The proposal provides rich semantic information suitable for API docs: the
`void`/`int` return types, the memory_order semantics, the `restrict`
pointer contracts on `atomic_wait_expected`/`atomic_notify`, the spurious
wake and re-park rules, the `*expected` update behaviour, the
`max_threads_to_wake` semantics, and the `sizeof(C)` constraint for the
`_expected`/`notify` pair.

- [x] Add Doxygen documentation to every public macro and type in
      `atomic_wait.h`, using the proposal's synopsis and description text to
      produce documentation aimed at a typical programmer (not just committee
      wording).

    Uses `//!` trailing-comment briefs and `/*! ... */` block-form details
    with `\param`, `\return`, `\retval`, and `\details` tags. The `/** ... */`
    form is not used anywhere in the header.

      - One-line `//!` brief (function signature paraphrase).
      - `/*! \details ... */` block with `\param`/`\return`/`\retval` tags
        where applicable.
      - Plain-English `\details` paragraph for subtle semantics (spurious
        wake re-park, `*expected` update, `max_threads_to_wake`, duration
        ceiling/conversion).
    Verify with `doxygen Doxyfile` that `doc/html/` is populated without
    warnings.
