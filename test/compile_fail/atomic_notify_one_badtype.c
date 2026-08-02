/* Compile-fail test: atomic_notify_one on an object whose width is not
   1/2/4/8. */
#include <wg14_atomic_waits/atomic_wait.h>

typedef struct
{
  int a, b, c, d;
} unsigned_16_t;

static unsigned_16_t obj16;

void atomic_notify_one_badtype_test(void)
{
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&obj16);
}

int main(void)
{
  return 0;
}
