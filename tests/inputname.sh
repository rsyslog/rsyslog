echo \[inputname.sh\]: testing $InputTCPServerInputName directive
$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason

echo port 12514
./nettester -tinputname_imtcp_12514 -cinputname_imtcp -itcp -p12514
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo port 12515
./nettester -tinputname_imtcp_12515 -cinputname_imtcp -itcp -p12515
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo port 12516
./nettester -tinputname_imtcp_12516 -cinputname_imtcp -itcp -p12516
if [ "$?" -ne "0" ]; then
  exit 1
fi
