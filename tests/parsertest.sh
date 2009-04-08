#!/bin/bash -e
echo test parsertest via udp
./nettester parse1 udp
echo test parsertest via tcp
./nettester parse1 tcp
