# This test is similar to tcpsndrcv, but it forwards messages in
# zlib-compressed format (our own syslog extension).
# rgerhards, 2009-11-11
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[sndrcv_gzip.sh\]: testing sending and receiving via tcp in zlib mode
source $srcdir/sndrcv_drvr.sh sndrcv_gzip 50000
