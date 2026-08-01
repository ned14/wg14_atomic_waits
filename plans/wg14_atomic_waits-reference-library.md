# Plan: Reference Library Implementation for `wg14_atomic_waits`

## Goal

Create a full C11 reference library (`wg14_atomic_waits`) that implements the `atomic_wait`/`atomic_notify` semantics from `docs/proposal.md`, mirroring the structure, format, and style of `/var/home/ned/boostish/wg14_signals`.

`docs/proposal.md` is the authoritative spec. Behavior, return values, and memory-order semantics must match it exactly.

---

## Key Design Decision: Prefixing Convention

Every public macro, type, and function name MUST be wrapped in `WG14_ATOMIC_WAITS_PREFIX(...)`.

Since the proposal uses names (`atomic_wait`, `atomic_notify_one`, `atomic_notify_all`, etc.) that collide with C23 `<stdatomic.h>` identifiers, `WG14_ATOMIC_WAITS_PREFIX(x)` is the mandatory indirection that allows users (and the library itself) to avoid name collisions.

For the implementation, `config.h` defaults `WG14_ATOMIC_WAITS_PREFIX(x)` to `x`, exposing the proposal names verbatim when included standalone. Users who include both `<stdatomic.h>` and this library can `#define WG14_ATOMIC_WAITS_PREFIX(x) WG14_ATOMIC_WAITS_##x` before including to get prefixed names.

---

## Header-Only Compatibility (mandatory)

The entire library **must** be header-only compatible. Every backend `.ipp` file (`atomic_wait_linux.c.ipp`, `atomic_wait_freebsd.c.ipp`, `atomic_wait_pthreads.c.ipp`, `atomic_wait_macos.c.ipp`, `atomic_wait_windows.c.ipp`) must be safe to `#include` into **any** translation unit, regardless of whether the TU also includes `<stdatomic.h>` or any other header.

This means:
- Every `.ipp` must include its own required system headers (`<stdatomic.h>`, `<time.h>`, `<sys/umtx.h>`, `<bsd/sys/ulock.h>`, `<synchapi.h>`, etc.) — never rely on the includer having included them first.
- No backend `.ipp` may contain non-`static inline` function or variable definitions without `WG14_ATOMIC_WAITS_IGNORE_MULTIPLE_DEFINITIONS` (or equivalent `static` / `inline` guard), since multiple TUs may include the same `.ipp`.
- No backend `.ipp` may reference symbols with external linkage unless they are declared `WG14_ATOMIC_WAITS_EXTERN` and provided by a compiled source TU (or the header-only path).
- `atomic_wait.h`'s header-only `#if WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY` block selects exactly one backend `.ipp` based on the platform macros (`_WIN32`, `__linux__`, `__FreeBSD__`, `__APPLE__`, else POSIX fallback). The selected `.ipp` must provide the complete implementation of all six public functions when included.

---

## File Layout (mirrors `wg14_signals`)

```
wg14_atomic_waits/
├── .gitattributes
├── .gitmodules
├── .github/
│   └── workflows/
│       └── ci.yml
├── AGENTS.md
├── CMakeLists.txt
├── LICENSE                          (Apache 2.0, same as wg14_signals)
├── Readme.md
├── .clang-format
├── cmake/
│   ├── ProjectConfig.cmake.in
│   ├── sanitize-toolchain.cmake
│   ├── filc-toolchain.cmake
│   └── toolchain-windows-mingw.cmake
├── doc/
│   └── html/                        (Doxygen output, empty placeholder)
├── include/
│   └── wg14_atomic_waits/
│       ├── config.h                 (all WG14_ATOMIC_WAITS_* macros)
│       ├── atomic_wait.h            (primary public API header)
│       └── detail/
│           └── impl/
│               ├── atomic_wait_common.ipp.ipp  (shared internal types/helpers)
│               ├── atomic_wait_linux.c.ipp      (Linux futex wait/notify impl)
│               ├── atomic_wait_freebsd.c.ipp    (FreeBSD umtx wait/notify impl)
│               ├── atomic_wait_pthreads.c.ipp    (non-Linux non-FreeBSD POSIX pthread wait/notify impl)
│               ├── atomic_wait_macos.c.ipp      (macOS ulock wait/notify impl)
│               └── atomic_wait_windows.c.ipp    (Windows wait/notify impl)
├── src/
│   └── wg14_atomic_waits/
│       ├── atomic_wait_linux.c      (1-line: includes the .ipp)
│       ├── atomic_wait_freebsd.c    (1-line: includes the .ipp)
│       ├── atomic_wait_pthreads.c      (1-line: includes the .ipp)
│       ├── atomic_wait_macos.c      (1-line: includes the .ipp)
│       └── atomic_wait_windows.c    (1-line: includes the .ipp)
└── test/
    ├── CMakeLists.txt
    ├── test_common.h
    ├── atomic_wait_test.c
    ├── atomic_wait_expected_test.c
    ├── atomic_notify_test.c
    ├── benchmark_atomic_wait_test.c
    ├── header_only_test.cpp         (C++ compile test)
    ├── header_only_test1.cpp        (C++ compile test, additional TU)
    └── header_only_test2.cpp        (C++ compile test, additional TU)
```

---

## AGENTS.md Additive Rule

Copy `wg14_signals/AGENTS.md` verbatim, then add a new rule 4:

```
4. C++ is permitted in `test/` solely for compile-testing the public header and
   verifying `extern "C"` linkage. Do NOT use C++ in any source or header file
   under `include/` or `src/`.
```

---

## Implementation Steps

### Step 1 — Project scaffolding files

Create the root-level files matching `wg14_signals` exactly:

| File | Action |
|---|---|
| `LICENSE` | Copy Apache 2.0 verbatim; update copyright notice to `[2026] [Niall Douglas]`. |
| `AGENTS.md` | Copy `wg14_signals/AGENTS.md` verbatim, then append the additive carve-out rule described above. |
| `.gitattributes` | Mirror `wg14_signals/.gitattributes`; ensure `*.ipp` `*.c.ipp` `*.ipp.ipp` are all `text svneol=native#text/plain`. Copy the file verbatim if possible. |
| `.clang-format` | Copy `wg14_signals/.clang-format` verbatim. |
| `.gitmodules` | Copy if any submodules (none expected for this project). |
| `cmake/ProjectConfig.cmake.in` | Copy `wg14_signals/cmake/ProjectConfig.cmake.in` verbatim. |
| `cmake/sanitize-toolchain.cmake` | Copy `wg14_signals/cmake/sanitize-toolchain.cmake` verbatim. |
| `cmake/filc-toolchain.cmake` | Copy `wg14_signals/cmake/filc-toolchain.cmake` verbatim. |
| `cmake/toolchain-windows-mingw.cmake` | Copy `wg14_signals/cmake/toolchain-windows-mingw.cmake` verbatim. |

---

### Step 2 — `include/wg14_atomic_waits/config.h`

Mirror the structure and macro set of `wg14_signals/config.h`:

