# Reference implementation for proposed WG14 atomic wait/notify extension to standard C

(C) 2026 Niall Douglas [http://www.nedproductions.biz/](http://www.nedproductions.biz/)

CI: [![CI](https://github.com/ned14/wg14_atomic_waits/actions/workflows/ci.yml/badge.svg)](https://github.com/ned14/wg14_atomic_waits/actions/workflows/ci.yml)

Reference API docs: https://ned14.github.io/wg14_atomic_waits/

Implements the proposal for `atomic_wait`/`atomic_notify` for C11/C23 `<stdatomic.h>`. Can be configured to be a standard library implementation for your standard C library runtime. Licensed permissively. Features:

- Header-only or compiled library.
- Windows uses `WaitOnAddress`/`WakeByAddressSingle`/`WakeByAddressAll`.
- macOS/iOS uses `ulock_wait`/`ulock_wake`.
- Linux uses `SYS_futex`.
- FreeBSD uses `_umtx_time_spec` (`UMTX_OP_WAIT_UINT` for 4-byte, `UMTX_OP_WAIT` for native-width).
- Other POSIX falls back to `pthread_cond_wait`/`pthread_cond_signal`.

## Example of use

```c
#include <wg14_atomic_waits/atomic_wait.h>
#include <stdatomic.h>

int worker(atomic_int *value)
{
  int expected = 0;
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(value, expected);
  return *value;
}

void set_value(atomic_int *value)
{
  atomic_store_explicit(value, 1, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(value);
}
```

Without `WG14_ATOMIC_WAITS_PREFIX(...)`, you can simply include `<wg14_atomic_waits/atomic_wait.h>` directly if not including `<stdatomic.h>` from C23.

## Supported targets

This library should work well on any POSIX implementation, as well as Microsoft Windows. You will need a minimum of C 11 in your toolchain.

Current CI test targets:

- Ubuntu Linux, x64.
- Mac OS, AArch64.
- Microsoft Windows, x64.

Current compilers:

- GCC
- clang
- MSVC

## Configuration

You can find a number of user definable macros to override in `config.h`.
They have sensible defaults on the major platforms and toolchains.

The only one to be especially aware of is `WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY`.
When set to `1`, the library operates as header-only by including the
platform-specific implementation directly within the header.

## Performance

On Linux with `SYS_futex` for native-width 4-byte types and the hash table
fallback for other widths, `atomic_wait()` is only moderately more expensive
than a futex on the same architecture.
