echo \[omod-if-array.sh\]: test omod-if-array via udp
$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason

./nettester -tomod-if-array -iudp -p4711
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test omod-if-array via tcp
./nettester -tomod-if-array -itcp
if [ "$?" -ne "0" ]; then
  exit 1
fi

