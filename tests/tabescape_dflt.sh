#!/bin/bash
echo ===============================================================================
echo \[tabescape_dflt.sh\]: test for default tab escaping
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-HOSTNAME

./nettester -ttabescape_dflt -iudp
if [ "$?" -ne "0" ]; then
  echo erorr in udp run
  exit 1
fi

echo test via tcp
./nettester -ttabescape_dflt -itcp
if [ "$?" -ne "0" ]; then
  echo erorr in tcp run
  exit 1
fi
