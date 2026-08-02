/* Compile-fail test: atomic_wait_expected on an object whose width does not
   match uint_native_wait_notify_t (4 bytes). */
#include <wg14_atomic_waits/atomic_wait.h>

typedef struct
{
  long long a;
} unsigned_8_t;

static unsigned_8_t obj8;
static uint_least32_t expected = 0;

void atomic_wait_expected_badtype_test(void)
{
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &obj8, &expected, WG14_ATOMIC_WAITS_NULLPTR,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst,
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX memory_order_seq_cst);
}

int main(void)
{
  return 0;
}
