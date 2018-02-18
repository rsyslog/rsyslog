#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

echo ===============================================================================
echo '[omprog-feedback.sh]: test omprog with confirmMessages flag enabled'

. $srcdir/diag.sh init
. $srcdir/diag.sh startup omprog-feedback.conf
. $srcdir/diag.sh wait-startup

. $srcdir/diag.sh injectmsg 0 10

. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

expected_output="<= OK
=> msgnum:00000000:
<= OK
=> msgnum:00000001:
<= OK
=> msgnum:00000002:
<= OK
=> msgnum:00000003:
<= OK
=> msgnum:00000004:
<= Error: could not process log message
=> msgnum:00000004:
<= Error: could not process log message
=> msgnum:00000004:
<= OK
=> msgnum:00000005:
<= OK
=> msgnum:00000006:
<= OK
=> msgnum:00000007:
<= Error: could not process log message
=> msgnum:00000007:
<= Error: could not process log message
=> msgnum:00000007:
<= OK
=> msgnum:00000008:
<= OK
=> msgnum:00000009:
<= OK"

written_output=$(<rsyslog.out.log)
if [[ "x$expected_output" != "x$written_output" ]]; then
    echo unexpected omprog script output:
    echo "$written_output"
    . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