- `WG14_ATOMIC_WAITS_PREFIX(x)` — identity default; user overridable
- `WG14_ATOMIC_WAITS_INLINE` — defaults to `inline`
- `WG14_ATOMIC_WAITS_THREAD_LOCAL` — `_Thread_local` (C11) / `thread_local` (C++)
- `WG14_ATOMIC_WAITS_NULLPTR` — `nullptr` (C23) or `NULL`
- `WG14_ATOMIC_WAITS_IGNORE_MULTIPLE_DEFINITIONS` — `__attribute__((weak))` / `__declspec(selectany)`
- `WG14_ATOMIC_WAITS_DEFAULT_VISIBILITY` — `__attribute__((visibility("default")))` / empty on Windows
- `WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY` — `0` default
- `WG14_ATOMIC_WAITS_EXTERN_IMPL` — export/visibility when `WG14_ATOMIC_WAITS_SOURCE` is defined
- `WG14_ATOMIC_WAITS_EXTERN` — inline when header-only, extern otherwise
- `WG14_ATOMIC_WAITS_STDERR_PRINTF(...)` — `fprintf(stderr, __VA_ARGS__)`
- `extern "C"` block for C++ compatibility; opens at top, closes at bottom

---

### Step 3 — `include/wg14_atomic_waits/atomic_wait.h` (primary public API header)

Copy the calling convention of `thrd_signal_handle.h` (~400–500 lines):

1. License block header comment
2. `#ifndef WG14_ATOMIC_WAITS_ATOMIC_WAIT_H` / `#define` include guard
3. `#include "config.h"`
4. `#include <stdatomic.h>` and `#include <time.h>`
5. `extern "C"` opening block
6. **Public type definitions**, all prefixed:
   - `WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t)` — implementation-defined typedef for the smallest `uint_leastN_t` where `N` is at least thirty-two and for which atomic waits and notifies have least overhead on this implementation; equals 4 bytes on all platforms.
   - `WG14_ATOMIC_WAITS_PREFIX(atomic_uint_native_wait_notify_t)` — `_Atomic` typedef
 7. **Public API declarations**, all with `WG14_ATOMIC_WAITS_EXTERN` and name-prefixed. The six functions split into two categories:
    - **Generic** (accept any `_Atomic` type): `atomic_wait`, `atomic_wait_explicit`, `atomic_notify_one`, `atomic_notify_all`
    - **Generic, runtime width-checked to native width** (generic signature but `sizeof(C) == sizeof(uint_native_wait_notify_t)` enforced at runtime): `atomic_wait_expected`, `atomic_notify`
   - `WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(const volatile A *object, C expected)`
   - `WG14_ATOMIC_WAITS_PREFIX(atomic_wait_explicit)(const volatile A *object, C expected, memory_order order)`
   - `WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(volatile A *object)`
   - `WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(volatile A *object)`
   - `WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(const volatile A *restrict object, C *restrict expected, const struct timespec *restrict duration, memory_order success, memory_order failure)`
   - `WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(volatile A *restrict object, C *restrict expected, C desired, unsigned max_threads_to_wake, memory_order success, memory_order failure)`
8. `extern "C"` closing block
9. Header-only `#if` block (before the closing `#endif` guard, matching `thrd_signal_handle.h:463-469`). Each included `.ipp` must be self-contained (includes its own system headers, no reliance on prior includes) and must not violate the one-definition rule when included into multiple TUs:
   ```c
   #if WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY
   #if defined(_WIN32) || defined(_WIN64)
   #include "detail/impl/atomic_wait_windows.c.ipp"
   #elif defined(__linux__)
   #include "detail/impl/atomic_wait_linux.c.ipp"
   #elif defined(__FreeBSD__)
   #include "detail/impl/atomic_wait_freebsd.c.ipp"
   #elif defined(__APPLE__)
   #include "detail/impl/atomic_wait_macos.c.ipp"
   #else
   #include "detail/impl/atomic_wait_pthreads.c.ipp"
   #endif
   #endif
   ```

**Proposal semantic pinning for the header:**
- `atomic_wait` and `atomic_wait_explicit` return `void`. Do not return a status code.
- `atomic_wait_expected` and `atomic_notify` use the `restrict`-qualified pointer pattern from the proposal; their declarations must match the synopsis verbatim.

---

### Step 5 — Source TU wrappers (`src/wg14_atomic_waits/`)

Each `.c` file is exactly 1 line, including its corresponding `.ipp`. Exactly mirrors `wg14_signals/src/wg14_signals/current_thread_id.c`. When not building header-only, these compiled TUs provide the externally-linked definitions of all `WG14_ATOMIC_WAITS_EXTERN` functions, ensuring exactly one definition across the whole program (ODR anchor):

```c
/* Proposed WG14 atomic wait/notify support
   (C) 2026 Niall Douglas <http://www.nedproductions.biz/>
   File Created: Jul 2026
   Licensed under the Apache License...
*/
#include "wg14_atomic_waits/detail/impl/atomic_wait_linux.c.ipp"
```

```c
/* Proposed WG14 atomic wait/notify support
   (C) 2026 Niall Douglas <http://www.nedproductions.biz/>
   File Created: Jul 2026
   Licensed under the Apache License...
*/
#include "wg14_atomic_waits/detail/impl/atomic_wait_freebsd.c.ipp"
```

```c
/* Proposed WG14 atomic wait/notify support
   (C) 2026 Niall Douglas <http://www.nedproductions.biz/>
   File Created: Jul 2026
   Licensed under the Apache License...
*/
#include "wg14_atomic_waits/detail/impl/atomic_wait_pthreads.c.ipp"
```

```c
/* Proposed WG14 atomic wait/notify support
   (C) 2026 Niall Douglas <http://www.nedproductions.biz/>
   File Created: Jul 2026
   Licensed under the Apache License...
*/
#include "wg14_atomic_waits/detail/impl/atomic_wait_macos.c.ipp"
```

Platform-selection in `CMakeLists.txt`:
```cmake
set(LIBRARY_SOURCES
  $<$<PLATFORM_ID:Windows>:src/wg14_atomic_waits/atomic_wait_windows.c>
  $<$<PLATFORM_ID:Darwin>:src/wg14_atomic_waits/atomic_wait_macos.c>
  $<$<PLATFORM_ID:Linux>:src/wg14_atomic_waits/atomic_wait_linux.c>
  $<$<PLATFORM_ID:FreeBSD>:src/wg14_atomic_waits/atomic_wait_freebsd.c>
  $<$<NOT:$<PLATFORM_ID:Windows>>:$<NOT:$<PLATFORM_ID:Darwin>>:$<PLATFORM_ID:Linux>>:$<PLATFORM_ID:FreeBSD>>:src/wg14_atomic_waits/atomic_wait_pthreads.c>
)
```

---

### Step 6 — Root `CMakeLists.txt`

Mirror `wg14_signals/CMakeLists.txt` with adaptations:

