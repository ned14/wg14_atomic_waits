// Compile-only check that the header-only, no-timeout configuration does not
// pull in <time.h> or <stdio.h>. The include directory provided by CMake puts
// poisoning versions of those headers first.
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// This test checks the no-timeout freestanding path. Disable the optional
// yield dependency even when the build enables SPIN_BACKEND_YIELD: some C
// libraries' <threads.h> includes <time.h>, which would defeat the poisoning
// check below.
#ifdef WG14_ATOMIC_WAITS_SPIN_USE_YIELD
#undef WG14_ATOMIC_WAITS_SPIN_USE_YIELD
#endif
#define WG14_ATOMIC_WAITS_SPIN_USE_YIELD 0

#define WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 1
#define WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT 1
#include <wg14_atomic_waits/atomic_wait.h>

int main(void)
{
  return 0;
}
