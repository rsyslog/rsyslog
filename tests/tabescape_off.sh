#!/bin/bash
echo ===============================================================================
echo \[tabescape_off.sh\]: test for tab escaping off
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-HOSTNAME

./nettester -ttabescape_off -iudp
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test via tcp
./nettester -ttabescape_off -itcp
if [ "$?" -ne "0" ]; then
  exit 1
fi
