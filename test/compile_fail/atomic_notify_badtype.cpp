// C++ compile-fail test: atomic_notify on an object whose width does not match
// uint_native_wait_notify_t (4 bytes).
#include <wg14_atomic_waits/atomic_wait.h>

struct unsigned_8_t
{
  long long a;
};

static unsigned_8_t obj8;
static WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;

void atomic_notify_badtype_test()
{
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
  &obj8, &expected, 1, 1, WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);
}

int main()
{
  return 0;
}
