// C++ compile-fail test: atomic_wait on an object whose width is not 1/2/4/8.
#include <wg14_atomic_waits/atomic_wait.h>

struct unsigned_16_t
{
  int a, b, c, d;
};

static unsigned_16_t obj16;

void atomic_wait_badtype_test()
{
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&obj16, 0);
}

int main()
{
  return 0;
}
