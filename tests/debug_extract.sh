#!/bin/bash
# This is primarily a tool to extract relevant parts of large debug
# files
# usage:
# $1 - file to search
# $2 - search term
# $3 - number of lines before first match (optional, default 5000)
# $4 - number of lines after first match (optional, default 10000)
# output is directed to stdout - use redirection if desired
#
# Copyright (C) 2018 by Rainer Gerhards, released under ASL 2.0

if [ "$1" == "" ]; then
	echo file name must be given
	exit 1
fi
if [ "$2" == "" ]; then
	echo search term must be given
	exit 1
fi
FILE="$1"
SEARCH_TERM="$2"
if [ "$3" == "" ]; then
	LINES_BEFORE=5000
else
	LINES_BEFORE="$3"
fi
if [ "$4" == "" ]; then
	LINES_AFTER=5000
else
	LINES_AFTER="$4"
fi
line=$(cat -n $FILE | grep "$SEARCH_TERM" |cut -f1 |head -1)
if [ "$line" == "" ]; then
	echo search term not found!
	exit 1
fi
startline=$((line-LINES_BEFORE))
numlines=$((LINES_AFTER + LINES_BEFORE+1)) # +1 for search term line itself!
printf "file %s lines %d to %d, arround first occurence of '%s':\n" \
	$FILE $startline $((startline+numlines)) $SEARCH_TERM
tail -n +$startline $FILE | head -n $numlines
