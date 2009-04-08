#!/bin/bash -e
echo test omod-if-array via udp
./nettester omod-if-array udp
echo test omod-if-array via tcp
./nettester omod-if-array tcp
