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

## imtcp session fuzzing

The `fuzz_imtcp_session` target drives the shared imtcp session engine without
opening sockets. It covers LF, additional-delimiter, octet-counted, multiline,
and fixed-regex framing, together with zlib and zstd stream decompression. The
target varies receive chunk boundaries and optionally closes incomplete
sessions.

Each input starts with four control bytes. Byte 0 uses bits 0-1 for compression
(`0` or `3` plain, `1` zlib, `2` zstd), then bits 2-7 respectively enable
fixed-regex framing, octet framing, multiline mode, discard instead of split,
LF-delimiter disabling, and Cisco-space framing. Byte 1 uses bits 0-1 for the
maximum message size (`32`, `128`, `512`, or `2048`), bits 2-3 for the
additional delimiter (none, NUL, `|`, or CR), and bits 4-5 to select one of
these fixed anchored expressions:

- `^<[0-9]{1,3}>`
- `^<[0-9]{1,3}>[A-Z][a-z][a-z] [ 0-9][0-9]`
- `^BEGIN:`

Byte 2 uses bits 6-7 to select whole-input, bytewise, fixed-size, or
deterministic pseudo-random chunking; its low six bits supply the fixed chunk
size or pseudo-random seed. Byte 3 uses bit 0 for `PrepareClose`, bits 1-2 for
the expansion-ratio limit (`0`, `2`, `8`, or `64`), bits 3-4 for the
decompressed-byte limit (`64`, `4096`, `65536`, or `1048576`), and bits 5-6
for the zstd window budget (`0`, `65536`, `262144`, or `2097152`). Unassigned
bits are ignored.

All remaining bytes are passed to imtcp as exact wire data. The checked-in
corpus is hexadecimal so binary zlib and zstd streams remain reviewable and
portable through distribution archives. Decode it before a local campaign:

```sh
python3 tests/fuzz/prepare-imtcp-session-corpus.py \
	tests/fuzz/corpus/imtcp-session fuzz-imtcp-corpus-work
tools/fuzz_imtcp_session \
	-max_len=65536 \
	-timeout=5 \
	-rss_limit_mb=2048 \
	fuzz-imtcp-corpus-work
```

The target treats rejected malformed or truncated compressed streams as
expected input handling. Crashes, sanitizer reports, resource leaks, invalid
submitted frame sizes, and inconsistent byte accounting are failures.
