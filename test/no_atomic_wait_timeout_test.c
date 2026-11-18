// Verifies the __STDC_NO_ATOMIC_WAIT_TIMEOUT__ contract: zero and
// STDC_ATOMIC_WAIT_FOREVER remain supported, while a non-zero finite duration
// is rejected without blocking.
#define WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 1
#define WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT 1
#include <wg14_atomic_waits/atomic_wait.h>

#include <stdio.h>

#define CHECK(x)                                                               \
  do                                                                           \
  {                                                                            \
    if(!(x))                                                                   \
    {                                                                          \
      fprintf(stderr,                                                          \
              "no_atomic_wait_timeout_test: CHECK(" #x                         \
              ") failed at line %d\n",                                         \
              __LINE__);                                                       \
      return 1;                                                                \
    }                                                                          \
  } while(0)

int main(void)
{
#ifndef __STDC_NO_ATOMIC_WAIT_TIMEOUT__
  fprintf(stderr, "no_atomic_wait_timeout_test: "
                  "__STDC_NO_ATOMIC_WAIT_TIMEOUT__ is not defined\n");
  return 1;
#endif

  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t value = 0;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;

  // A non-zero finite duration is unsupported, but an immediate mismatch must
  // still win and return 0.
  expected = 1;
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value, &expected, 1000000000ull, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected == 0);

  // A non-zero finite duration with a matching value is rejected.
  expected = 0;
  r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value, &expected, 1000000000ull, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(r < 0);
  CHECK(expected == 0);

  // Zero remains the non-blocking poll.
  r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value, &expected, 0, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected == 0);

  // STDC_ATOMIC_WAIT_FOREVER remains supported. Use an immediate mismatch so
  // the test does not block.
  expected = 1;
  r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value, &expected, STDC_ATOMIC_WAIT_FOREVER, memory_order_seq_cst,
  memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected == 0);

  return 0;
}
