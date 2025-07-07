#!/bin/bash
# Test for checking the fileCreateMode imjournal parameter
# Basically we set 3 different file creation modes for the state file
# and test if those are really set
# 
. ${srcdir:=.}/diag.sh init

# $1 - the file create mode that we set for the state file
test_imjournal_filecreatemode() {
  local EXPECTED=$1

  generate_conf
  add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(
  load="../plugins/imjournal/.libs/imjournal"
  StateFile="imjournal.state"
  fileCreateMode="'$EXPECTED'"
)
'
  startup

  local ACTUAL="$(stat -c "%#a" "$RSYSLOG_DYNNAME.spool/imjournal.state")"
  if [ "$ACTUAL" != "$EXPECTED" ]; then
    echo "imjournal fileCreateMode failed, incorrect permissions on state file: expected($EXPECTED) != actual($ACTUAL)"
    error_exit
  fi
  shutdown_when_empty
  wait_shutdown
  ls -l "$RSYSLOG_DYNNAME.spool/imjournal.state"
}

test_imjournal_filecreatemode "0600"
test_imjournal_filecreatemode "0640"
test_imjournal_filecreatemode "0644"

exit_test
