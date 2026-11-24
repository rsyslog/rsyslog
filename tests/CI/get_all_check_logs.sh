#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
for i in tests/*.log ; do
    echo
    echo $i:
    cat $i
done
