#!/bin/bash
# 
# this shell script provides automatic coredump analysis with gdb. 
# begun 2015-10-14 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0

echo "----------------------------------"
echo "--- Coredump Analysis with GDB ---"
echo "----------------------------------"
read -p "Where are the core files localed (default /)?" DIRECTORY
if [ "$DIRECTORY" == "" ]; then
	DIRECTORY="/"
fi

COREFILES=`ls $DIRECTORY/core* 2> /dev/null`
# &> /dev/null`

select COREFILE in $COREFILES
do
	echo "Which coredump do you want to analyse? "
	break;
done

# Check for found core files
if [ "$COREFILE" == "" ]; then
	echo "Failed to find any coredump files in $DIRECTORY or invalid selection."
	exit;
fi

# generating tmp commandfile
echo "Trying to analyze core for main rsyslogd binary"
echo "set pagination off" >gdb.in
echo "core $COREFILE" >>gdb.in
echo "info thread" >> gdb.in
echo "thread apply all bt full" >> gdb.in
echo "q" >> gdb.in

# Run GDB with commandfile
gdb /usr/sbin/rsyslogd < gdb.in

# rm tmp commandfile
rm gdb.in
