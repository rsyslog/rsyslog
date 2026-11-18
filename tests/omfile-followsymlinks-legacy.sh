#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Validate that legacy selector omfile actions follow a final-component
# symlink by default (compatibility.defaults.secure=warn), as documented.
# Regression: the legacy path left followSymlinks unset, so O_NOFOLLOW was
# always applied (visible on Solaris where /dev/null is a symlink).
# The oracle is the symlink target content after synchronized shutdown.
. ${srcdir:=.}/diag.sh init

link="./${RSYSLOG_DYNNAME}.link"
target="${RSYSLOG_DYNNAME}.target"
rm -f "$link" "$target"
: >"$target"
ln -s "$target" "$link" || skip_test

generate_conf
add_conf '
$template legacyfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" '${link}';legacyfmt
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
content_check "00000000" "$target"
exit_test
