`atomic_notify` could wake only those waiters waiting on
the same expected value as the value notified. This
would avoid the thundering herd problem. Should be
implementable on Mac OS and Linux.
