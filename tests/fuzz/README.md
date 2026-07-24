# Syslog parser fuzzing

The `fuzz_rsyslog_message` target sends every input independently through the
core PRI parser and the built-in RFC 3164 and RFC 5424 parsers. The seed corpus
contains both formats, with additional RFC 3164 cases because legacy syslog
remains widely deployed.

Build an instrumented target in a clean worktree:

```sh
export CC=clang
export CFLAGS="-O1 -g -fno-omit-frame-pointer \
	-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize=function"
export LDFLAGS="-fsanitize=address,undefined -fno-sanitize=function"
./autogen.sh --enable-debug --enable-testbench --enable-fuzzing
make -C tools -j"${RSYSLOG_LOCAL_BUILD_JOBS:-10}" fuzz_rsyslog_message
```

`LIB_FUZZING_ENGINE` may name an alternative fuzzer library or linker flags
when the compiler's `-fsanitize=fuzzer` runtime cannot be linked directly.
The function-type UBSan check is excluded because rsyslog's module ABI uses
generic callback pointers; ASan and all other `undefined` checks remain active.

Run a bounded local campaign:

```sh
mkdir -p fuzz-corpus-work
tools/fuzz_rsyslog_message \
	-max_len=65536 \
	-timeout=5 \
	-rss_limit_mb=2048 \
	fuzz-corpus-work \
	tests/fuzz/corpus/syslog-message
```

libFuzzer writes newly interesting inputs to its first corpus directory, so
the separate work directory keeps the checked-in seed corpus unchanged.
The fuzzer treats crashes, sanitizer findings, timeouts, and invalid parser
offsets as failures. Minimize any artifact before turning it into a permanent
regression test:

```sh
tools/fuzz_rsyslog_message \
	-minimize_crash=1 \
	-exact_artifact_path=minimized-input \
	crash-input
```
