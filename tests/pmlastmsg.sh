echo ==============================================================================
echo \[pmlastmsg.sh\]: tests for pmlastmsg
source $srcdir/diag.sh init
source $srcdir/diag.sh nettester pmlastmsg udp
source $srcdir/diag.sh nettester pmlastmsg tcp
source $srcdir/diag.sh exit
