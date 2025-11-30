#!/bin/bash
## @brief Validate SRV error handling and mutual exclusivity for omfwd targetSrv.
. ${srcdir:=.}/diag.sh init

DNS_PORT=$(get_free_port)
RESOLV_BACKUP="$RSYSLOG_DYNNAME.resolv.bak"
cp /etc/resolv.conf "$RESOLV_BACKUP"
trap 'cp "$RESOLV_BACKUP" /etc/resolv.conf' EXIT
cat > /etc/resolv.conf <<EOFRESOLV
nameserver 127.0.0.1
options port:$DNS_PORT attempts:1 timeout:1
EOFRESOLV

# missing SRV records should fail config check
generate_conf
add_conf '
module(load="builtin:omfwd")
action(type="omfwd" targetSrv="nosrv.test" protocol="tcp")
'

if ../tools/rsyslogd -N1 -f"$RSYSLOG_CONFFILE" -M../runtime/.libs:../.libs; then
    echo "Expected configuration failure when SRV lookup has no answers"
    exit 1
fi

# conflicting parameters should also fail
generate_conf
add_conf '
module(load="builtin:omfwd")
action(type="omfwd" targetSrv="nosrv.test" target="127.0.0.1" protocol="tcp")
'

if ../tools/rsyslogd -N1 -f"$RSYSLOG_CONFFILE" -M../runtime/.libs:../.libs; then
    echo "Expected configuration failure when target and targetSrv are combined"
    exit 1
fi

cp "$RESOLV_BACKUP" /etc/resolv.conf
trap - EXIT
exit_test
