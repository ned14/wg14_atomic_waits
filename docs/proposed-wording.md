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

## 6.10.10.4 Conditional feature macros, p1

* Append after the entry for `__STDC_NO_ATOMICS__`:

> <ins>
> `__STDC_NO_ATOMIC_WAIT_TIMEOUT__` The integer literal 1, intended to indicate that the
> implementation does not support a non-zero finite duration in the `atomic_wait_expected`
> generic function.
> </ins>

## 7.17.1 Introduction, p3

* Amend p3 as follows:

> 3   The macro
>
>               __STDC_VERSION_STDATOMIC_H__
>
>     is an integer constant expression with a value equivalent to <del>202311L</del><ins>202611L</ins>.

## 7.17.1 Introduction, p4

* Amend p4 by appending after the `ATOMIC_FLAG_INIT` description:

>     which expands to an initializer for an object of type `atomic_flag`<del>.</del><ins>; and
>
>               STDC_ATOMIC_WAIT_FOREVER
>
>     which expands to `UINT_LEAST64_MAX`.</ins>

## 7.17.1 Introduction, p5

* Amend p5 as follows:

> 5   The types include `memory_order`, which is an enumerated type whose enumerators identify
>     memory ordering constraints; `atomic_flag`, which is a structure type representing a
>     lock-free, primitive atomic flag; <del>and several atomic analogs</del><ins>
>     `uint_native_wait_notify_t`, which is the `uint_leastN_t` type with the smallest *N*, where
>     *N* is at least 32, for which atomic waiting and notifying operations have the least overhead
>     on this implementation; and several atomic analogs</ins> of integer types.

## 7.17.6 Atomic integer types, p1, Table 7.6

| Type name                               | C type                                |
|-----------------------------------------|---------------------------------------|
| `atomic_uintmax_t`                      | `_Atomic uintmax_t`                   |
| <ins>`atomic_uint_native_wait_notify_t`</ins> | <ins>`_Atomic uint_native_wait_notify_t`</ins> |

## 7.17.7.1 General, append

> 1   This subclause describes several generic functions that operate on all atomic types other than
>     `atomic_flag`. Also, 7.17.7.6 provides such generic functions for some read-modify-write
>     operations on atomic integer types that are defined in addition to the operators defined in
>     6.5.3.5, 6.5.4.2 and 6.5.17.
>
> <ins>
> 2   The atomic_wait, atomic_wait_explicit, and atomic_wait_expected generic functions are
>     *waiting operations*, and the atomic_notify_one, atomic_notify_all, and atomic_notify generic
>     functions are *notifying operations*. A waiting operation on an atomic object *M* may block
>     the calling thread, in which case the thread is *blocked on M*. A thread blocked on *M*
>     becomes unblocked when it is *woken up*. A wakeup can be caused by a notifying operation on
>     *M* or can occur spuriously.
>
> 3   A call to a waiting operation on an atomic object *M* is eligible to be unblocked by a call to
>     a notifying operation on *M* if there exist side effects *X* and *Y* on *M* such that:
>
>     — the waiting operation has blocked after observing the result of *X*,
>
>     — *X* precedes *Y* in the modification order of *M*, and
>
>     — *Y* happens before the call to the notifying operation.
>
> 4   All side effects to the atomic object *M* that happen before a call *N* to a notifying
>     operation on *M* synchronize with the completion of any waiting operation *W* on *M* that is
>     unblocked by *N*.
>
> 5   NOTE 1 Programs are not guaranteed to observe transient atomic values, an issue known as the
>     A-B-A problem, resulting in continued blocking if a condition is only temporarily met.
>
> 6   NOTE 2 A waiting operation can return because it observes a value that differs from the
>     expected value without being unblocked by a notifying operation, for example when a store or
>     exchange to the atomic object occurs before the calling thread blocks. A store or exchange
>     alone is not sufficient to reliably unblock a thread that is already blocked on the atomic
>     object; a subsequent notifying operation is required to ensure that the thread is unblocked.
> </ins>

The following new subsections are inserted after 7.17.7.6 and before 7.17.8, and are numbered
7.17.7.7 through 7.17.7.11. Because these subsections are entirely new, they are shown in full
without further change markup. The last paragraph of 7.17.7.6 is:

> **Returns**
>
> 4   Atomically, the value pointed to by `object` immediately before the effects.

## 7.17.7.7 The atomic_wait generic functions

**Synopsis**

1

```
#include <stdatomic.h>
void atomic_wait(const volatile A *object, C expected);
void atomic_wait_explicit(const volatile A *object, C expected, memory_order order);
```

**Description**

2   The `order` argument shall not be `memory_order_release` nor `memory_order_acq_rel`.

3   The atomic object pointed to by `object` is denoted by *M*. The atomic_wait generic function is
    equivalent to the atomic_wait_explicit generic function with an `order` argument of
    `memory_order_seq_cst`.

4   The atomic_wait_explicit generic function repeatedly performs the following steps, in order:

    — evaluates `atomic_load_explicit(object, order)` and compares its value for equality with
      `expected`, as if by the `memcmp` function;

    — if the values compare unequal, returns;

    — blocks until it is unblocked by a notifying operation or is unblocked spuriously.

