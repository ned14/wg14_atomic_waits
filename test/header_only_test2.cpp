// Third TU for header-only ODR test (OS-specific backend dispatch)
#include <wg14_atomic_waits/atomic_wait.h>
#include <atomic>

void wait_all_fn(std::atomic<int> *x)
{
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_all)(x);
}
