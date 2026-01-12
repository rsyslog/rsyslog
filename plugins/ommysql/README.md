# ommysql TSAN notes

The `ommysql` module uses per-worker state (`wrkrInstanceData_t`) so each
worker thread has its own `MYSQL *` handle and SQL buffer. In TSAN runs of
`tests/mysql-asyn.sh` with the default Ubuntu 24.04 `libmysqlclient` package,
ThreadSanitizer reports data races originating inside `libmysqlclient` during
`mysql_query()`. The traces point at `memcmp()` in the client library while
another worker is allocating or resizing its own buffer.

We added diagnostic logging in `actionCheckAndCreateWrkrInstance()` and
`writeMySQL()` to confirm that each worker thread owns distinct
`actWrkrData` and `sqlBuf` pointers. The logs show:

- Each worker thread gets a unique `actWrkrData` pointer.
- Each worker thread allocates a unique `sqlBuf` pointer.

This indicates the per-worker isolation in rsyslog is functioning correctly.
The remaining TSAN reports are likely due to uninstrumented or internally
synchronized `libmysqlclient` code paths, which TSAN cannot observe when
using the stock Ubuntu package. We are not using a TSAN-instrumented MySQL
client library in CI, and we generally do not require custom instrumented
libraries for other dependencies.

As such, we added a suppression for the `libmysqlclient` stack frames in
`tests/tsan-rt.supp`, but keep the default behavior for normal builds
unchanged.
