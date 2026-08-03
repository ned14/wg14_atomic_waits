// C++ compile test for header-only inclusion
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <wg14_atomic_waits/atomic_wait.h>

// Progress markers on stderr: ctest only echoes them on failure, so a hang is
// localisable to the exact part of the test that blocked.
#define SECTION(name) std::fprintf(stderr, "header_only_test: " name "\n")

// Defined in the other header-only test TUs; calling them here forces the
// header's inline definitions to be pulled in and executed, not just linked.
void notify_fn(std::atomic<int> *x);
void wait_all_fn(std::atomic<int> *x);

int main()
{
  SECTION("wait/notify within a single TU");
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

  SECTION("cross-TU header-only ODR linkage");
  notify_fn(&x);
  wait_all_fn(&x);
  return 0;
}