**Returns**

5   The atomic_wait and atomic_wait_explicit generic functions return no value.

## 7.17.7.8 The atomic_notify_one generic function

**Synopsis**

1

```
#include <stdatomic.h>
void atomic_notify_one(volatile A *object);
```

**Description**

2   The atomic_notify_one generic function unblocks at least one waiting operation on the atomic
    object pointed to by `object` that is eligible to be unblocked by this call, if any such
    waiting operation exists.

**Returns**

3   The atomic_notify_one generic function returns no value.

## 7.17.7.9 The atomic_notify_all generic function

**Synopsis**

1

```
#include <stdatomic.h>
void atomic_notify_all(volatile A *object);
```

**Description**

2   The atomic_notify_all generic function unblocks all waiting operations on the atomic object
    pointed to by `object` that are eligible to be unblocked by this call.

**Returns**

3   The atomic_notify_all generic function returns no value.

## 7.17.7.10 The atomic_wait_expected generic function

**Synopsis**

1

```
#include <stdatomic.h>
int atomic_wait_expected(const volatile A *restrict object, C *restrict expected,
                         uint_least64_t duration_ns, memory_order success,
                         memory_order failure);
```

**Description**

2   The value of `sizeof(C)` shall equal the value of `sizeof(uint_native_wait_notify_t)`.

3   The `success` argument shall not be `memory_order_release` nor `memory_order_acq_rel`. The
    `failure` argument shall not be `memory_order_release` nor `memory_order_acq_rel`.

4   The atomic object pointed to by `object` is denoted by *M*. Atomically, the value pointed to
    by `object` is compared for equality with the value pointed to by `expected`, as if by the
    `memcmp` function. If the values do not compare equal, the function updates the contents of the
    memory pointed to by `expected` with that pointed to by `object`, and memory is affected
    according to the value of `failure`. Otherwise, unless `duration_ns` is zero, the calling
    thread is blocked on *M* until it is woken up. If, when woken up, the value still compares
    equal to the value pointed to by `expected` on entry, the thread is blocked again.
    Otherwise, the function updates the contents of the memory pointed to by `expected` with the
    value pointed to by `object`, and memory is affected according to the value of `success`.

5   If `duration_ns` is neither zero nor `STDC_ATOMIC_WAIT_FOREVER`, the function shall not return
    because the interval elapsed until the total accumulated time during which the calling thread
    is suspended in this function is at least `duration_ns` nanoseconds, as measured by an
    implementation-defined monotonic time base. The suspension time can be longer than requested
    because the interval is rounded up to an integer multiple of the wait resolution or because of
    the scheduling of other activity by the system, but, except when the request cannot be honored,
    will not be less than that specified. After that time has elapsed, the function returns and
    memory is affected according to the value of `failure`.

6   If `duration_ns` is `STDC_ATOMIC_WAIT_FOREVER`, there is no time limit on when the function
    returns.

7   If the values compare equal and `__STDC_NO_ATOMIC_WAIT_TIMEOUT__` is defined and `duration_ns`
    is neither zero nor `STDC_ATOMIC_WAIT_FOREVER`, the function does not suspend the calling
    thread.

**Returns**

8   If the function returns because the values did not compare equal, or because the interval
    elapsed, the function returns zero. If the function returns because the value was no longer
    equal to the value pointed to by `expected` on entry after the calling thread was suspended,
    the function returns an implementation-defined positive value. If the request could not be
    honored, including an unsupported non-zero finite `duration_ns`, the function returns a
    negative value.

9   On return, if the function returns zero because the values did not compare equal, or if it
    returns a positive value, the object pointed to by `expected` contains the value that caused
    the function to return; otherwise, it is unchanged.

## 7.17.7.11 The atomic_notify generic function

**Synopsis**

1

```
#include <stdatomic.h>
int atomic_notify(volatile A *restrict object, C *restrict expected, C desired,
                  unsigned max_threads_to_wake, memory_order success,
                  memory_order failure);
```

**Description**

2   The value of `sizeof(C)` shall equal the value of `sizeof(uint_native_wait_notify_t)`.

3   The `failure` argument shall not be `memory_order_release` nor `memory_order_acq_rel`. The
    `failure` argument shall not impose more constraints on the operation than the `success`
    argument.

4   The atomic_notify generic function performs as if by
    `atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure)`. If the
    result of the comparison is true, the function unblocks `max_threads_to_wake` threads that are
    blocked on the atomic object pointed to by `object` in waiting operations, or all such threads
    if fewer than `max_threads_to_wake` are blocked. If `max_threads_to_wake` is zero, no threads
    are unblocked.

**Returns**

5   If the comparison is true and the request is otherwise honored, the function returns an
    implementation-defined positive value. If the comparison is false, the function returns zero.
    If the request could not be honored, the function returns a negative value.

**Recommended practice**

6   If the number of threads unblocked by the call is known, the function should return one plus
    that number.
