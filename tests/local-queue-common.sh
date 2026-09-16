#!/bin/bash
# Shared deterministic helpers for the bounded local-queue scenarios.  All
# waits poll an observable impstats snapshot or a test-owned file, never a
# guessed scheduling delay.

localq_wait_stats() {
	local stats_file="$1"
	local record="$2"
	shift 2
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	local line pattern complete
	while [ "$(date +%s)" -le "$deadline" ]; do
		line=$(grep -F "${record}: origin=core.queue.local " "$stats_file" 2>/dev/null | tail -n 1 || true)
		complete=yes
		for pattern in "$@"; do
			case " $line " in
				*" $pattern "*) ;;
				*) complete=no; break ;;
			esac
		done
		if [ "$complete" = yes ]; then
			printf 'local queue stats reached %s: %s\n' "$record" "$line"
			return
		fi
		$TESTTOOL_DIR/msleep 100
	done
	printf 'FAIL: local queue stats for %s did not contain:' "$record"
	printf ' %s' "$@"
	printf '\nlast record: %s\n' "$line"
	cat "$stats_file" 2>/dev/null || true
	error_exit 1
}

# Wait for a numeric counter to exceed another counter or a literal bound.
# Callers establish a stable phase: for example the held dedicated callback
# fixes BE active holdings while any producer may add pending BE inventory.
localq_wait_stats_greater() {
    local stats_file="$1" record="$2" left="$3" right="$4"
    local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
    local line left_count right_count
    while [ "$(date +%s)" -le "$deadline" ]; do
        line=$(grep -F "${record}: origin=core.queue.local " "$stats_file" 2>/dev/null | tail -n 1 || true)
        read -r left_count right_count < <(printf '%s\n' "$line" | awk -v lhs="$left" -v rhs="$right" '
            BEGIN { r = rhs ~ /^[0-9]+$/ ? rhs : "" }
            { for (i = 1; i <= NF; ++i) {
                split($i, field, "=")
                if (field[1] == lhs) l = field[2]
                if (field[1] == rhs) r = field[2]
            } }
            END { print l, r }')
        if [[ "$left_count" =~ ^[0-9]+$ && "$right_count" =~ ^[0-9]+$ ]] &&
            [ "$left_count" -gt "$right_count" ]; then
            printf 'local queue stats reached %s > %s: %s\n' "$left" "$right" "$line"
            return
        fi
        $TESTTOOL_DIR/msleep 100
    done
    printf 'FAIL: local queue stats did not reach %s > %s; last record: %s\n' "$left" "$right" "$line"
    error_exit 1
}

# As above, but use ERE field expressions when a counter is deliberately
# nonzero and its exact scheduling-dependent value is not part of the contract.
localq_wait_stats_regex() {
	local stats_file="$1"
	local record="$2"
	shift 2
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	local line pattern complete
	while [ "$(date +%s)" -le "$deadline" ]; do
		line=$(grep -F "${record}: origin=core.queue.local " "$stats_file" 2>/dev/null | tail -n 1 || true)
		complete=yes
		for pattern in "$@"; do
			if ! printf '%s\n' "$line" | grep -Eq "(^| )${pattern}( |$)"; then
				complete=no
				break
			fi
		done
		if [ "$complete" = yes ]; then
			printf 'local queue stats reached %s: %s\n' "$record" "$line"
			return
		fi
		$TESTTOOL_DIR/msleep 100
	done
	printf 'FAIL: local queue stats for %s did not match:' "$record"
	printf ' %s' "$@"
	printf '\nlast record: %s\n' "$line"
	cat "$stats_file" 2>/dev/null || true
	error_exit 1
}

# Wait for several consecutive completed impstats records after a known
# quiescent point. Every record must carry the same lifetime fields: this is
# the oracle that impstats resetCounters does not reset or double-count the
# local adapter's CTR_FLAG_NONE snapshot values.
localq_wait_stable_stats() {
	local stats_file="$1"
	local record="$2"
	local sample_count="$3"
	shift 3
	local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	local lines line pattern complete line_count
	while [ "$(date +%s)" -le "$deadline" ]; do
		lines=$(grep -F "${record}: origin=core.queue.local " "$stats_file" 2>/dev/null | tail -n "$sample_count" || true)
		line_count=$(printf '%s\n' "$lines" | sed '/^$/d' | wc -l)
		complete=yes
		if [ "$line_count" -eq "$sample_count" ]; then
			while IFS= read -r line; do
				for pattern in "$@"; do
					case " $line " in
						*" $pattern "*) ;;
						*) complete=no; break 2 ;;
					esac
				done
			done <<EOF
$lines
EOF
		else
			complete=no
		fi
		if [ "$complete" = yes ]; then
			printf 'local queue stats stayed stable for %s samples: %s\n' "$sample_count" "$record"
			return
		fi
		$TESTTOOL_DIR/msleep 100
	done
	printf 'FAIL: local queue stats did not stay stable for %s samples: %s\n' "$sample_count" "$record"
	printf '%s\n' "$lines"
	error_exit 1
}

# The callback opens this FIFO for reading after it has published enter_file.
# Opening the writer is therefore an acknowledgement, not a duration-based
# release.  Call only after wait_file_lines has observed the marker.
localq_release_barrier() {
	printf 'release\n' > "$1" || error_exit $?
}

# Local queues qualify every action, including diag.sh's own startup marker.
# The checker requires that omfile destination to be absolute. Rewrite only
# this generated per-test marker after generate_conf, before add_conf appends
# the local-queue graph.
localq_make_startup_marker_absolute() {
	local config="${TESTCONF_NM}.conf"
	local relative="./${RSYSLOG_DYNNAME}.started"
	local absolute="$PWD/${RSYSLOG_DYNNAME}.started"
	local temporary="${config}.localq"
	case "$RSYSLOG_OUT_LOG" in
		/*) ;;
		*) export RSYSLOG_OUT_LOG="$PWD/$RSYSLOG_OUT_LOG" ;;
	esac
	# Define the ordinary template before the generated action. The local graph
	# deliberately rejects the default generated template because it is a
	# generated template rather than the explicit string template contract.
	# Construct a replacement file rather than using sed's nonportable insert
	# command: BSD sed and GNU sed use incompatible -i forms here.
	{
		printf '%s\n' 'template(name="localdiag" type="string" string="%msg%\\n")'
		cat "$config"
	} > "$temporary" || error_exit $?
	mv "$temporary" "$config" || error_exit $?
	sed -i.localq "s|file=\"${relative}\"|file=\"${absolute}\" template=\"localdiag\"|" "$config" || error_exit $?
	rm -f "${config}.localq"
}
