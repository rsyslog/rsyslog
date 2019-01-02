#!/bin/bash
RUN=1
MAXRUN=10000000
while [ $RUN -le $MAXRUN ]; do
     ls -l
     let RUN+=1
done