- `project(wg14_atomic_waits LANGUAGES C CXX)`
- `option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)`
- `option(HEADER_ONLY_BUILD "Build using header only (unity build)" OFF)`
- `set(LIBRARY_SOURCES ...)` — Windows, macOS, Linux (futex), FreeBSD (umtx), and non-Linux non-FreeBSD POSIX (pthread) platform-conditioned `src/` wrappers
- `target_compile_features(... c_std_11)`
- `target_compile_definitions(... PRIVATE WG14_ATOMIC_WAITS_SOURCE)`
- Compiler flags: `-Wall -Wextra -Wpedantic -Werror` (MSVC: `/W4`)
- `configure_file(... cmake/ProjectConfig.cmake.in ...)` and install cmake config + export targets
- `add_subdirectory("test")` when `PROJECT_IS_TOP_LEVEL`
- `add_code_example` and `add_code_test` helper functions matching `wg14_signals` exactly

**Windows-specific:** MSVC uses `/W4` (no `/experimental:c11atomics` needed for this library; C11 atomics are supported natively in VS2022).

---

### Step 7 — `include/wg14_atomic_waits/detail/impl/atomic_wait_common.ipp.ipp`

**Function-to-hash-table mapping:** On Windows, all six functions bypass the hash table for every operand size (1–8 bytes) — the hash table is never used. On macOS and FreeBSD, bypass occurs when `sizeof(*object) >= 4` and `object` is suitably aligned. On Linux, bypass occurs only when `sizeof(*object) == 4` (futex is 32-bit only). On the pthreads backend, bypass never occurs. When bypass is not possible (sub-4-byte types on Linux/macOS/FreeBSD, misaligned addresses, or the pthreads backend), all six fall back to the hash table. See table below for per-function details.

| Function | Hash table required? | Notes |
|---|---|---|
| `atomic_wait` / `atomic_wait_explicit` | **Windows: always bypass** (all sizes 1–8 bytes). **macOS: bypass** for 4-byte and 8-byte. **Linux/FreeBSD: bypass** for 4-byte only. **pthreads:** never bypass | Generic APIs; on Windows, `WaitOnAddress` supports 1/2/4/8 bytes. On macOS, `UL_COMPARE_AND_WAIT`/`UL_COMPARE_AND_WAIT64` supports 4-byte and 8-byte. On Linux, `FUTEX_WAIT` is 32-bit only (`int *uaddr`) so bypass is 4-byte only; 8-byte on Linux must use hash table. On FreeBSD, `UMTX_OP_WAIT` (native-width) supports 8-byte but `UMTX_OP_WAIT_UINT` is 4-byte only. On pthreads or sub-4-byte, fall back to hash table. |
| `atomic_notify_one` / `atomic_notify_all` | **Windows: always bypass** (all sizes 1–8 bytes). **macOS: bypass** for 4-byte and 8-byte. **Linux/FreeBSD: bypass** for 4-byte only. **pthreads:** never bypass | Generic APIs; on Windows, `WakeByAddressSingle`/`WakeByAddressAll` support 1/2/4/8 bytes. On macOS, `ulock_wake` supports 4-byte and 8-byte via matching flags. On Linux, `FUTEX_WAKE` is 32-bit only so bypass is 4-byte only; 8-byte must use hash table via `wake_waiters`. On FreeBSD, select `UMTX_OP_WAKE` (8-byte) or `UMTX_OP_WAKE_UINT` (4-byte). On pthreads or sub-4-byte, fall back to hash table via `wake_waiters`. |
| `atomic_wait_expected` | **Must bypass** on all native backends (Linux, FreeBSD, macOS, Windows); hash table on pthreads backend | Native-width API (4-byte `C` only); all four native backends support 4-byte operands. macOS selects `UL_COMPARE_AND_WAIT` (4-byte) or `UL_COMPARE_AND_WAIT64` (8-byte) at runtime. FreeBSD selects `UMTX_OP_WAIT` (native-width `long`) or `UMTX_OP_WAIT_UINT` (4-byte `uint32_t`). The pthreads backend must always use the hash table fallback because `pthread_cond_wait` has no kernel-level per-address wait tracker. |
| `atomic_notify` | **Must bypass** on all native backends (Linux, FreeBSD, macOS, Windows); hash table on pthreads backend | Native-width API (4-byte `C` only); must bypass via kernel primitive directly up to `max_threads_to_wake` times after CAS success. On macOS, use `ulock_wake` with the same `UL_COMPARE_AND_WAIT`/`UL_COMPARE_AND_WAIT64` flag matching the wait operation. The pthreads backend must always use the hash table via `wake_waiters`. |

Three native backends (macOS ulock, Windows WaitOnAddress, FreeBSD via `UMTX_OP_WAIT`) support the full native width including 8-byte operands. FreeBSD provides `UMTX_OP_WAIT` (native-width, `long`) for 8-byte and `UMTX_OP_WAIT_UINT` (4-byte, `uint32_t`) for 4-byte operands; the implementation must select the matching wake operation (`UMTX_OP_WAKE` / `UMTX_OP_WAKE_UINT`). macOS selects `UL_COMPARE_AND_WAIT` (4-byte) or `UL_COMPARE_AND_WAIT64` (8-byte) at runtime. Windows `WaitOnAddress` accepts `AddressSize` of 1, 2, 4, or 8 bytes per MSDN. The bypass condition is: bypass when `sizeof(*object) == 4` and the address is suitably aligned for the platform primitive, except on macOS and Windows where 8-byte also bypasses. The pthreads backend (`atomic_wait_pthreads.c.ipp`) has no kernel-level per-address wait tracker (`pthread_cond_wait` operates on a user-allocated `pthread_cond_t`), so it must always use the hash table fallback for all six functions.

**Hash-table bypass summary by backend:**

| Backend | 1 byte | 2 bytes | 4 bytes | 8 bytes | Hash table needed? |
|---|---|---|---|---|---|
| Linux (`FUTEX_WAIT`/`FUTEX_WAKE`) | ✗ | ✗ | ✓ | ✗ | For 1/2-byte and 8-byte; `futex` is 32-bit only (`int *uaddr`, `int val`) |
| macOS (`UL_COMPARE_AND_WAIT`/`UL_COMPARE_AND_WAIT64`) | ✗ | ✗ | ✓ | ✓ | For 1/2-byte, or sub-native-width types |
| Windows (`WaitOnAddress`) | ✓ | ✓ | ✓ | ✓ | **Never** — all operand sizes bypass |
| FreeBSD (`UMTX_OP_WAIT`/`UMTX_OP_WAKE`) | ✗ | ✗ | ✓ | ✓ | For 1/2-byte, or sub-native-width types; `UMTX_OP_WAKE` accepts a count parameter directly |
| pthreads fallback (`pthread_cond_wait`) | ✗ | ✗ | ✗ | ✗ | **Always** — no kernel tracker exists |

✓ = kernel primitive available; hash table is bypassed.
✗ = no suitable kernel primitive; must use the user-space hash table.

Linux, macOS, FreeBSD, and Windows bypass for aligned 4-byte atomics. macOS, FreeBSD, and Windows additionally bypass for 8-byte atomics; Linux does not (futex is 32-bit only). Windows additionally bypasses for 1-byte and 2-byte operands (`WaitOnAddress` supports `AddressSize` = 1, 2, 4, or 8 per MSDN). **The Windows backend never needs the hash table** — it can be implemented without any `wait_queue_t`, `wake_waiters`, or `LOCK`/`UNLOCK` machinery. The pthreads fallback always uses the hash table because `pthread_cond_wait` operates on a user-allocated `pthread_cond_t` with no kernel per-address tracker.

