echo test parsertest via udp
$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason

./nettester -tparse1 -iudp
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test parsertest via tcp
./nettester -tparse1 -itcp
if [ "$?" -ne "0" ]; then
  exit 1
fi
