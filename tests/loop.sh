#!/bin/bash
# this is a small helper script used to run testbench tests
# repetetively. It is meant for manual use and not included
# in any testbench functionality.
# There are some options commented out, e.g. exit on first
# failuere. If that option is desired, it must be "commented
# in" -- we don't think it's worth to add proper options for that.
# Copyright (2015) by Rainer Gerhards, released under ASL 2.0
RUN=1
MAXRUN=50
SUCCESS=0
FAIL=0
while [ $RUN -le $MAXRUN ]; do
     echo
     echo "###### RUN: " $RUN "success" $SUCCESS "fail" $FAIL
     $1
     if [ "$?" -ne "0" ]; then
     	 let FAIL+=1
	 echo "FAIL!"
	 #vi work
         exit 1
     else
     	let SUCCESS+=1
     fi
     let RUN+=1
done
echo
echo "Nbr success: " $SUCCESS
echo "Nbr fail...: " $FAIL
