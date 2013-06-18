echo \[timestamp.sh\]: various timestamp tests
source $srcdir/diag.sh init
source $srcdir/diag.sh nettester ts3164 udp
source $srcdir/diag.sh nettester ts3164 tcp
source $srcdir/diag.sh nettester ts3339 udp
source $srcdir/diag.sh nettester ts3339 tcp
source $srcdir/diag.sh nettester tsmysql udp
source $srcdir/diag.sh nettester tsmysql tcp
source $srcdir/diag.sh nettester tspgsql udp
source $srcdir/diag.sh nettester tspgsql tcp
source $srcdir/diag.sh nettester subsecond udp
source $srcdir/diag.sh nettester subsecond tcp
source $srcdir/diag.sh init
