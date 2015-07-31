#!/bin/bash
echo "\$OMFileAsyncWriting off" > rsyslog.action.1.include
. $srcdir/dynfile_cachemiss.sh
