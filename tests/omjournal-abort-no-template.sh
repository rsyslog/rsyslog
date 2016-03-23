#!/bin/bash
# a very basic test for omjournal. Right now, we have no
# reliable way of verifying that data was actually written
# to the journal, but at least we check that rsyslog does
# not abort when trying to use omjournal. Not high tech,
# but better than nothing.
# addd 2016-03-16 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/omjournal/.libs/omjournal")

action(type="omjournal")
'
. $srcdir/diag.sh startup
./msleep 500
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
# if we reach this, we have at least not aborted
. $srcdir/diag.sh exit
