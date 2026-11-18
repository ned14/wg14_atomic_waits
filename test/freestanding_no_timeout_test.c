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

#define WG14_ATOMIC_WAITS_ENABLE_HEADER_ONLY 1
#define WG14_ATOMIC_WAITS_NO_ATOMIC_WAIT_TIMEOUT 1
#include <wg14_atomic_waits/atomic_wait.h>

int main(void)
{
  return 0;
}
