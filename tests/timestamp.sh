#!/bin/bash
echo \[timestamp.sh\]: various timestamp tests
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-HOSTNAME
. $srcdir/diag.sh nettester ts3164 udp
. $srcdir/diag.sh nettester ts3164 tcp
. $srcdir/diag.sh nettester ts3339 udp
. $srcdir/diag.sh nettester ts3339 tcp
. $srcdir/diag.sh nettester tsmysql udp
. $srcdir/diag.sh nettester tsmysql tcp
. $srcdir/diag.sh nettester tspgsql udp
. $srcdir/diag.sh nettester tspgsql tcp
. $srcdir/diag.sh nettester subsecond udp
. $srcdir/diag.sh nettester subsecond tcp
. $srcdir/diag.sh init
