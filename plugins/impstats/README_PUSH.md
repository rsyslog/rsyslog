# impstats Push Notes (Developer Scope)

This file is intentionally limited to contributor notes.
User-facing documentation for impstats push mode lives in:

- `doc/source/configuration/modules/impstats.rst`
- `doc/source/faq/impstats-push-mode.rst`
- `doc/source/reference/parameters/impstats-push-*.rst`

## Implementation map

- `impstats.c`: module config parsing, activation, and integration hooks
- `impstats_push.c`: Remote Write send path, metric/label shaping, HTTP/TLS
- `prometheus_remote_write.c`: protobuf encoding + snappy compression
- `remote.proto`: protobuf schema

## Build gate

Push support is compiled only with:

```bash
./configure --enable-impstats --enable-impstats-push
```

## Contributor checklist

When changing push behavior:

1. Update canonical docs in `doc/source/...` (not this file).
2. Keep parameter docs and module/FAQ behavior descriptions consistent.
3. Run relevant tests in `tests/` and update/add tests if behavior changes.
