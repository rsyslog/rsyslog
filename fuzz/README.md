# rsyslog fuzzing

This directory contains the in-process rsyslog fuzz targets. The targets avoid
starting a full daemon and keep filesystem writes under temporary directories.

## Smoke replay

Build the AFL++ file-mode target with in-tree sanitizer instrumentation:

```sh
make -j2 fuzz FUZZ_SANITIZERS=address,undefined FUZZ_COVERAGE=0
fuzz/run_fuzz_smoke.sh file
```

The file-mode driver accepts more than one input path. The smoke script uses
that interface for same-process `A -> A` and `A -> B -> A` lifecycle checks;
the second sequence includes a legacy configuration input. Per-input legacy
configuration values, including the owned main-queue filename, are restored
before the next input is dispatched.

An empty file is dispatched to the normal message parser with logical length
zero. Runtime initialization errors are fatal in both drivers instead of being
reported as successful no-op runs.

Build and replay the libFuzzer target:

```sh
CC=clang CFLAGS='-g -O1 -fno-omit-frame-pointer' \
	./configure --enable-testbench --enable-imdiag --enable-omstdout
make -j2 fuzz CC=clang FUZZ_SANITIZERS=address,undefined \
	FUZZ_NO_SANITIZE_FLAGS=-fno-sanitize=function FUZZ_COVERAGE=0
make -C fuzz CC=clang FUZZ_SANITIZERS=address,undefined \
	FUZZ_NO_SANITIZE_FLAGS=-fno-sanitize=function FUZZ_COVERAGE=0 \
	fuzz_rsyslog_parsers_libfuzzer
fuzz/run_fuzz_smoke.sh libfuzzer
```

Configure with the same compiler used for the fuzz build. Overriding `CC` only
at `make` time can leave stale module/linker state from a previous configure
run.

`-fno-sanitize=function` is needed for clang UBSAN because rsyslog module
interfaces intentionally pass generic query function pointers through the
historic plugin ABI.

Collect gcov/lcov-style coverage after seed replay:

```sh
make -j2 fuzz FUZZ_COVERAGE=1
FUZZ_COLLECT_COVERAGE=1 \
FUZZ_COVERAGE_OBJECT_DIR=/path/to/dedicated/rsyslog-build \
fuzz/run_fuzz_smoke.sh file
```

If the tree was built for coverage with clang, use `GCOV_TOOL='llvm-cov gcov'`
when replaying seeds so clang-generated profile notes are decoded by the
matching gcov frontend. `FUZZ_COVERAGE_OBJECT_DIR` is mandatory and must point
to the dedicated coverage build tree; only counters below that directory are
cleared or collected.

Coverage output is written below `fuzz/coverage/`.

## Corpus and crash scripts

Minimize an AFL++ corpus:

```sh
fuzz/minimize_corpus.sh --engine afl --target ./fuzz/fuzz_rsyslog_parsers \
	--input corpus/full --output corpus/min --dict fuzz/dicts/parser.dict
```

Minimize a libFuzzer corpus:

```sh
fuzz/minimize_corpus.sh --engine libfuzzer \
	--target ./fuzz/fuzz_rsyslog_parsers_libfuzzer \
	--input corpus/full --output corpus/min --dict fuzz/dicts/parser.dict
```

Replay crashes or timeout artifacts:

```sh
fuzz/replay_crashes.sh --engine file --target ./fuzz/fuzz_rsyslog_parsers crashes/
fuzz/replay_crashes.sh --engine libfuzzer \
	--target ./fuzz/fuzz_rsyslog_parsers_libfuzzer crashes/
```

## External libfastjson instrumentation

`libfastjson` is an external dependency, not a vendored subtree. The in-tree
`make fuzz` target instruments rsyslog objects, but full JSON sanitizer coverage
requires a separately instrumented libfastjson build:

```sh
fuzz/build_libfastjson_sanitized.sh /path/to/libfastjson /tmp/lfjson-fuzz
export PKG_CONFIG_PATH="/tmp/lfjson-fuzz/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
./configure --enable-testbench --enable-imdiag --enable-omstdout
make -j2 fuzz FUZZ_SANITIZERS=address,undefined FUZZ_COVERAGE=0
```

The helper script does not clone or download dependencies. It expects an
existing libfastjson checkout.

## Optional disk-assisted queue worker coverage

The queue lifecycle harness starts deterministic disk and disk-assisted queue
configuration paths by default. It intentionally does not start the DA queue
worker path during normal seed replay because that creates background worker
behavior. For a local, opt-in stress check:

```sh
RSYSLOG_FUZZ_ENABLE_DA_START=1 ./fuzz/fuzz_rsyslog_parsers seed-file
RSYSLOG_FUZZ_ENABLE_DA_START=1 RSYSLOG_FUZZ_ENABLE_DA_WORKER=1 \
	./fuzz/fuzz_rsyslog_parsers seed-file
```

Do not enable that environment variable for long fuzzing campaigns unless the
runner has been checked for stable worker shutdown behavior.

## Optional full action config coverage

The default action harness exercises omfile/omfwd-style template serialization
without invoking full action runtime teardown. To also instantiate omfile, omfwd,
and optional omrelp action configs, enable the opt-in path locally:

```sh
RSYSLOG_FUZZ_ENABLE_ACTION_CONFIG=1 ./fuzz/fuzz_rsyslog_parsers seed-file
```

Keep this disabled in CI and long fuzzing campaigns until the target runner has
been checked for stable action lifecycle shutdown behavior.
