# Reference implementation for proposed WG14 atomic wait/notify extension to standard C

(C) 2026 Niall Douglas [http://www.nedproductions.biz/](http://www.nedproductions.biz/)

CI: [![CI](https://github.com/ned14/wg14_atomic_waits/actions/workflows/ci.yml/badge.svg)](https://github.com/ned14/wg14_atomic_waits/actions/workflows/ci.yml)

Reference API docs: https://ned14.github.io/wg14_atomic_waits/

Implements the proposal for `atomic_wait`/`atomic_notify` for C11/C23 `<stdatomic.h>`. Can be configured to be a standard library implementation for your standard C library runtime. Licensed permissively. Features:

- Header-only or compiled library.
- Windows uses `WaitOnAddress`/`WakeByAddressSingle`/`WakeByAddressAll`.
- macOS/iOS uses `ulock_wait`/`ulock_wake`.
- Linux uses `SYS_futex`.
- FreeBSD uses `_umtx_op` (`UMTX_OP_WAIT_UINT` for 4-byte, `UMTX_OP_WAIT` for 8-byte).
- Other POSIX falls back to `pthread_cond_wait`/`pthread_cond_signal`.

## Example of use

```c
#include <wg14_atomic_waits/atomic_wait.h>
#include <stdatomic.h>

// Blocks until *value no longer equals *expected (or until the duration
// expires). On return *expected is updated to the observed value, and the
// return value is positive if the thread actually suspended at least once.
int worker(atomic_int *value)
{
  int expected = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  value,                       // object whose value is compared and waited on
  &expected,                   // expected value; *expected is updated on return
  NULL,                        // duration: NULL waits forever, else a timespec
  memory_order_seq_cst,        // success order: the load that ends the wait
  memory_order_seq_cst         // failure order: the load on timeout or retry
  );
  return expected;
}

// Atomically stores *value = desired (provided it still equals *expected) and
// wakes up to max_threads_to_wake parked waiters in a single operation.
// Returns a positive value on success (1 + the number woken on some
// platforms), 0 if the compare-exchange failed, or a negative error.
int set_value(atomic_int *value)
{
  int expected = 0;
  return WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
  value,                       // object whose value is compared and exchanged
  &expected,                   // expected value; updated on a failed exchange
  1,                           // desired: value stored on a successful exchange
  1,                           // max_threads_to_wake: waiters to wake at most
  memory_order_seq_cst,        // success order: the successful exchange
  memory_order_seq_cst         // failure order: the failed exchange
  );
}
```

Without `WG14_ATOMIC_WAITS_PREFIX(...)`, you can simply include `<wg14_atomic_waits/atomic_wait.h>` directly if not including `<stdatomic.h>` from C23.

## Building and using

### Header-only

To use the library without compiling anything, add `include/` to your include
path and define `WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY` before including
`<wg14_atomic_waits/atomic_wait.h>`. The header then pulls in the
platform-specific implementation itself and works from both C11 and C++11
translation units:

```c
#define WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 1
#include <wg14_atomic_waits/atomic_wait.h>
```

To force the portable pthreads backend in header-only mode instead of the native
platform backend, additionally define `WG14_ATOMIC_WAITS_ALWAYS_USE_PTHREADS`
(equivalent to the CMake `-DALWAYS_USE_PTHREADS_BACKEND=ON` option, which defines
it for you):

```c
#define WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 1
#define WG14_ATOMIC_WAITS_ALWAYS_USE_PTHREADS 1
#include <wg14_atomic_waits/atomic_wait.h>
```

When using the CMake `-DHEADER_ONLY_BUILD=ON` build this macro is defined for you.

### CMake

As a subdirectory or via `FetchContent`:

```cmake
add_subdirectory(wg14_atomic_waits)   # or FetchContent_Declare/Populate
target_link_libraries(myapp PRIVATE wg14_atomic_waits)
```

Or install it and use the exported package target:

```sh
cmake -S . -B build
cmake --build build --parallel
cmake --install build
```

```cmake
find_package(wg14_atomic_waits CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE wg14_atomic_waits::wg14_atomic_waits)
```

Relevant CMake options:

| Option | Default | Effect |
|---|---|---|
| `BUILD_SHARED_LIBS` | `OFF` | Build a shared library instead of a static one. |
| `HEADER_ONLY_BUILD` | `OFF` | Header-only "unity" build; defines `WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY`. |
| `ALWAYS_USE_PTHREADS_BACKEND` | `OFF` | Force the portable pthreads backend on every platform (also in header-only mode, via `WG14_ATOMIC_WAITS_ALWAYS_USE_PTHREADS`). |
| `BUILD_TESTING` | `ON` | Build the test suite (only when this is the top-level project). |
| `CMAKE_C_STANDARD` | `11` | C standard; C23 (`23`) is also supported. |

### Building and running the tests

```sh
cmake -S . -B build
cmake --build build --parallel
cd build && ctest --output-on-failure --timeout 300 -E benchmark
```

The benchmark target is `EXCLUDE_FROM_ALL`; build and run it explicitly:

```sh
cmake --build build --target benchmark_atomic_wait_test
./build/bin/benchmark_atomic_wait_test
```

## Backends and native widths

Which widths take the native fast path and which go through the per-process
hash-table fallback:

| Platform | Primitive | Native widths | Fallback widths |
|---|---|---|---|
| Windows (x64) | `WaitOnAddress` / `WakeByAddress*` | 1, 2, 4, 8 | (8 on 32-bit Windows) |
| macOS / iOS | `__ulock_wait` / `__ulock_wake` | 4, 8 | 1, 2 |
| Linux | `SYS_futex` | 4 | 1, 2, 8 |
| FreeBSD | `_umtx_op` | 4, 8 | 1, 2 |
| Other POSIX | `pthread_cond_wait` / `pthread_cond_signal` | — | 1, 2, 4, 8 |

Semantics notes:

- `atomic_wait_expected` and `atomic_notify` are defined for the native 4-byte
  `uint_native_wait_notify_t` type only.
- `duration` is a relative remaining interval measured against a monotonic
  clock, or `NULL` for no timeout. macOS, Windows and pthreads consume it
  directly; Linux converts it to an absolute CLOCK_MONOTONIC deadline for
  `FUTEX_WAIT_BITSET`; FreeBSD leaves the `UMTX_ABSTIME` flag clear so the kernel
  treats it as relative.
- `atomic_notify_all` always wakes every parked waiter: Windows uses
  `WakeByAddressAll`, macOS uses the wake-all flag, Linux and FreeBSD clamp the
  wake count, pthreads broadcasts.
- `atomic_notify` returns a positive value (on some platforms 1 plus the number
  of threads woken) on a successful exchange, `0` when the compare-exchange
  fails, and a negative `-errno` on error.

## Supported targets

This library should work well on any POSIX implementation, as well as Microsoft Windows. You will need a minimum of C 11 in your toolchain.

Current CI test targets:

- Ubuntu Linux, x64: GCC and clang, Debug and Release, C11 and C23, static and
  shared, native and pthreads backends, all under AddressSanitizer + UBSan.
- Mac OS, AArch64: clang, Debug and Release, C11 and C23, static and shared,
  native and pthreads backends, all under AddressSanitizer + UBSan.
- Microsoft Windows, x64 (MSVC VS2022): Debug and Release, C11 and C17, static
  and shared, under AddressSanitizer.
- FreeBSD, x64: FreeBSD 15 clang (real FreeBSD VM on an Ubuntu runner via
  `vmactions/freebsd-vm`), Release, C11 and C23, native and pthreads backends,
  under AddressSanitizer + UBSan.
- Header-only (`-DHEADER_ONLY_BUILD=ON`): all three runners, C11 and C23
  (C17 on MSVC); the header-only + `ALWAYS_USE_PTHREADS_BACKEND` combination is
  additionally run on Linux and macOS at both C11 and C23.
- ThreadSanitizer: Ubuntu (GCC and clang) and Mac OS (clang), C11 and C23.
- Fil-C memory-safe compiler: Ubuntu, C11, native and pthreads backends, via
  `cmake/filc-toolchain.cmake`.

Current compilers:

- GCC
- clang
- MSVC
- Fil-C (memory-safe clang derivative)

## Configuration

You can find a number of user definable macros to override in `config.h`.
They have sensible defaults on the major platforms and toolchains.

The only one to be especially aware of is `WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY`.
When set to `1`, the library operates as header-only by including the
platform-specific implementation directly within the header.

## Performance

On Linux with `SYS_futex` for native-width 4-byte types and the hash table
fallback for other widths, `atomic_wait()` is only moderately more expensive
than a futex on the same architecture. The same holds for the native 4/8-byte
paths on macOS and FreeBSD and for all widths on Windows; the hash-table
fallback widths (1/2/8-byte on Linux, 1/2-byte on macOS/FreeBSD, all widths on
the pthreads backend) additionally pay a hash-table lookup and a per-process
proxy object on top of the underlying wait primitive.

### Empirical results

All figures come from `benchmark_atomic_wait_test` (see "Reproduce" below): a
ping-pong suspend-wake benchmark in which one producer and one consumer
exchange a shared counter. The consumer parks in `atomic_wait` (a genuine
kernel suspend), the producer busy-settles so the park has reached the kernel,
then stores a new value and notifies. Every wake latency is timed on the
consumer from the producer's notify timestamp (ns-resolution monotonic clock)
to its own return from `atomic_wait`, which excludes the settle.

Because suspend-wake latencies are skewed (CPU idle-exit, scheduler and
frequency effects pile onto the right tail), each run records 500,000 wake
latencies per path and reports percentiles rather than a mean; the tabulated
figures are the mean over 10 runs of the per-run p50/p90/p99/p99.9. A fixed
settle is not portable: if it is too short for the consumer's park to reach
the kernel before each notify, producer and consumer fall into phase-locked
orbits and the reported figure swings with the run duration. The benchmark
therefore calibrates the settle at run time, raising it from an 8 µs floor
until at least 99.9% of notifies catch a genuinely parked consumer (the
"calibration" lines, and the per-run "suspend fraction", both printed by the
binary), and measures both paths at the same calibrated settle so the CPU
idle-state effects of the park duration cannot favour one path. On this host
the settle calibrated to 8–10 µs in 9 of 10 runs (20 µs in the remaining run,
which landed in an external load burst), and the measured suspend fraction was
≥ 99.6% in every run.

The two measured paths are the 4-byte native fast path and the 1-byte width,
which on the POSIX backends is served by the hash-table fallback (see
"Backends and native widths"). On Windows every width is native
(`WaitOnAddress`), so its fallback column measures the native 1-byte path
instead.

| Machine | OS | Toolchain | Native fast path (4-byte), p50/p90/p99/p99.9 | Hash-table fallback (1-byte), p50/p90/p99/p99.9 |
|---|---|---|---|---|
| Apple M3 Pro (12 cores, 18 GB) | macOS 15.7.7 (Darwin 24.6.0, arm64) | Apple clang 17.0.0, `Release` | unknown — pending re-run with the new benchmark | unknown — pending re-run with the new benchmark |
| AMD Ryzen Threadripper PRO 5975WX (32 cores, 130 GB) | Fedora 44 (Aurora, Linux 7.0.12, x86_64) | GCC 16.1.1, `Release` (`-O3`) | 3.57 / 4.14 / 4.72 / 7.17 µs | 3.73 / 4.32 / 5.01 / 8.02 µs |

On the Threadripper host above the hash-table fallback is slower than the
native fast path at every percentile in every one of the 10 runs: p50 by a
mean of +0.16 µs (per-run range +0.14 to +0.20 µs, against per-run p50 values
of 3.50–3.52 µs native and 3.65–3.67 µs fallback in the 9 settle-8/10 µs
runs), and p99.9 by a mean of +0.84 µs. The growth from the ~0.16 µs median
cost to the ~0.84 µs 99.9th-percentile cost is consistent with the fallback's
per-cycle hash-table lookup, proxy reference counting, and malloc/free of the
proxy on top of the same kernel suspend-wake primitive: the steady-state
overhead is small and nearly constant, while the occasional scheduler-visible
contention (the proxy is removed and recreated under the hash-table lock every
cycle) lands in the tail.

Figures measured with earlier versions of the benchmark are not comparable
with the rows above: the old benchmark used a fixed 3 µs settle and a
µs-resolution clock, and on fast x86_64 its settle was too short for the
consumer to reliably reach the kernel before each notify, which made the
reported mean swing with the run duration (and could even invert the
native/fallback ordering, as no code-level difference can make the fallback
faster). The macOS row therefore awaits re-running the current benchmark on
that machine.

Reproduce with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmark_atomic_wait_test --parallel
# A full run calibrates each path and then records 500,000 wake latencies per
# path; allow ~25 s. On Linux, pinning the run to a handful of dedicated CPUs
# (taskset -c 0,2,4,6) keeps the figures tightest; the tabulated x86_64 row
# was measured that way.
./build/bin/benchmark_atomic_wait_test
```
