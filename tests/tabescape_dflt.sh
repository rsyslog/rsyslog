echo ===============================================================================
echo \[tabescape_dflt.sh\]: test for default tab escaping
source $srcdir/diag.sh init
source $srcdir/diag.sh generate-HOSTNAME

./nettester -ttabescape_dflt -iudp
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test via tcp
./nettester -ttabescape_dflt -itcp
if [ "$?" -ne "0" ]; then
  exit 1
fi
