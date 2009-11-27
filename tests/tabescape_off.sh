echo ===============================================================================
echo \[tabescape_off.sh\]: test for tab escaping off
$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason

./nettester -ttabescape_off -iudp
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test via tcp
./nettester -ttabescape_off -itcp
if [ "$?" -ne "0" ]; then
  exit 1
fi
