# Test Suite Review: Problematic Patterns and Missing Coverage

Source of truth: `docs/proposal.md` (§7.17.7.7–7.17.7.11) for API semantics,
together with `plans/combined-analysis.md` (implementation deviations) and the
concurrency/correctness constraints from `AGENTS.md`.

---


## B. Open / not-fully-implemented items


### B4. [ ] Wake-by-store and negative `atomic_wait_expected` return are intentionally untested — document, don't chase

Per `combined-analysis.md` §1.4/§4, these are deliberately NOT asserted as positive
tests, and a future reviewer should not "complete" them naively:

- **Store-triggered wake**: `atomic_wait` may return after a value-changing store on any
  backend (a waiter in its load/re-check/park window observes the new value, and Windows
  native wakes every parked waiter). Proposal NOTE 1 is advisory. The suite therefore makes
  **no** negative "store does not wake" assertion; it only asserts notify-driven
  wake counts.
- **Negative `atomic_wait_expected` return**: only reachable on a genuine backend
  failure (wait_on_address hard error / hash-table allocation failure), not
  deterministically triggerable by a portable unit test. The suite asserts `0` on a
  negative `duration` (advisory) instead (`atomic_wait_expected_test.c`).

These are fully covered by existing tests as far as is portable; leave as documented
behaviour rather than adding a flaky/wrong assertion.
