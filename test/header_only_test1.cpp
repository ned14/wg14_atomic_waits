// Second TU for header-only ODR test
#include <wg14_atomic_waits/atomic_wait.h>
#include <atomic>

void notify_fn(std::atomic<int> *x)
{
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(x);
}
