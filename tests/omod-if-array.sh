echo test omod-if-array via udp
./nettester omod-if-array udp
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test omod-if-array via tcp
./nettester omod-if-array tcp
if [ "$?" -ne "0" ]; then
  exit 1
fi

