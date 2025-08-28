#! /bin/bash
# add 2019-04-04 by Philippe DUVEAU, released under ASL 2.0
mysleep=./msleep
ACK=0
SLEEP=0
DELAY=500
NB=1
MESSAGE="program data"
SIGNALED=0
ERR=$0.stderr
while getopts "cd:e:s:n:m:g" OPTION; do
	case "${OPTION}" in 
		g)
			SIGNALED=1
			;;
		c)
			ACK=1
			;;
		d)
			DELAY=${OPTARG}
			;;
		e)
			ERR=${OPTARG}
			;;
		s)
			SLEEP=${OPTARG}
			;;
		n)
			NB=${OPTARG}
			;;
		m)
			MESSAGE=${OPTARG}
			;;
		*)
			exit 0
	esac
done
trap 'echo "SIGTERM Received" >> '$ERR';echo $0" SIGTERM Received" >&2;exit 0' 15
if (( DELAY > 0 )); then $mysleep ${DELAY}; fi
if [ ${ACK} == 1 ]; then
	while [ "x$order" != "xSTART" ]; do 
		read -r order
		echo $order' Received' >> $ERR
		echo $0' '$order' Received' >&2
	done
	while [ "x$order" != "xSTOP" ]; do
		if (( NB > 0 )); then
			echo ${MESSAGE}
			echo "Message sent" >&2
			(( NB-- ))
		fi
		unset order
		read -r order
		echo $order' Received'  >> $ERR
		echo $0' '$order' Received' >&2
		if (( SLEEP > 0 )); then $mysleep ${SLEEP}; fi
	done
else
	while (( NB > 0 )); do
		echo ${MESSAGE}
		echo $0" Message sent" >&2
		if (( SLEEP > 0 )); then $mysleep ${SLEEP}; fi
		(( NB-- ))
	done
	if [ ${SIGNALED} == 1 ]; then 
		$mysleep 100000 &
		wait
	fi
fi
echo "Leaving improg_sender" >&2