Shared internal implementation (~350–450 lines, analogous to `thrd_signal_handle_common.ipp.ipp` but adapted for atomic waits):

- `#include "../../config.h"` and `#include "../../atomic_wait.h"`
- Internal per-object wait queue type (`WG14_ATOMIC_WAITS_PREFIX(wait_queue_t)`): a singly linked intrusive list of parked threads, keyed by address of the atomic object being waited on. Parked threads store their `expected` value.
- A small, purpose-built **open-addressing hash table** implemented directly in this `.ipp.ipp`, mapping `void *object` address → `wait_queue_t *`. This table is the **mandatory fallback** for `atomic_wait` / `atomic_wait_explicit` / `atomic_notify_one` / `atomic_notify_all` (which accept any atomic type, including sub-native-width types). It is also the fallback for `atomic_wait_expected` / `atomic_notify` on platforms where the native primitive cannot operate on the given type width or alignment. This is **not** verstable.h; it is a minimal, single-purpose table with no generic template machinery, no custom allocator hooks, and no async-signal-safety requirements. Key design:
  - Fixed power-of-2 bucket count (e.g., 1024 initially, growable via `realloc` if load factor exceeds threshold).
  - Quadratic probing for collision resolution (matching the style of `wg14_signals`'s internal hashing, but without the 11-bit link/displacement metadata — that was specific to verstable.h's tombstone-free deletion strategy, which this library does not need).
  - Each bucket stores either a `void *key` (the `object` pointer) and a `wait_queue_t *value`, or an empty/deleted sentinel.
  - No separate metadata array; bucket is `struct { void *key; wait_queue_t *value; }`.
  - Hash function: simple pointer fingerprint, e.g., `((uintptr_t)key >> 3) ^ ((uintptr_t)key >> (3 + 4))` or `(uintptr_t)key * 0x9E3779B9u` — any decent mixing function is fine; the table is small and collisions are rare.
  - Insertion: quadratic probe until empty bucket found; store key/value.
  - Lookup: quadratic probe until key matches or empty bucket reached.
  - Deletion: set bucket to `{NULL, NULL}` sentinel (no tombstones needed because wait queues are long-lived and deletions are rare; if a bucket is re-inserted, probing skips empty sentinels correctly).
- An `atomic_flag` based lock serializes all mutations (insert, find, delete) of the hash table. Lookup during `atomic_notify` also holds the lock briefly to safely read/modify the wait queue. The `volatile`-to-`void*` cast for the key: `object` is `volatile A *` per the proposal; cast to `void *` via `(void *)(uintptr_t)object`. The cast discards `volatile` but does not dereference the pointer, so it is well-defined as a hash key. Test under `-Wextra` to suppress qualifier-discard warnings.
- **Implements all six public functions** with the following semantics required by the proposal:

  **`atomic_wait` / `atomic_wait_explicit` delegate to `atomic_wait_expected`:**
  - These are `void` per the proposal synopsis. They **must** be implemented as thin wrappers around `atomic_wait_expected`:
    - `atomic_wait(object, expected)` calls `atomic_wait_expected(object, &expected, NULL, memory_order_seq_cst, memory_order_seq_cst)` and discards the return value.
    - `atomic_wait_explicit(object, expected, order)` calls `atomic_wait_expected(object, &expected, NULL, order, order)` and discards the return value.
  - The `NULL` `duration` means `atomic_wait_expected` has no timeout, so it will suspend at least once if `*object == expected` and will only return once a notify occurs and the subsequent reload shows a mismatch.
  - The `*expected` update side-effect of `atomic_wait_expected` must be preserved (though the caller of `atomic_wait` does not observe it, since they passed `expected` by value). The implementation may pass the address of a local copy of `expected` to satisfy the `restrict` pointer requirement without mutating the caller's variable. On native-width backends where `sizeof(expected) == sizeof(uint_native_wait_notify_t) == 4` (or 8-byte on macOS/FreeBSD), this delegation bypasses the hash table entirely.
  - The spurious-wake re-park loop is thus implemented once, inside `atomic_wait_expected`, and inherited by both wrappers.

  **`atomic_wait_expected` `*expected` update:**
  - After every park return and before final return, reload `object` into `*expected`.

  **`atomic_notify` return value and `*expected` update:**
  - Perform `atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure)`. On success, unblock up to `max_threads_to_wake` waiters from the object's wait queue. Returns positive on exchanged+notified (possibly `1 + N` platform-dependent), 0 if CAS failed (memory ordering = failure), or negative on error. `*expected` on return is the value observed by the CAS.

  **`atomic_notify` CAS-failure path:**
  - Returns 0 and memory ordering `failure` without waking any threads.

  **C11 `_Atomic` compatibility:**
  - `object` is `volatile A *`. Access fields only via `atomic_load_explicit` / `atomic_compare_exchange_weak_explicit` / `atomic_compare_exchange_strong_explicit` / `atomic_store_explicit`. Never dereference `object` directly.

---

## Per-API Implementation Constraints and Freedoms

This section pins what the implementation agent MUST, MUST NOT, and MAY do for each public function.

### `atomic_wait` / `atomic_wait_explicit` (void)

**MUST:**
- Block until a subsequent `atomic_load` (or the next park return) observes that `*object` is no longer equal to `expected`, then return.
- Re-compare `expected` against `*object` after every park return; if still equal, re-park. This loop is required by the proposal wording ("if when woken up the value still compares equal to `expected` the thread is suspended again").
- Use `memory_order` from the call site (for `atomic_wait_explicit`) or the default `memory_order_seq_cst` (for `atomic_wait`) when accessing `*object`.
- Treat the `volatile` qualifier on `object` correctly: access via `atomic_load_explicit` / `atomic_compare_exchange_weak_explicit` using `memory_order_seq_cst` or the user-supplied order. Never access `*object` via a non-atomic dereference.
- **Uses the internal hash table**: **must bypass** on Windows for all operand sizes (1–8 bytes) — the Windows backend never uses the hash table. On Linux, FreeBSD, and macOS, bypass only for 4-byte and 8-byte inputs (1-byte and 2-byte use hash table). The implementation must call the kernel primitive directly; for 1-byte and 2-byte inputs on Windows, it cannot delegate to `atomic_wait_expected` (which only accepts native-width 4-byte `C`), so `atomic_wait` must contain its own `WaitOnAddress` call for those widths. When bypassing, no `wait_queue_t` node is allocated and the hash table is not touched.
- **Uses the internal hash table**: **must use** the hash table on the pthreads backend regardless of size (no kernel tracker), and on Linux/macOS/FreeBSD for sub-4-byte types. Must also fall back when `object` is misaligned for the platform primitive. Look up `object`'s wait queue, append the current thread to it, and hold the hash table lock while enqueuing before parking.

**MUST NOT:**
- Return a status code. The proposal synopsis declares these functions `void`.
- Spurious wake the thread and treat it as a notification. A spurious wake must trigger the re-compare-and-re-park loop, not early return.

