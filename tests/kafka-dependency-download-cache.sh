#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# A failed ZooKeeper download must not become a reusable cache entry. Exercise
# the real download_kafka helper with an owned cache and wget stub: both empty
# and partial failures must exit 77 after two attempts and leave no destination.
# A valid tiny archive and a no-download cache hit are independent controls.
# Source only the read-only relevance action, use real relevance/error_exit in
# child shells, and start no daemon/service or network request.
set -e

testdir="$(cd "${srcdir:-$(dirname "$0")}" && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
export RSYSLOG_TESTBENCH_CHANGED_FILES=tests/kafka-dependency-download-cache.sh
unset RSYSLOG_TESTBENCH_FORCE_SERVICE_TESTS RSYSLOG_TESTBENCH_FORCE_KAFKA_TESTS
unset RSYSLOG_TESTBENCH_SKIP_SERVICE_RELEVANCE
# diag.sh also supports this action when sourced; it defines the actual helper
# without the process cleanup or setup performed by its init action.
. "$testdir/diag.sh" module-needs-testing kafka
module_needs_testing kafka || { echo 'Kafka relevance gate unexpectedly disabled' >&2; exit 1; }
printf 'Kafka relevance gate enabled for tests/kafka-dependency-download-cache.sh\n'

export RS_ZK_DOWNLOAD="fixture-zookeeper-$$-$RANDOM.tar.gz"
export RS_KAFKA_DOWNLOAD="fixture-kafka-$$-$RANDOM.tar.gz"
[ ! -e "/local_dep_cache/$RS_ZK_DOWNLOAD" ] || exit 1
[ ! -e "/local_dep_cache/$RS_KAFKA_DOWNLOAD" ] || exit 1
dep_cache_dir="$scratch/cache"
dep_zk_cached_file="$dep_cache_dir/$RS_ZK_DOWNLOAD"
dep_kafka_cached_file="$dep_cache_dir/$RS_KAFKA_DOWNLOAD"
export dep_zk_url=fixture://zookeeper
mkdir "$dep_cache_dir" "$scratch/bin" "$scratch/archive"
printf 'fixture\n' > "$scratch/archive/member"
tar -czf "$scratch/valid.tar.gz" -C "$scratch/archive" member
cp "$scratch/valid.tar.gz" "$dep_kafka_cached_file"
export FAKE_WGET_CALLS="$scratch/wget.calls" FAKE_DOWNLOAD_ARCHIVE="$scratch/valid.tar.gz"
export FAKE_WGET_DEST="$dep_zk_cached_file"

cat > "$scratch/bin/wget" <<'STUB'
#!/usr/bin/env bash
set -eu
printf '%s\n' "$*" >> "$FAKE_WGET_CALLS"
output=''
while [ "$#" -gt 0 ]; do
 case "$1" in
  -q|fixture://zookeeper) shift ;;
  -O) output="$2"; shift 2 ;;
  *) exit 99 ;;
 esac
done
[ "$output" = "$FAKE_WGET_DEST" ] || exit 99
case "$FAKE_WGET_MODE" in
 empty) : > "$output"; exit 4 ;;
 partial) printf 'partial-download' > "$output"; exit 4 ;;
 success) cp "$FAKE_DOWNLOAD_ARCHIVE" "$output" ;;
 *) exit 99 ;;
esac
STUB
chmod +x "$scratch/bin/wget"
export PATH="$scratch/bin:$PATH"
[ "$(command -v wget)" = "$scratch/bin/wget" ] || exit 1

run_download() {
 (
  cd "$scratch"
  # Real error_exit must see only this empty owned process namespace. Clearing
  # optional service/stat hooks also prevents expected failures from contacting
  # external services or collecting logs from the parent checkout.
  srcdir="$scratch"
  export RSYSLOG_PIDBASE="$scratch/no-daemon"
  export RSYSLOG_DYNNAME="$scratch/fixture"
  TB_STARTTEST=$(date +%s)
  export TB_STARTTEST
  export EXTRA_EXIT='' EXTRA_EXITCHECK='' REDIS_DYN_DIR='' RSYSLOG_STATSURL=''
  export MINITCPSRVR_PIDS='' UDP_RECEIVER_PIDS='' TEST_STATUS='reliable'
  download_kafka
 )
}

failures=0
for mode in empty partial; do
 rm -f "$dep_zk_cached_file"
 : > "$FAKE_WGET_CALLS"
 export FAKE_WGET_MODE="$mode"
 status=0
 run_download > "$scratch/$mode.log" 2>&1 || status=$?
 calls=$(wc -l < "$FAKE_WGET_CALLS")
 printf '%s: status=%s attempts=%s\n' "$mode" "$status" "$calls"
 if [ "$status" -ne 77 ] || [ "$calls" -ne 2 ]; then
  cat "$scratch/$mode.log" >&2
  exit 1
 fi
 if [ -e "$dep_zk_cached_file" ]; then
  printf '%s: failed download remained cached (%s bytes)\n' "$mode" "$(wc -c < "$dep_zk_cached_file")" >&2
  failures=$((failures + 1))
 fi
done

rm -f "$dep_zk_cached_file"
: > "$FAKE_WGET_CALLS"
export FAKE_WGET_MODE=success
run_download > "$scratch/recovery.log" 2>&1
cmp "$scratch/valid.tar.gz" "$dep_zk_cached_file"
tar -tzf "$dep_zk_cached_file" > /dev/null
[ "$(wc -l < "$FAKE_WGET_CALLS")" -eq 1 ] || exit 1
export FAKE_WGET_MODE=empty
run_download > "$scratch/cache-hit.log" 2>&1
[ "$(wc -l < "$FAKE_WGET_CALLS")" -eq 1 ] || exit 1
cmp "$scratch/valid.tar.gz" "$dep_zk_cached_file"
printf 'Recovery and existing valid cache controls passed\n'
[ "$failures" -eq 0 ]
