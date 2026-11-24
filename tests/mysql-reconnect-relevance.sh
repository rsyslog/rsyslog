#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# The reconnect test must skip before its first MySQL query when the MySQL
# family is irrelevant. A stub records that query without requiring a server;
# relevant and forced runs must reach it. The stub identifies as MariaDB to
# finish those cases at the first query. A failed-query case also checks that
# the test gives error_exit a numeric status and emits a useful diagnostic.
set -eu

testdir="$(cd "${srcdir:-$(dirname "$0")}" && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
mkdir "$scratch/bin"

cat > "$scratch/diag.sh" <<EOF
ensure_module_needs_testing() {
	"$testdir/diag.sh" module-needs-testing "\$1" || exit 77
}
require_plugin() { :; }
error_exit() {
	case "\$1" in
		''|*[!0-9]*) printf 'nonnumeric error_exit status: %s\n' "\$1" >&2; exit 2 ;;
	esac
	exit "\$1"
}
EOF
cat > "$scratch/bin/mysql" <<'EOF'
#!/usr/bin/env bash
printf 'called\n' >> "$MYSQL_PROBE_MARKER"
if [ "${MYSQL_PROBE_FAIL:-0}" = 1 ]; then
	exit 6
fi
printf '8.0.42-MariaDB\n'
EOF
chmod +x "$scratch/bin/mysql"

check_case() {
	local name="$1" changed="$2" forced="$3" expected_query="$4" status=0
	rm -f "$scratch/mysql-called"
	(
		cd "$testdir"
		export srcdir="$scratch" MYSQL_PROBE_MARKER="$scratch/mysql-called"
		export RSYSLOG_TESTBENCH_CHANGED_FILES="$changed"
		export RSYSLOG_TESTBENCH_FORCE_MYSQL_TESTS="$forced"
		unset RSYSLOG_TESTBENCH_FORCE_SERVICE_TESTS RSYSLOG_TESTBENCH_SKIP_SERVICE_RELEVANCE
		export SUDO='' MYSQL_PROBE_FAIL=0
		if [ "$name" = queryfailure ]; then export MYSQL_PROBE_FAIL=1; fi
		export PATH="$scratch/bin:$PATH"
		bash "$testdir/mysql-reconnect-timeout.sh"
	) > "$scratch/$name.log" 2>&1 || status=$?
	local expected_status=77
	if [ "$name" = queryfailure ]; then expected_status=1; fi
	if [ "$status" -ne "$expected_status" ]; then
		printf '%s: expected exit %s, got %s\n' "$name" "$expected_status" "$status" >&2
		cat "$scratch/$name.log" >&2
		exit 1
	fi
	if [ "$expected_query" = yes ] && [ ! -s "$scratch/mysql-called" ]; then
		printf '%s: relevant MySQL test did not query MySQL\n' "$name" >&2
		exit 1
	fi
	if [ "$expected_query" = no ] && [ -e "$scratch/mysql-called" ]; then
		printf '%s: irrelevant MySQL test queried MySQL\n' "$name" >&2
		exit 1
	fi
	if [ "$name" = queryfailure ] && ! grep -q 'could not query MySQL server version' "$scratch/$name.log"; then
		printf 'queryfailure: missing query failure diagnostic\n' >&2
		exit 1
	fi
}

case "${1:-all}" in
	irrelevant) check_case irrelevant 'tests/rscript_ipv42num.sh' 0 no ;;
	relevant) check_case relevant 'tests/mysql-basic.sh' 0 yes ;;
	forced) check_case forced 'tests/rscript_ipv42num.sh' 1 yes ;;
	queryfailure) check_case queryfailure 'tests/mysql-basic.sh' 0 yes ;;
	all)
		check_case irrelevant 'tests/rscript_ipv42num.sh' 0 no
		check_case relevant 'tests/mysql-basic.sh' 0 yes
		check_case forced 'tests/rscript_ipv42num.sh' 1 yes
		check_case queryfailure 'tests/mysql-basic.sh' 0 yes
		;;
	*) printf 'unknown case: %s\n' "$1" >&2; exit 2 ;;
esac
