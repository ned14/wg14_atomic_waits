### Related documents

| number                                                              | Title                                  | Authors        | Remarks                   |
|---------------------------------------------------------------------|----------------------------------------|----------------|---------------------------|
| [N3886](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3886.pdf) | Working Draft                          |                | base for diff, 2026-05-24 |


# Proposed Wording

All changes are relative to N3886. 

### Legend

Deletions in the shown standard text are as shown <del>here</del>,
additions, as shown <ins>here</ins>. These may render differently
according to the style in which the document is shown by your browser,
but should always be well distinguishable. In the provided style there
are two visual distinctions:

- A high contrast color palete
  ([Okabe and Ito](https://jfly.uni-koeln.de/color/)) namely using colors
  black, <span style="color: #E69F00">orange</span>, <span
  style="color: #009E73">teal green</span> and <span
  style="background: #FFFFD0">light yellow</span>
- normal text, <s>strike through</s>, <u>underlining</u> and
  <tt>typewriter font</tt>.

Close to each other proposed changes <span style="color:
  #009E73"><u>resemble</u></span> <span style="color:
  #E69F00"><s>like</s></span> `this`.



## 7.17.1 Introduction, p5

* Amend p5 as follows:

> 5   The types include `memory_order`, which is an enumerated type whose enumerators identify
>     memory ordering constraints; `atomic_flag`, which is a structure type representing a
>     lock-free, primitive atomic flag; <del>and several atomic analogs</del><ins>
>     `uint_native_wait_notify_t`, which is the `uint_leastN_t` type with the smallest *N*, where
>     *N* is at least 32, for which atomic waiting and notifying operations have the least overhead
>     on this implementation; and several atomic analogs</ins> of integer types.

## To 7.17.6 Atomic integer types, p1, Table 7.6, append the row

| Type name                               | C type                                |
|-----------------------------------------|---------------------------------------|
| `atomic_uintmax_t`                      | `_Atomic uintmax_t`                   |
| <ins>`atomic_uint_native_wait_notify_t`</ins> | <ins>`_Atomic uint_native_wait_notify_t`</ins> |

## To 7.17.7.1 General, append:

> 1   This subclause describes several generic functions that operate on all atomic types other than
>     `atomic_flag`. Also, 7.17.7.6 provides such generic functions for some read-modify-write
>     operations on atomic integer types that are defined in addition to the operators defined in
>     6.5.3.5, 6.5.4.2 and 6.5.17.
>
> <ins>
> 2   The atomic_wait, atomic_wait_explicit, and atomic_wait_expected generic functions are
>     *waiting operations*, and the atomic_notify_one, atomic_notify_all, and atomic_notify generic
>     functions are *notifying operations*. The execution of a waiting operation on an atomic object
>     *M* can block the calling thread, in which case the thread is *blocked on M*. A thread blocked
>     on *M* becomes unblocked when it is *woken up*. A wakeup can be caused by a notifying operation
>     on *M* and can also occur for other reasons.
> </ins>

The following new subsections are inserted after 7.17.7.6 and before 7.17.8, and are numbered
7.17.7.7 through 7.17.7.11. The last paragraph of 7.17.7.6 is:

> **Returns**
>
> 4   Atomically, the value pointed to by `object` immediately before the effects.

## 7.17.7.7 The atomic_wait generic functions

<ins>

### Synopsis

```
#include <stdatomic.h>
void atomic_wait(const volatile A *object, C expected);
void atomic_wait_explicit(const volatile A *object, C expected, memory_order order);
```

### Description

1   Atomically checks the value pointed to by *object* as if by atomic_load(*object*) or
    atomic_load_explicit(*object*, *order*), respectively, and determines whether it compares equal to
    *expected* as if by the memcmp function. If it does not, the function returns immediately.
    Otherwise, the calling thread is blocked on the atomic object pointed to by *object* until it is
    woken up; if, when woken up, the value still compares equal to *expected*, the thread is blocked
    again.

2   NOTE 1 A wakeup can be triggered by a notifying operation on *object* or by an atomic store or
    exchange to *object*.

3   All side effects to the atomic object *M* that happen before a call *N* to a notifying operation on
    *M* synchronize with the completion of any waiting operation *W* on *M* that is unblocked by *N*.

</ins>

## 7.17.7.8 The atomic_notify_one generic functions

<ins>

### Synopsis

```
#include <stdatomic.h>
void atomic_notify_one(volatile A *object);
```

### Description

1   If prior to the call a thread has been blocked on the atomic object pointed to by *object* in a
    waiting operation and has not yet been woken up, at least one such thread is woken up,
    synchronizing as described in 7.17.7.7.

</ins>

## 7.17.7.9 The atomic_notify_all generic functions

<ins>

### Synopsis

```
#include <stdatomic.h>
void atomic_notify_all(volatile A *object);
```

### Description

1   If prior to the call threads have been blocked on the atomic object pointed to by *object* in
    waiting operations and have not yet been woken up, all such threads are woken up, synchronizing as
    described in 7.17.7.7.

</ins>

## 7.17.7.10 The atomic_wait_expected generic functions

<ins>

### Synopsis

```
#include <stdatomic.h>
int atomic_wait_expected(const volatile A *restrict object, C *restrict expected,
                         const struct timespec *restrict duration,
                         memory_order success, memory_order failure);
```

### Description

1   The sizeof(C) shall equal the sizeof(uint_native_wait_notify_t).

2   Atomically checks the value pointed to by *object* and determines whether it compares equal to
    *\*expected* as if by the memcmp function. If it does not, the function returns immediately.
    Otherwise, the calling thread is blocked on the atomic object pointed to by *object* until it is
    woken up; if, when woken up, the value still compares equal to the initial value of *\*expected*,
    the thread is blocked again.

3   NOTE 1 A wakeup can be triggered by a notifying operation on *object* or by an atomic store or
    exchange to *object*.

4   The total accumulated time during which the calling thread is blocked in this function shall be at
    least the interval pointed to by *duration* if *duration* is not a null pointer; after that time
    has elapsed, a timed-out result is returned as soon as is convenient. If *duration* is a null
    pointer, there are no time limits on when this function returns.

5   All side effects to the atomic object *M* that happen before a call *N* to a notifying operation on
    *M* synchronize with the completion of any waiting operation *W* on *M* that is unblocked by *N*.

### Returns

1   If the function returned immediately because the value did not compare equal to *\*expected*, or if
    a timed-out result is returned, the function returns zero and memory is affected according to the
    value of *failure*. If the calling thread was blocked at least once, the function returns a
    positive number and memory is affected according to the value of *success*. If the request could
    not be honored, the function returns a negative value.

2   In all cases, on return *\*expected* contains the value of *\*object* that caused the wait to exit;
    if a timed-out result is returned, *\*expected* is the same as on entry.

</ins>

## 7.17.7.11 The atomic_notify generic functions

<ins>

### Synopsis

```
#include <stdatomic.h>
int atomic_notify(volatile A *restrict object, C *restrict expected, C desired,
                  unsigned max_threads_to_wake, memory_order success, memory_order failure);
```

### Description

1   The sizeof(C) shall equal the sizeof(uint_native_wait_notify_t).

2   Performs as if by
    atomic_compare_exchange_strong_explicit(*object*, *expected*, *desired*, *success*, *failure*).
    If the result of that comparison is true, unblocks at least *max_threads_to_wake* threads that are
    blocked on the atomic object pointed to by *object* in waiting operations.

### Returns

1   If the exchange was performed, returns a positive number, possibly one plus the number of threads
    woken, and memory is affected according to the value of *success*. If the result of the comparison
    was false, returns zero and memory is affected according to the value of *failure*. If the request
    could not be honored, the function returns a negative value.

</ins>
