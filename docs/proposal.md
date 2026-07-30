# Proposed Wording

## 7.17.1 Introduction p5

The types include

`int_native_wait_notify_t`
`uint_native_wait_notify_t`

which are the smallest `int_leastN_t` and `uint_leastN_t` types where `N` is at least thirty-two for which atomic waits and notifies have least overhead on this implementation.

## To 7.17.6 Atomic integer types p1 append

`atomic_int_native_wait_notify_t`
`_Atomic int_native_wait_notify_t`

`atomic_uint_native_wait_notify_t`
`_Atomic uint_native_wait_notify_t`

## To subsection 7.17.7 (Operations on atomic types) append:

## 7.17.7.7 The `atomic_wait` generic functions

### Synopsis

```
#include <stdatomic.h>
void atomic_wait(const volatile A *object, C expected);
void atomic_wait_explicit(const volatile A *object, C expected, memory_order order);
```

### Description

Atomically checks the value pointed to by `object` as if by
`atomic_load(object)` or `atomic_load_explicit(object, order)`
respectively, if it compares equal to `expected` as if by a call to `memcmp()`.
If it does not, the function
returns immediately. Otherwise, the calling thread is suspended
until it is woken up by a call to `atomic_notify*(object)` or until
another wakeup occurs; if when woken up the value still compares equal to
`expected` the thread is suspended again.

**NOTE 1:** Note that a wakeup is possibly triggered by either a notifying
operation or by an atomic store or exchange.

All side effects to the atomic object `M` that happen before a call
  `N` to a notifying operation that unblocks a waiting operation `W`
  on `M` synchronize with the waiting thread before the waiting
  operation returns.

## 7.17.7.8 The `atomic_notify_one` generic functions

### Synopsis

```
#include <stdatomic.h>
void atomic_notify_one(volatile A *object);
```

### Description

If prior to the call a thread has been suspended by a waiting
  operation on `object` and not yet been woken up, at least one such
  thread is woken up, synchronizing as per the description of
  `atomic_wait()`.

## 7.17.7.9 The `atomic_notify_all` generic functions

### Synopsis

```
#include <stdatomic.h>
void atomic_notify_all(volatile A *object);
```

### Description

If prior to the call threads have been suspended by a waiting
  operation on `object` and not yet been woken up, all such
  threads are woken up, synchronizing as per the description of
  `atomic_wait()`.

## 7.17.7.10 The `atomic_wait_expected` generic functions

### Synopsis

```
#include <stdatomic.h>
int atomic_wait_expected(const volatile A *restrict object, C *restrict expected,
                         const struct timespec *restrict duration,
                         memory_order success, memory_order failure);
```

### Description

`sizeof(C)` must equal `sizeof(uint_native_wait_notify_t)`,
or this generic function shall be undefined.

If the atomic at `object` does not compare equal to `*expected`
as if by a call to `memcmp()`, returns immediately. If they were
equal, the calling thread is suspended
until it is woken up by a call to `atomic_notify*(object)` or until
another wakeup occurs; if when woken up the value still compares equal to the initial
value of `*expected` the thread is suspended again.

**NOTE 1:** Note that a wakeup is possibly triggered by either a notifying
operation or by an atomic store or exchange.

Total accumulated time in this function shall be at least `*duration`
if `duration` is not null, after which a timed out result is returned as
soon as is convenient. If `duration` is null, there are no time limits to when
this function returns.

All side effects to the atomic object `M` that happen before a call
  `N` to a notifying operation that unblocks a waiting operation `W`
  on `M` synchronize with the waiting thread before the waiting
  operation returns.

### Returns

If no thread suspension occurred or duration timeout occurs, returns zero and the memory synchronization
ordering will be `failure`. Otherwise,
if the calling thread was suspended at least once, returns a positive number and
the memory synchronization ordering will be `success`. If unsuccessful, this function returns a negative value.

In all cases, `*expected` on return was the value of `*object` when most
recently loaded -- in the case of duration timeout, it will be the same as on entry.

## 7.17.7.11 The `atomic_notify` generic functions

### Synopsis

```
#include <stdatomic.h>
int atomic_notify(volatile A *object, C *restrict expected, C desired,
                  unsigned max_threads_to_wake, memory_order success, memory_order failure);
```

### Description

`sizeof(C)` must equal `sizeof(uint_native_wait_notify_t)`,
or this generic function shall be undefined.

If `atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure)`
returns `true`, unblocks up to `max_threads_to_wake` threads blocked in `atomic_wait_*()`
for `object`.

### Returns

If `*object` was exchanged with `desired` and the atomic was successfully notified,
returns a positive number (possibly one plus the number of threads woken on some platforms)
and the memory synchronization ordering will be `success`. If `*object`
was not `*expected`, returns zero and the memory synchronization
ordering will be `failure`. If unsuccessful, this function returns a negative value.

In all cases, `*expected` on return was the value of `*object` when the atomic compare
exchange was performed.
