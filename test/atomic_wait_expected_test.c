#include "test_common.h"
#include <wg14_atomic_waits/atomic_wait.h>

// Progress markers on stderr: ctest only echoes them on failure, so a hang is
// localisable to the exact section of the test that blocked.
#define SECTION(name) fprintf(stderr, "atomic_wait_expected_test: " name "\n")

// Bounded head start given to a spawned waiter, after the park handshake,
// before a value-changing store + notify. The handshake only guarantees the
// waiter is *about* to enter the wait; on a heavily instrumented runtime (e.g.
// Fil-C, whose per-access checks make thread entry comparatively slow) the
// store can beat the waiter's first recheck every attempt, so the wait would
// return 0 (no suspension) on every retry. The settle sits inside the
// park-handshake synchronisation (AGENTS.md rule 5: sleeps within a proper
// synchronisation are permitted) and mirrors spurious_wake_test in
// atomic_wait_more_test.c.
#define PARK_SETTLE_MS 50

static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_result = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t g_reloaded = 0;

static int waiter_suspend_once(void *arg)
{
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *value =
  (WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_store_explicit(&g_parked, 1, memory_order_release);
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  value, &expected, WG14_ATOMIC_WAITS_NULLPTR, memory_order_seq_cst,
  memory_order_seq_cst);
  atomic_store_explicit(&g_result, r, memory_order_release);
  atomic_store_explicit(&g_reloaded, expected, memory_order_release);
  return 0;
}

// --- spurious-wake re-park for atomic_wait_expected ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_sp_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_sp_returned = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_sp_result = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t g_sp_reloaded = 0;

static int waiter_spurious(void *arg)
{
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *value =
  (WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_store_explicit(&g_sp_parked, 1, memory_order_release);
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  value, &expected, WG14_ATOMIC_WAITS_NULLPTR, memory_order_seq_cst,
  memory_order_seq_cst);
  atomic_store_explicit(&g_sp_result, r, memory_order_release);
  atomic_store_explicit(&g_sp_reloaded, expected, memory_order_release);
  atomic_store_explicit(&g_sp_returned, 1, memory_order_release);
  return 0;
}

// --- woken-before-duration for atomic_wait_expected ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_td_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_td_result = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t g_td_reloaded = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_long g_td_elapsed_ms = 0;

static int waiter_timed(void *arg)
{
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *value =
  (WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  struct timespec d = {.tv_sec = 5, .tv_nsec = 0};
  atomic_store_explicit(&g_td_parked, 1, memory_order_release);
  struct timespec start, fin;
  timespec_get(&start, TIME_UTC);
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  value, &expected, &d, memory_order_seq_cst, memory_order_seq_cst);
  timespec_get(&fin, TIME_UTC);
  atomic_store_explicit(&g_td_result, r, memory_order_release);
  atomic_store_explicit(&g_td_reloaded, expected, memory_order_release);
  atomic_store_explicit(&g_td_elapsed_ms,
                        (long) ((fin.tv_sec - start.tv_sec) * 1000L +
                                (fin.tv_nsec - start.tv_nsec) / 1000000L),
                        memory_order_release);
  return 0;
}

// A helper that repeatedly issues spurious notifies (value unchanged) for a
// bounded time, to exercise the "total accumulated time >= *duration" clause
// even when the waiter is woken and re-suspended repeatedly.
static int spurious_notifier(void *arg)
{
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *value =
  (WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *) arg;
  for(int i = 0; i < 30; i++)
  {
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(value);
    thrd_sleep_ms(10);
  }
  return 0;
}

// --- distinct memory_order success/failure contract (item 1) ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_o_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_o_returned = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_o_result = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_least32_t g_o_reloaded = 0;

static int waiter_orders(void *arg)
{
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *value =
  (WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t *) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_store_explicit(&g_o_parked, 1, memory_order_release);
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  value, &expected, WG14_ATOMIC_WAITS_NULLPTR, memory_order_acquire,
  memory_order_relaxed);
  atomic_store_explicit(&g_o_result, r, memory_order_release);
  atomic_store_explicit(&g_o_reloaded, expected, memory_order_release);
  atomic_store_explicit(&g_o_returned, 1, memory_order_release);
  return 0;
}

// --- happens-before / synchronizes-with (item 3) ---
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_hb_data = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t
g_hb_val = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_hb_parked = 0;
static WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_int g_hb_seen = -1;

static int hb_waiter(void *arg)
{
  (void) arg;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;
  atomic_store_explicit(&g_hb_parked, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &g_hb_val, &expected, WG14_ATOMIC_WAITS_NULLPTR, memory_order_acquire,
  memory_order_relaxed);
  // Evaluate the happens-before edge: side effects sequenced before the
  // notifying release-store must be visible to the woken reader.
  atomic_store_explicit(&g_hb_seen,
                        atomic_load_explicit(&g_hb_data, memory_order_acquire),
                        memory_order_release);
  return 0;
}