**MAY:**
- Implement a brief spin loop before parking, to handle the case where the notifying store is very close in time to the wait (reduces context-switch overhead for high-contention objects).
- Cache the loaded value from the park return and use it directly in the next loop iteration (avoids an extra load), but must reload from `*object` if the park returned due to timeout or spurious wake.

---

### `atomic_notify_one` / `atomic_notify_all` (void)

**MUST:**
- Both must be implemented as thin wrappers around an internal helper `wake_waiters(void *object, unsigned max)` that performs the actual walk of the wait queue and issues the platform wake primitive. They **must not** call `atomic_notify` (which includes a CAS).
- `atomic_notify_one(object)` calls `wake_waiters(object, 1)` and discards the return value.
- `atomic_notify_all(object)` calls `wake_waiters(object, UINT_MAX)` (or an equivalent "all" sentinel) and discards the return value.
- The wakeup must establish the memory synchronization described in the proposal: all side effects that happen before the notify synchronize with the waiting thread before the wait returns.
- **Uses the internal hash table**: **must bypass** on Windows for all operand sizes (1–8 bytes) — the Windows backend never uses the hash table. On Linux, FreeBSD, and macOS, bypass only for 4-byte and 8-byte inputs. Implement as thin wrappers around `wake_waiters(object, max)` only when using the hash table path; on Windows, call `WakeByAddressSingle` / `WakeByAddressAll` directly without any `wake_waiters` helper. On the pthreads backend, or when `sizeof(*object) < 4` or `object` is misaligned, **must use** the hash table: look up `object`'s wait queue, hold the lock, and call `wake_waiters` to walk/wake.

**MUST NOT:**
- Call `atomic_notify` internally (avoiding the unnecessary CAS and its failure semantics).
- Dereference `object` directly; use `atomic_load_explicit` only for any pre-wake load checks.
- Wake threads that are not currently suspended in `atomic_wait` on `object`.
- Block or spin indefinitely if no waiters exist — return immediately.

**MAY:**
- Acquire the hash table lock briefly during the walk of the wait queue; this is required to safely manipulate the queue.
- Use the platform-native wake primitive in a loop if the primitive wakes fewer threads than expected (e.g., `FUTEX_WAKE` returns the number woken; if it returns less than the queue length, call it again).

---

### `atomic_wait_expected` (returns int)

**MUST:**
- Return positive if the calling thread was suspended at least once (regardless of subsequent timeout or notification).
- Return zero if no suspension occurred, or if the accumulated time in the function reached `*duration` before any suspension (i.e., timed out on the first attempt).
- Return negative on error (e.g., invalid `duration` or system call failure).
- On all return paths, set `*expected` to the value of `*object` observed by the most recent load before returning.
- Use ceiling conversion from `*duration` (`struct timespec`) to the platform timeout, ensuring total accumulated wait is at least `*duration`.
- Use the `success` memory order for accesses that happen after a notify successfully wakes the thread, and `failure` for the initial load-compare and any timeout path.
- **Uses the internal hash table**: **must bypass** on Windows, macOS, and FreeBSD (4-byte via `UMTX_OP_WAIT_UINT` or 8-byte via `UMTX_OP_WAIT`); on Linux, bypass only for 4-byte inputs (Linux `FUTEX_WAIT` is 32-bit only, `int *uaddr` / `int val`). Call the kernel primitive directly on the address of the value at `object`; the kernel internally tracks waiters by address, so no user-space queue is needed for the suspend/resume path. When bypassing, `atomic_wait_expected` does not allocate a `wait_queue_t` node or touch the hash table at all.
- **Uses the internal hash table**: **must use** the hash table on the pthreads backend (`atomic_wait_pthreads.c.ipp`), because `pthread_cond_wait` has no kernel-level per-address wait tracker, and on Linux for 8-byte inputs (no 64-bit futex). Must also fall back to the hash table on any backend when `sizeof(C) < 4` or `object` is misaligned for the platform primitive: look up `object`'s wait queue, append the current thread, and hold the lock while enqueuing before parking. Relies on the same `wake_waiters` helper used by `atomic_notify_one` / `atomic_notify_all` to dequeue waiters on notify.
- **Type constraint**: `A` must be `_Atomic(C)` for some unsigned integer type `C` where `sizeof(C) == sizeof(uint_native_wait_notify_t)`. `C` need not be exactly `uint_native_wait_notify_t`; any unsigned integer type of the same size is valid (e.g., `uint32_t`, `uint64_t`, `uint_fast32_t` on a platform where `sizeof(uint_native_wait_notify_t) == 4`). The implementation must access `*object` using the platform primitive's native width (the width of `uint_native_wait_notify_t`), not the exact type of `C`.

**MUST NOT:**
- Return positive if the thread was never parked, even if the call succeeded nominally.
- Return without updating `*expected` on any path, including error and timeout.
- Use floor conversion for `timespec` → platform timeout; a floor-converted duration shorter than `*duration` violates the proposal wording.

**MAY:**
- Use the platform's absolute-time timeout syscall (Linux `FUTEX_WAIT`, FreeBSD `_umtx_time_spec`) if it avoids drift from repeated relative-time sleeps.
- On macOS, cap `ulock_wait` nanoseconds per call at `UINT32_MAX` and loop; the proposal allows accumulating time across multiple park calls.

---

### `atomic_notify` (returns int)

**MUST:**
- Perform `atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure)` and proceed only on success.
- On CAS success, call the same `wake_waiters(object, max_threads_to_wake)` helper used by `atomic_notify_one` / `atomic_notify_all` to unblock threads. Do not duplicate the wake-queue walk logic.
- Return a positive number (possibly `1 + N` platform-dependent) on successful exchange and notify.
- Return zero if the CAS fails (memory ordering = `failure`).
- Return negative on error.
- Set `*expected` to the value observed by the CAS on all return paths.
- **Uses the internal hash table**: **must bypass** on Windows, macOS, and FreeBSD (using `UMTX_OP_WAKE` for 8-byte or `UMTX_OP_WAKE_UINT` for 4-byte); on Linux, bypass only for 4-byte (Linux `FUTEX_WAKE` is 32-bit only, `int *uaddr` / `int val`). After the CAS succeeds, call the kernel wake primitive directly on `object`'s address up to `max_threads_to_wake` times. On Windows, `WakeByAddressSingle` wakes one thread per call; loop up to `max_threads_to_wake` times or until `GetLastError() == ERROR_TIMEOUT`. When bypassing, no hash table lookup or lock is needed after the CAS.
- **Uses the internal hash table**: **must use** the hash table on the pthreads backend (`atomic_wait_pthreads.c.ipp`), because `pthread_cond_signal` / `pthread_cond_broadcast` has no kernel-level per-address wait tracker, and on Linux for 8-byte inputs (no 64-bit futex). Must also fall back to the hash table after CAS success on any backend when bypass conditions are not met: look up `object`'s wait queue and hold the lock while walking/waking up to `max_threads_to_wake` threads via `wake_waiters`.
- **Type constraint**: `A` must be `_Atomic(C)` for some unsigned integer type `C` where `sizeof(C) == sizeof(uint_native_wait_notify_t)`. `C` need not be exactly `uint_native_wait_notify_t`; any unsigned integer type of the same size is valid. The `atomic_compare_exchange_strong_explicit` must operate on the full width of `uint_native_wait_notify_t` (not `sizeof(C)` bytes if C is narrower).

