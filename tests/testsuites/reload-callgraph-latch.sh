#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# omprog acknowledgement is withheld until the test releases this caller.
echo OK
while IFS= read -r message; do
    printf '%s\n' "$message" > "$1"
    IFS= read -r release < "$2" || exit 1
    test "$release" = release || exit 1
    echo OK
done