static int hb_notifier(void *arg)
{
  (void) arg;
  // The main thread only starts us once g_hb_parked is set, so the waiter is
  // parked (or about to park); a value-changing store followed by a notify
  // cannot be lost either way.
  atomic_store_explicit(&g_hb_data, 1, memory_order_release);
  atomic_store_explicit(&g_hb_val, 1, memory_order_release);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&g_hb_val);
  return 0;
}

int atomic_wait_expected_test_main(void)
{
  int ret = 0;
  WG14_ATOMIC_WAITS_ATOMIC_PREFIX atomic_uint_native_wait_notify_t value = 0;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) expected = 0;

  // Immediate return - value != expected (no suspension)
  SECTION("immediate mismatch (no suspension)");
  expected = 1;
  int r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value, &expected, WG14_ATOMIC_WAITS_NULLPTR, memory_order_seq_cst,
  memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected == 0);

  // Wait with zero timeout, should always time out (no suspension)
  SECTION("zero timeout (no suspension)");
  struct timespec dur = {.tv_sec = 0, .tv_nsec = 0};
  expected = 0;
  r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
  &value, &expected, &dur, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(r == 0);
  CHECK(expected == 0);

  // Real (non-zero) duration with no notify: must suspend, wait for the full
  // duration (ceiling), then return 0 on timeout with *expected unchanged.
  SECTION("full-duration timeout (50ms)");
  {
    const unsigned timeout_ms = 50;
    struct timespec start, fin, d = {.tv_sec = 0, .tv_nsec = 0};
    d.tv_nsec = (long) timeout_ms * 1000000L;
    expected = 0;
    timespec_get(&start, TIME_UTC);
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value, &expected, &d, memory_order_seq_cst, memory_order_seq_cst);
    timespec_get(&fin, TIME_UTC);
    CHECK(r == 0);
    CHECK(expected == 0);
    const long elapsed_ms = (long) ((fin.tv_sec - start.tv_sec) * 1000L +
                                    (fin.tv_nsec - start.tv_nsec) / 1000000L);
    CHECK(elapsed_ms >= (long) timeout_ms);
  }

  // Multi-second duration that crosses the 1 second boundary; exercises the
  // UINT32_MAX-ns cap-and-loop and severity conversion paths on macOS.
  SECTION("multi-second duration (1.25s)");
  {
    struct timespec start, fin, d = {.tv_sec = 1, .tv_nsec = 250000000L};
    expected = 0;
    timespec_get(&start, TIME_UTC);
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value, &expected, &d, memory_order_seq_cst, memory_order_seq_cst);
    timespec_get(&fin, TIME_UTC);
    CHECK(r == 0);
    CHECK(expected == 0);
    const long elapsed_ms = (long) ((fin.tv_sec - start.tv_sec) * 1000L +
                                    (fin.tv_nsec - start.tv_nsec) / 1000000L);
    CHECK(elapsed_ms >= 1250L);
  }

  // Negative duration: the implementation treats it as an immediate timeout
  // (returns 0 with *expected unchanged) rather than as an error. The proposal
  // leaves the error contract open; we verify the sane no-crash behaviour.
  SECTION("negative duration");
  {
    struct timespec d = {.tv_sec = -1, .tv_nsec = 0};
    expected = 0;
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value, &expected, &d, memory_order_seq_cst, memory_order_seq_cst);
    CHECK(r == 0);
    CHECK(expected == 0);
  }

  // Positive return when the thread was actually suspended at least once, and
  // *expected reloaded to the notified object value after wake.
  SECTION("suspend once, value-changing notify");
  // The waiter stores its parked flag before it enters the wait, so the
  // store+notify below can land in the window before the waiter's first value
  // load; the wait then returns immediately with 0 (no suspension, per the
  // documented \retval contract) and *expected reloaded to the new value. That
  // is a legitimate outcome, not a failure, so retry until the waiter really
  // suspends and this section exercises the notified path. The PARK_SETTLE_MS
  // settle after the handshake gives the waiter a bounded head start to reach
  // the kernel park, so this section does not depend on winning that race (it
  // flaked on Fil-C + pthreads backend CI, see plans/combined-analysis.md
  // §1.14).
  int wr;
  int suspended = 0;
  for(int attempt = 0; !suspended && attempt < 100; attempt++)
  {
    g_parked = 0;
    g_result = 0;
    g_reloaded = 0;
    atomic_store_explicit(&value, 0, memory_order_seq_cst);
    thrd_t thr;
    CHECK(thrd_create(&thr, waiter_suspend_once, &value) == thrd_success);
    test_wait_until("g_parked", &g_parked, 1);
    thrd_sleep_ms(PARK_SETTLE_MS);
    atomic_store_explicit(&value, 7, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);
    CHECK(thrd_join(thr, &wr) == thrd_success);
    suspended = atomic_load_explicit(&g_result, memory_order_acquire) > 0;
  }
  CHECK(suspended);
  CHECK(atomic_load_explicit(&g_reloaded, memory_order_acquire) == 7);

  // Spurious wake while parked (value unchanged): the caller must re-suspend
  // and only return once the value really differs, with *expected reloaded.
  SECTION("spurious wake re-park");
  g_sp_parked = 0;
  g_sp_returned = 0;
  g_sp_result = 0;
  g_sp_reloaded = 0;
  atomic_store_explicit(&value, 0, memory_order_seq_cst);
  thrd_t sp;
  CHECK(thrd_create(&sp, waiter_spurious, &value) == thrd_success);
  test_wait_until("g_sp_parked", &g_sp_parked, 1);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);
  thrd_sleep_ms(30);
  CHECK(atomic_load_explicit(&g_sp_returned, memory_order_acquire) == 0);
  atomic_store_explicit(&value, 9, memory_order_seq_cst);
  WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);
  CHECK(thrd_join(sp, &wr) == thrd_success);
  CHECK(atomic_load_explicit(&g_sp_returned, memory_order_acquire) == 1);
  CHECK(atomic_load_explicit(&g_sp_result, memory_order_acquire) > 0);
  CHECK(atomic_load_explicit(&g_sp_reloaded, memory_order_acquire) == 9);

  // Woken before the duration elapses: must return promptly (not at timeout)
  // with a positive result and *expected reloaded.
  SECTION("woken before duration");
  // Same entry-window race as the "suspend once" section above: g_td_parked is
  // stored before the waiter enters the wait, so the store+notify can land
  // before the waiter's first value load and the wait returns 0 (no suspension)
  // with *expected reloaded. The PARK_SETTLE_MS settle removes the race; the
  // retry loop below is a safety net. Retry until the waiter really suspends.
  int td_suspended = 0;
  for(int attempt = 0; !td_suspended && attempt < 100; attempt++)
  {
    g_td_parked = 0;
    g_td_result = 0;
    g_td_reloaded = 0;
    g_td_elapsed_ms = 0;
    atomic_store_explicit(&value, 0, memory_order_seq_cst);
    thrd_t td;
    CHECK(thrd_create(&td, waiter_timed, &value) == thrd_success);
    test_wait_until("g_td_parked", &g_td_parked, 1);
    thrd_sleep_ms(PARK_SETTLE_MS);
    atomic_store_explicit(&value, 3, memory_order_seq_cst);
    WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);
    CHECK(thrd_join(td, &wr) == thrd_success);
    td_suspended = atomic_load_explicit(&g_td_result, memory_order_acquire) > 0;
  }
  CHECK(td_suspended);
  CHECK(atomic_load_explicit(&g_td_reloaded, memory_order_acquire) == 3);
  CHECK(atomic_load_explicit(&g_td_elapsed_ms, memory_order_acquire) < 3000L);

  // Accumulated timeout: repeated spurious notifies must not let the wait
  // return before *duration; on timeout it still returns 0 with *expected
  // unchanged.
  SECTION("accumulated timeout (spurious notifier)");
  {
    struct timespec start, fin, d = {.tv_sec = 0, .tv_nsec = 0};
    d.tv_nsec = 150000000L;
    atomic_store_explicit(&value, 0, memory_order_seq_cst);
    thrd_t notifier;
    CHECK(thrd_create(&notifier, spurious_notifier, &value) == thrd_success);
    expected = 0;
    timespec_get(&start, TIME_UTC);
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value, &expected, &d, memory_order_seq_cst, memory_order_seq_cst);
    timespec_get(&fin, TIME_UTC);
    thrd_join(notifier, &wr);
    CHECK(r == 0);
    CHECK(expected == 0);
    const long elapsed_ms = (long) ((fin.tv_sec - start.tv_sec) * 1000L +
                                    (fin.tv_nsec - start.tv_nsec) / 1000000L);
    CHECK(elapsed_ms >= 150L);
  }

  // CAS failure returns 0 and *expected is updated to the observed value.
  SECTION("atomic_notify CAS failure");
  atomic_store_explicit(&value, 5, memory_order_seq_cst);
  expected = 10;
  WG14_ATOMIC_WAITS_PREFIX(uint_native_wait_notify_t) desired = 20;
  int nr = WG14_ATOMIC_WAITS_PREFIX(atomic_notify)(
  &value, &expected, desired, 1, memory_order_seq_cst, memory_order_seq_cst);
  CHECK(nr == 0);
  CHECK(expected == 5);

  // Item 1 - distinct success/failure memory orders are accepted and the
  // return-value / reload contract holds for both the no-suspension (zero) and
  // suspended (positive) outcomes. NOTE: the precise per-outcome ordering that
  // `atomic_wait_expected` performs internally is not observably
  // distinguishable from correct caller code on real hardware, so this verifies
  // the contract behaviour, not the exact ordering (the success-side ordering
  // is additionally exercised by the happens-before test below).
  SECTION("distinct success/failure orders");
  {
    atomic_store_explicit(&value, 0, memory_order_seq_cst);
    expected = 5;  // mismatch -> no suspension, returns 0, *expected reloaded
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value, &expected, WG14_ATOMIC_WAITS_NULLPTR, memory_order_acquire,
    memory_order_relaxed);
    CHECK(r == 0);
    CHECK(expected == 0);

    atomic_store_explicit(&value, 0, memory_order_seq_cst);
    expected = 0;
    struct timespec zd = {.tv_sec = 0, .tv_nsec = 0};
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value, &expected, &zd, memory_order_relaxed, memory_order_seq_cst);
    CHECK(r == 0);
    CHECK(expected == 0);

    // Same entry-window race as the "suspend once" section above: retry until
    // the waiter really suspends (the PARK_SETTLE_MS settle removes the race;
    // the retry loop below is a safety net).
    int o_suspended = 0;
    for(int attempt = 0; !o_suspended && attempt < 100; attempt++)
    {
      g_o_parked = 0;
      g_o_returned = 0;
      g_o_result = 0;
      g_o_reloaded = 0;
      atomic_store_explicit(&value, 0, memory_order_seq_cst);
      thrd_t ow;
      CHECK(thrd_create(&ow, waiter_orders, &value) == thrd_success);
      test_wait_until("g_o_parked", &g_o_parked, 1);
      thrd_sleep_ms(PARK_SETTLE_MS);
      atomic_store_explicit(&value, 11, memory_order_seq_cst);
      WG14_ATOMIC_WAITS_PREFIX(atomic_notify_one)(&value);
      CHECK(thrd_join(ow, &wr) == thrd_success);
      o_suspended = atomic_load_explicit(&g_o_result, memory_order_acquire) > 0;
    }
    CHECK(o_suspended);
    CHECK(atomic_load_explicit(&g_o_returned, memory_order_acquire) == 1);
    CHECK(atomic_load_explicit(&g_o_reloaded, memory_order_acquire) == 11);
  }

  // Item 3 - happens-before / synchronizes-with: side effects sequenced before
  // the notifying release-store are visible to the woken waiter (proposal
  // §7.17.7.10 "All side effects ... synchronize with the waiting thread").
  SECTION("happens-before");
  g_hb_data = 0;
  g_hb_val = 0;
  g_hb_parked = 0;
  g_hb_seen = -1;
  thrd_t hw, hn;
  // Start the waiter first and only start the notifier once the waiter is
  // parked, so the notifier is guaranteed to fire while the waiter is in the
  // wait. This removes the lost-wakeup window that could otherwise leave the
  // unbounded waiter asleep forever (a CI flake that surfaced as a timeout).
  CHECK(thrd_create(&hw, hb_waiter, NULL) == thrd_success);
  test_wait_until("g_hb_parked", &g_hb_parked, 1);
  CHECK(thrd_create(&hn, hb_notifier, NULL) == thrd_success);
  CHECK(thrd_join(hw, &wr) == thrd_success);
  CHECK(thrd_join(hn, &wr) == thrd_success);
  CHECK(atomic_load_explicit(&g_hb_seen, memory_order_acquire) == 1);

  // Item 4 - error / corner semantics: the return value must never be a
  // spurious positive on degenerate inputs. A genuinely-negative value is only
  // produced on a backend synchronization failure (e.g. wait_on_address hard
  // error or hash-table allocation failure), which is not deterministically
  // reachable in a portable unit test; these inputs must still return
  // non-positive (0) without crashing and leave *expected unchanged.
  SECTION("corner semantics");
  {
    struct timespec dec = {.tv_sec = -1, .tv_nsec = 0};
    atomic_store_explicit(&value, 0, memory_order_seq_cst);
    expected = 0;
    r = WG14_ATOMIC_WAITS_PREFIX(atomic_wait_expected)(
    &value, &expected, &dec, memory_order_seq_cst, memory_order_seq_cst);
    CHECK(r == 0);
    CHECK(expected == 0);
  }

  return ret;
}

int main(void)
{
  return atomic_wait_expected_test_main();
}