**MUST NOT:**
- Exceed `max_threads_to_wake` waked threads. If the wait queue has more waiters than `max_threads_to_wake`, leave the excess parked.
- Wake threads waiting on a different atomic object.
- Return positive without performing a successful CAS first.

**MAY:**
- Add a platform-dependent `+1` to the return value on some platforms (the proposal explicitly allows "possibly one plus the number of threads woken on some platforms").
- Use a fast-path check before acquiring the hash table lock: if `max_threads_to_wake == 0`, skip the hash table lookup and `wake_waiters` call entirely and return 0 immediately (no CAS, no notify, no side effects).
- Share the `wake_waiters` implementation with `atomic_notify_one` / `atomic_notify_all` to avoid code duplication.

**MAY:**
- Add a platform-dependent `+1` to the return value on some platforms (the proposal explicitly allows "possibly one plus the number of threads woken on some platforms").
- Use a fast-path check before acquiring the hash table lock: if `max_threads_to_wake == 0`, skip the hash table lookup entirely and return 0 immediately (no CAS, no notify, no side effects).

---

### Step 8 — `include/wg14_atomic_waits/detail/impl/atomic_wait_linux.c.ipp`

Linux-only platform implementation includes `atomic_wait_common.ipp.ipp` plus:

- **Linux futex-based park/unpark** (`SYS_futex` via `syscall(2)`): `FUTEX_WAIT` for park, `FUTEX_WAKE` for notify. The futex operates on a 32-bit `int` at the address derived from `object` (e.g., `(int *)(uintptr_t)object`). **Linux futex is 32-bit only**: it cannot directly wait on 8-byte atomics; 8-byte inputs on Linux must fall back to the hash table. Gate on `__linux__` at the top of the file; this file must only ever be compiled on Linux.
- Wrap raw syscall calls in `extern "C"` block matching `wg14_signals` pattern.
- Preserve errno across library calls (save to `save_errno` before any library call, restore before return).
- Only `atomic_wait_linux.c` is compiled as a source TU; the `.ipp` may be included in the header-only path. All internal helper functions in the `.ipp` must be `static inline` so they do not violate the ODR when included into multiple TUs.

---

### Step 9 — `include/wg14_atomic_waits/detail/impl/atomic_wait_freebsd.c.ipp`

FreeBSD-only platform implementation includes `atomic_wait_common.ipp.ipp` plus:

- **FreeBSD `_umtx_time_spec`-based park/unpark**: Use `UMTX_OP_WAIT_UINT` / `UMTX_OP_WAKE_UINT` for 4-byte inputs, or `UMTX_OP_WAIT` / `UMTX_OP_WAKE` (native-width `long`) for 8-byte inputs, via the `_umtx_time_spec` syscall. This is FreeBSD's native wait/notify primitive, analogous to Linux's `SYS_futex`. Wait on the `uint32_t` value at the address derived from `object`; wake on notify.
- Gate on `#ifdef __FreeBSD__` and `#if __FreeBSD_version >= 1200000` (FreeBSD 12+) at the top of the file; add a compile-time `#error` if the version is older, since `_umtx_time_spec` / `UMTX_OP_WAIT_UINT` / `UMTX_OP_WAKE_UINT` require FreeBSD 12+. Includes `<sys/umtx.h>`.
- Wrap raw syscall calls in `extern "C"` block matching `wg14_signals` pattern.
- Preserve errno across library calls (save to `save_errno` before any library call, restore before return).
- Only `atomic_wait_freebsd.c` is compiled as a source TU; the `.ipp` may be included in the header-only path. All internal helper functions in the `.ipp` must be `static inline` so they do not violate the ODR when included into multiple TUs.
- **Native-width selection**: FreeBSD provides `UMTX_OP_WAIT` (native-width, `long`) and `UMTX_OP_WAIT_UINT` (4-byte, `uint32_t`). The FreeBSD backend must select the appropriate operation at runtime based on `sizeof(*object)`: use `UMTX_OP_WAIT` / `UMTX_OP_WAKE` when `sizeof(*object) == sizeof(long)` (8-byte on LP64), and `UMTX_OP_WAIT_UINT` / `UMTX_OP_WAKE_UINT` when `sizeof(*object) == 4`. Because FreeBSD supports both widths, it can always avoid the hash table for native-width inputs; the hash table fallback is only needed for sub-native-width types (1 or 2 bytes).

---

### Step 10 — `include/wg14_atomic_waits/detail/impl/atomic_wait_pthreads.c.ipp`

Non-Linux non-FreeBSD POSIX (pthread) platform implementation includes `atomic_wait_common.ipp.ipp` plus:

- **pthread-based park/unpark**: Use `pthread_cond_wait` / `pthread_cond_timedwait` + `pthread_mutex_t` + a per-object condition variable. For timed waits, use `pthread_cond_timedwait` with `CLOCK_MONOTONIC`. The per-object CV and mutex are allocated on first wait (protected by the global hash table lock).
- Gate on `#ifndef __linux__ && !defined(__FreeBSD__) && !defined(__APPLE__)` at the top of the file; this file is the pthread fallback for POSIX platforms without native wait/notify (e.g. OpenBSD, NetBSD, Solaris).
- Wrap raw pthread calls in `extern "C"` block matching `wg14_signals` pattern.
- Preserve errno across library calls (save to `save_errno` before any library call, restore before return).
- Only `atomic_wait_pthreads.c` is compiled as a source TU; the `.ipp` may be included in the header-only path. All internal helper functions in the `.ipp` must be `static inline` so they do not violate the ODR when included into multiple TUs.

---

### Step 11 — `include/wg14_atomic_waits/detail/impl/atomic_wait_macos.c.ipp`

macOS-specific implementation includes `atomic_wait_common.ipp.ipp` plus:

- **`ulock`-based park/unpark**: Use `ulock_wait` / `ulock_wake` from `<bsd/sys/ulock.h>`. The address passed is `object`. `ulock` is the native lightweight wait/notify primitive on macOS/iOS and is preferred over pthread condition variables for this use case. `ulock_wait`'s timeout is a `uint32_t` in **nanoseconds**; the implementation must convert `*duration` (a `struct timespec`) to nanoseconds, capping at `UINT32_MAX` (~4.29 seconds) per call and looping for longer timeouts.
- Gate on `#ifdef __APPLE__` and `#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200` (macOS 10.12+) / `#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000` (iOS 10+) at the top of the file; add a compile-time `#error` if the deployment target is older, since `ulock` is unavailable before macOS 10.12 / iOS 10. Add `#include <bsd/sys/ulock.h>`.
- Wrap raw ulock calls in `extern "C"` block matching `wg14_signals` pattern.
- Preserve errno across library calls (save to `save_errno` before any library call, restore before return).
- Only `atomic_wait_macos.c` is compiled as a source TU; the `.ipp` may be included in the header-only path. All internal helper functions in the `.ipp` must be `static inline` so they do not violate the ODR when included into multiple TUs.
- **64-bit support**: `ulock` provides both `UL_COMPARE_AND_WAIT` (4-byte operand) and `UL_COMPARE_AND_WAIT64` (8-byte operand). The macOS backend must select the appropriate operation at runtime based on `sizeof(*object)`: use `UL_COMPARE_AND_WAIT64` when `sizeof(*object) == 8`, `UL_COMPARE_AND_WAIT` when `sizeof(*object) == 4`. Because macOS supports both widths natively, it can always avoid the hash table for native-width inputs (4-byte and 8-byte); the hash table fallback is only needed for sub-native-width types (1 or 2 bytes).

