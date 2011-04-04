# rgerhards, 2011-04-04
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[sndrcv_tls_anon_rebind.sh\]: testing sending and receiving via TLS with anon auth and rebind
source $srcdir/sndrcv_drvr.sh sndrcv_tls_anon_rebind 25000
