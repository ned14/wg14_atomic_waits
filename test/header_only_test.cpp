// C++ compile test for header-only inclusion
#include <atomic>
#include <chrono>
#include <thread>
#include <wg14_atomic_waits/atomic_wait.h>

int main()
{
  std::atomic<int> x(0);
  std::thread t(
  [&]()
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    x.store(1);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&x);
  });
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait)(&x, 0);
  t.join();
  return 0;
}