---

### Step 12 — `include/wg14_atomic_waits/detail/impl/atomic_wait_windows.c.ipp`

Windows platform implementation includes `atomic_wait_common.ipp.ipp` plus:

- Uses `WaitOnAddress` (park, with `DWORD` millisecond timeout) / `WakeByAddressSingle` / `WakeByAddressAll` (Windows 8+, from `<synchapi.h>`). The address passed is `object`. `WaitOnAddress` supports `AddressSize` of `1`, `2`, `4`, or `8` bytes (per MSDN); the implementation passes `sizeof(*object)` as `AddressSize`. The address must be aligned appropriately for the operand size (e.g., 8-byte alignment for 8-byte atomics). Since `WaitOnAddress` supports the full native width on Windows, all native-width inputs bypass the hash table; only sub-native-width or misaligned accesses fall back. `WaitOnAddress`'s timeout parameter is a `DWORD` in **milliseconds** (not `timespec`); the implementation must convert `*duration` to milliseconds, capping at `INFINITE` (0xFFFFFFFF ≈ 49.7 days) for infinite waits. These APIs require linking against `Synchronization.lib` (they reside in `Kernel32.dll`). MSVC links `Kernel32.lib` by default, which transitively provides `Synchronization.lib`; no explicit additional library linking is needed in practice.
- At the top of the file, add a compile-time guard: `#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0602) #error atomic_wait_windows requires Windows 8 or later (_WIN32_WINNT >= 0x0602) #endif`. The library should also document that Windows 8 / Server 2012 is the minimum supported OS. If a runtime fallback for older Windows versions is desired (not in scope for this plan), `GetProcAddress` would be needed to load these symbols dynamically at runtime.
- `msvc` `#pragma warning(push/pop)` around any declarations that provoke warnings, matching `thrd_signal_handle_posix.c.ipp` pattern.
- Only `atomic_wait_windows.c` is compiled as a source TU; the `.ipp` may be included in the header-only path. All internal helper functions in the `.ipp` must be `static inline` so they do not violate the ODR when included into multiple TUs.

---

### Step 13 — `test/test_common.h`

Mirror `wg14_signals/test/test_common.h`; replace all `WG14_SIGNALS_*` with `WG14_ATOMIC_WAITS_*`.

- `#pragma once`
- `#include "wg14_atomic_waits/config.h"`
- `CHECK(x)` macro with `fprintf(stderr, ...)` failure reporting
- `#include <time.h>` (required for `timespec` manipulation in `atomic_wait_expected` tests)
- `<threads.h>` with pthread fallback for MacOS (same as wg14_signals)
- Declare a `thrd_sleep_ms(unsigned ms)` helper for tests that need to yield the processor briefly.
- Ensure the pthread fallback `<pthread.h>` path is used consistently across all POSIX-backend tests.

---

### Step 14 — Test files

All tests follow the `CHECK` pattern: `int ret = 0; ret += CHECK(...); return ret;`

Mark `benchmark_atomic_wait_test.c` with an `EXCLUDE_FROM_ALL`-style exclusion in `test/CMakeLists.txt` and gate `ctest` with `-E benchmark`.

| File | What it tests |
|---|---|
| `atomic_wait_test.c` | `atomic_wait`/`atomic_wait_explicit`: Thread A waits on atomic word in shared memory; Thread B changes value and notifies; A returns and asserts correct value. Also tests spurious wake-up: insert a notify before the thread has parked or inject a dummy notify during wait, and verify the loop re-compares and re-parks correctly. Uses `atomic_int` shared latch + pthreads (or `<threads.h>`). |
| `atomic_wait_expected_test.c` | `atomic_wait_expected` with `NULL` duration (infinite wait) and with real `timespec` duration (timeout). Verifies return value semantics: positive = suspended at least once, zero = no suspension or timed out, negative = error. Verifies `*expected` is updated on return to the value observed by the most recent load. On timeout, confirms `*expected` reflects the timed-out value. |
| `atomic_notify_test.c` | `atomic_notify_one` wakes exactly one waiter; `atomic_notify_all` wakes all; concurrent notify from multiple threads. Uses multiple waiting threads and a shared counter. Also tests `max_threads_to_wake` semantics (0, 1, N > waiters). Tests `atomic_notify` CAS-failure path returning 0. |
| `benchmark_atomic_wait_test.c` | High-contention benchmark: N producer threads notify; M consumer threads wait. Excluded from ctest (`-E benchmark`). |
| `header_only_test.cpp` | C++ compile test — defines `WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 1`, includes `<wg14_atomic_waits/atomic_wait.h>`, exercises the public API. |
| `header_only_test1.cpp` | Additional TU for the header-only test; exercises the header in a second translation unit to verify no ODR issues. |
| `header_only_test2.cpp` | Additional TU for the header-only test; exercises the header in a third translation unit. On Windows, includes the Windows `.ipp`; on macOS, includes the macOS `.ipp`; on Linux, includes the Linux `.ipp`; on FreeBSD, includes the FreeBSD `.ipp`; on other POSIX, includes the pthreads `.ipp`. |

---

### Step 15 — `test/CMakeLists.txt`

Mirror `wg14_signals/test/CMakeLists.txt`:

- `add_code_test(atomic_wait_test SOURCES "atomic_wait_test.c" FEATURES c_std_11)`
- `add_code_test(atomic_wait_expected_test SOURCES "atomic_wait_expected_test.c" FEATURES c_std_11)`
- `add_code_test(atomic_notify_test SOURCES "atomic_notify_test.c" FEATURES c_std_11)`
- `add_code_test(benchmark_atomic_wait_test SOURCES "benchmark_atomic_wait_test.c" FEATURES c_std_11)`
- `add_executable(header_only_test "header_only_test.cpp" "header_only_test1.cpp" "header_only_test2.cpp")`; compile features `c_std_11 cxx_std_11`, same include path as project
- Benchmark tests are registered via `add_code_test` but excluded from `ctest` with `-E benchmark`, matching `wg14_signals`
- Use the `add_code_test` / `add_code_example` helpers exported from the root `CMakeLists.txt`

---

### Step 16 — `.github/workflows/ci.yml`

Mirror `wg14_signals/.github/workflows/ci.yml` exactly: Linux/MacOS standard matrix `[11, 23]`, Windows standard matrix `[11, 17]`. C23 testing is appropriate because the library is a reference implementation of a C23 `<stdatomic.h>` extension.

