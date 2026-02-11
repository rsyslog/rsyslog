#!/bin/bash
# This test tests listening on ports in another network namespace.
echo ===============================================================================
echo \[imtcp-netns.sh\]: test for listening in another network namespace
echo This test must be run with CAP_SYS_ADMIN capabilities [network namespace creation/change required]
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi

. ${srcdir:=.}/diag.sh init
require_netns_capable
generate_conf

NS_PREFIX=$(basename ${RSYSLOG_DYNNAME})
NS_DEFAULT="${NS_PREFIX}.rsyslog_ns_default"
NS_FAIL=(
   "${NS_PREFIX}.rsyslog_ns_a_fail"
   "${NS_PREFIX}.rsyslog_ns_b_fail"
)
NS_GOOD=(
   "${NS_PREFIX}.rsyslog_ns_c"
   "${NS_PREFIX}.rsyslog_ns_d"
)

add_conf '
$MainMsgQueueTimeoutShutdown 10000
module(
    load="../plugins/imtcp/.libs/imtcp"
    NetworkNamespace="'${NS_DEFAULT}'"
)
input(
    Type="imtcp"
    Port="0"
    Address="127.0.0.1"
    ListenPortFileName="'${NS_DEFAULT}'.port"
)
'

for ns in "${NS_GOOD[@]}" "${NS_FAIL[@]}"; do
    add_conf '
        input(
            Type="imtcp"
            Port="0"
            Address="127.0.0.1"
            NetworkNamespace="'${ns}'"
            ListenPortFileName="'${ns}'.port"
        )
    '
done

add_conf '
input(
    Type="imtcp"
    Port="0"
    Address="127.0.0.1"
    NetworkNamespace=""
    ListenPortFileName="'$RSYSLOG_DYNNAME'.port"
)

template(name="outfmt" type="string" string="%msg:%\n")
:msg, contains, "imtcp-netns:" action(
    type="omfile"
    file="'${RSYSLOG_OUT_LOG}'"
    template="outfmt"
)
'
#
# create network namespace and bring it up
NS=(
    "${NS_DEFAULT}"
    "${NS_GOOD[@]}"
)
for ns in "${NS[@]}"; do
    ip netns add "${ns}"
    ip netns exec "${ns}" ip link set dev lo up
done
for ns in "${NS_FAIL[@]}"; do
    ip netns delete "${ns}" > /dev/null 2>&1
done

# now do the usual run
RS_REDIR="> ${RSYSLOG_DYNNAME}.log 2>&1"
startup
wait_file_exists "${RSYSLOG_DYNNAME}.port"
for ns in "${NS[@]}"; do
    wait_file_exists "${ns}.port"
done

MSGS=()
function logmsg() {
    local ns=$1
    local msg=$2
    local logger_cmd=()
    local port_file=${RSYSLOG_DYNNAME}.port

    if [[ -n "${ns}" ]]; then
        logger_cmd+=(ip netns exec "${ns}")
        port_file="${ns}.port"
    fi
    logger_cmd+=(
        logger
        --tcp
        --server 127.0.0.1
        --octet-count
        --tag "$0"
        --port "$(cat ${port_file})"
        --
        "imtcp-netns: ${msg}"
    )
    "${logger_cmd[@]}"
    MSGS+=("imtcp-netns: ${msg}")
}

# Inject a few messages
logmsg "" "start message from local namespace"
for ns in "${NS[@]}"; do
    logmsg "${ns}" "test message from namespace ${ns}"
    # Try to keep them ordered
    sleep 1
done
logmsg "" "end message from local namespace"

shutdown_when_empty
wait_shutdown

# remove network namespaces
for ns in "${NS[@]}"; do
    ip netns delete "${ns}"
done

EXPECTED=$(printf "%s\n" "${MSGS[@]}")
cmp_exact

# Verify we have error messages for the missing namespaces
# We don't redirect with valgrind, so skip this check with valgrind
if [[ "${USE_VALGRIND}" != "YES" ]]; then
    for ns in "${NS_FAIL[@]}"; do
        content_check "netns_switch: could not open namespace '${ns}':" ${RSYSLOG_DYNNAME}.log
    done
fi
exit_test
