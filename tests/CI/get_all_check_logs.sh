#!/bin/bash
for i in tests/*.log ; do
    echo
    echo $i:
    cat $i
done