```yaml
name: CI
on:
  push:
    branches: [main]
  pull_request:
  schedule:
  - cron: '0 0 1 * *'
jobs:
  Linux:   # gcc + clang, Debug+Release, C11+C23, shared ON+OFF
  MacOS:   # Debug+Release, C11+C23, shared ON+OFF
  Windows: # VS2022, Debug+Release, C11+C17, shared ON+OFF
```

---

### Step 17 — `Readme.md`

Follow `wg14_signals` convention:

- Single sentence description.
- Apache 2.0 license note.
- Build/usage instructions.
- Mention `HEADER_ONLY_BUILD` CMake option.
- Brief API summary referencing `atomic_wait.h`.
- Performance characteristics (futex-based on Linux; FreeBSD umtx-based on FreeBSD; `ulock`-based on macOS; pthread-based on other POSIX; `WaitOnAddress` on Windows).
- CI badge (mirror `wg14_signals` Readme.md pattern).

---

### Step 18 — `Doxyfile`

Copy from `wg14_signals/Doxyfile` and update `PROJECT_NAME` to `wg14_atomic_waits`.

---

## Concurrency / Correctness Constraints for Implementation Agent

1. **`atomic_wait` / `atomic_wait_explicit` return type**: These are `void` per the proposal synopsis. They block until a subsequent load observes a mismatch, then return without a status code.
2. **Spurious wake-ups**: The wait loop MUST re-compare `expected` after returning from park and re-park if still equal (required by proposal wording).
3. **`atomic_wait_expected` `*expected` update**: On return, `*expected` must contain the value observed by the most recent load before the function returns. Implement via a re-load after every park return and before the final return.
4. **`atomic_wait_expected` timed-wait conversion**: Convert `*duration` (a `struct timespec`) to the platform timeout using a **ceiling conversion** (round up, not down), ensuring the total accumulated wait is at least `*duration`. On Linux/FreeBSD, the futex/umtx timeout syscall uses absolute time; the implementation must convert `timespec` to the platform's absolute time base (e.g., `CLOCK_MONOTONIC`). On Windows, cap converted milliseconds at `INFINITE` (0xFFFFFFFF). On macOS, cap nanoseconds at `UINT32_MAX` per `ulock_wait` call and loop for longer timeouts.
5. **`atomic_notify` return value**: Returns positive number of woken threads, or `1 + N` where the extra info depends on platform. Must not exceed `max_threads_to_wake`. Returns 0 if the CAS fails or no waiters are parked; negative on error.
6. **`atomic_uint_native_wait_notify_t`**: The header must expose `atomic_uint_native_wait_notify_t` typedef per §7.17.1 / §7.17.6 of `docs/proposal.md`.
7. **C11 `_Atomic` compatibility**: `object` is `volatile A *`. Do not access fields directly — use `atomic_load_explicit` / `atomic_compare_exchange_weak_explicit` only. The `volatile` qualifier is required by the proposal.
8. **Platform detection in `config.h`**: The Linux backend (`atomic_wait_linux.c.ipp`) is Linux-only and uses futexes (`SYS_futex`). The FreeBSD backend (`atomic_wait_freebsd.c.ipp`) is FreeBSD-only and uses `_umtx_time_spec` with `UMTX_OP_WAIT` (native-width, `long`) or `UMTX_OP_WAIT_UINT` (4-byte, `uint32_t`) selected at runtime based on `sizeof(*object)`. The pthreads backend (`atomic_wait_pthreads.c.ipp`) is the fallback for non-Linux non-FreeBSD POSIX (e.g. OpenBSD, NetBSD, Solaris) and uses pthread condition variables. The macOS backend (`atomic_wait_macos.c.ipp`) is Darwin-only and uses `ulock_wait`/`ulock_wake` (`<bsd/sys/ulock.h>`) with `UL_COMPARE_AND_WAIT` (4-byte) or `UL_COMPARE_AND_WAIT64` (8-byte) selected at runtime based on `sizeof(*object)`. macOS can always avoid the hash table for native-width inputs (4-byte and 8-byte); the hash table fallback is only needed for sub-native-width types (1 or 2 bytes). The header-only include block in `atomic_wait.h` selects the correct backend via `#if defined(_WIN32) || defined(_WIN64)` / `#elif defined(__linux__)` / `#elif defined(__FreeBSD__)` / `#elif defined(__APPLE__)` / `#else`.
9. **Header-only compilation**: Ensure all `#if WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY` blocks are placed *after* the `extern "C"` closing `}` and *before* the final `#endif` of the include guard, matching `thrd_signal_handle.h:463-469`.
10. **C++ compatibility**: `atomic_wait.h` must compile cleanly under both C11 and C++11 (matching wg14_signals C11-only standard, but `extern "C"` for C++ linkage compatibility).
11. **Include `time.h`**: `atomic_wait.h` and `test_common.h` must include `<time.h>` because `atomic_wait_expected` takes a `const struct timespec *restrict duration`.

---

## Validation Plan

After implementation, verify with:
```bash
cd /var/home/ned/boostish/wg14_atomic_waits
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_STANDARD=11
cmake --build . --parallel
ctest --output-on-failure --timeout 300 -E benchmark
# Verify header-only build:
cd .. && rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_STANDARD=11 -DHEADER_ONLY_BUILD=ON
cmake --build . --parallel
ctest --output-on-failure --timeout 300 -E benchmark
# Format check:
cd .. && clang-format -n include/wg14_atomic_waits/*.h include/wg14_atomic_waits/detail/impl/*.ipp include/wg14_atomic_waits/detail/impl/*.h src/wg14_atomic_waits/*.c test/*.c
```

---

## Out of Scope (explicitly marked)

- No `sigguarded()`-style signal guard API (belongs to `wg14_signals`).
- No Windows structured exception handling chain (no OS signals involved in atomic waits).
- No Doxygen customisation beyond copying wg14_signals `Doxyfile`.
- No Rust or other language bindings.
- **Do NOT use C++** in any source or header file under `include/` or `src/` (per AGENTS.md rule 1). C++ is permitted only in `test/` for compile-testing the public header.
- No `verstable.h` (the async-signal-safe TLS hash table from `wg14_signals`). This library implements its own small, purpose-built inline open-addressing hash table in `atomic_wait_common.ipp.ipp`; verstable.h's generic template machinery, 11-bit displacement metadata, and tombstone-free deletion strategy are not needed here.
- No runtime fallback for pre-Windows 8 (pre-Win8 lacks `WaitOnAddress`/`WakeByAddressSingle`/`WakeByAddressAll`; a `GetProcAddress` fallback is out of scope). Minimum supported Windows version is Windows 8 / Server 2012.

---

## Acceptance Criteria

- [ ] `cmake --build . && ctest` passes all non-benchmark tests on Linux (gcc, clang), macOS, and Windows.
- [ ] `HEADER_ONLY_BUILD=ON` builds and tests pass, including separate-TU compile test.
- [ ] All public API names are wrapped in `WG14_ATOMIC_WAITS_PREFIX(...)`.
- [ ] Directory layout is a byte-for-byte structural match of `wg14_signals` at the corresponding level.
- [ ] `clang-format -n` shows no formatting issues on any header or source file.
- [ ] `AGENTS.md` content is present and includes the C++ test carve-out rule, matching wg14_signals style.
