# This test is similar to tcpsndrcv, but it forwards messages in
# zlib-compressed format (our own syslog extension).
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[tcpsndrcv_gzip.sh\]: testing sending and receiving via tcp in zlib mode
source $srcdir/tcpsndrcv_drvr.sh tcpsndrcv_gzip 50000
