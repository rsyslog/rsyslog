echo test parsertest via udp
./nettester parse1 udp
if [ "$?" -ne "0" ]; then
  exit 1
fi

echo test parsertest via tcp
./nettester parse1 tcp
if [ "$?" -ne "0" ]; then
  exit 1
fi
