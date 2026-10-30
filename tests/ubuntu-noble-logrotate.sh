#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Rotate Noble's rsyslog.log and mail.err fixtures. Exact archived content,
# delayed compression, missing/empty files, and a once-per-cycle postrotate hook
# are the oracle; no host logs or services are touched.
set -eu
command -v logrotate >/dev/null 2>&1 || exit 77
srcdir=${srcdir:-.}
config="$srcdir/../packaging/ubuntu/noble/v8-stable/debian/rsyslog.logrotate"
test -f "$config" || exit 77
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
chmod 755 "$work"
mkdir "$work/log"
sed -e "s|/var/log/|$work/log/|g" \
    -e "s|/usr/lib/rsyslog/rsyslog-rotate|echo rotated >> $work/hooks|" \
    "$config" > "$work/logrotate.conf"
printf 'first record\n' > "$work/log/rsyslog.log"
printf 'mail error\n' > "$work/log/mail.err"
printf 'syslog control\n' > "$work/log/syslog"
logrotate -f -s "$work/state" "$work/logrotate.conf"
test -f "$work/log/rsyslog.log.1" || {
    echo 'Noble policy did not rotate rsyslog.log' >&2
    exit 1
}
test "$(cat "$work/log/rsyslog.log.1")" = 'first record'
test "$(cat "$work/log/mail.err.1")" = 'mail error'
test "$(cat "$work/log/syslog.1")" = 'syslog control'
test ! -f "$work/log/rsyslog.log.1.gz"
test "$(wc -l < "$work/hooks")" -eq 1
printf 'second record\n' > "$work/log/rsyslog.log"
logrotate -f -s "$work/state" "$work/logrotate.conf"
test "$(cat "$work/log/rsyslog.log.1")" = 'second record'
test "$(gzip -cd "$work/log/rsyslog.log.2.gz")" = 'first record'
test "$(wc -l < "$work/hooks")" -eq 2
: > "$work/log/rsyslog.log"
logrotate -f -s "$work/state" "$work/logrotate.conf"
test "$(wc -l < "$work/hooks")" -eq 2
echo 'Noble rsyslog.log and mail.err rotation passed'
