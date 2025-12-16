# AGENTS.md – omotel output module

These instructions apply to files inside `plugins/omotel/`.

## Development notes
- Keep the module pure C unless the optional gRPC shim is enabled.
- Update `MODULE_METADATA.yaml` and the user documentation when adding new
  configuration parameters or behavioral changes.
- Refresh the concurrency note in `omotel.c` if locking expectations change.
- Run `devtools/format-code.sh` before committing.

## Testing
- Run `tests/omotel-http-batch.sh` to exercise the HTTP batching, gzip, and
  retry path.
