# this shell script provides commands to the common diag system. It enables
# test scripts to wait for certain conditions and initiate certain actions.
# needs support in config file.
# NOTE: this file should be included with "source diag.sh", as it otherwise is
# not always able to convey back states to the upper-level test driver
# begun 2009-05-27 by rgerhards
# This file is part of the rsyslog project, released under GPLv3
#valgrind="valgrind --log-fd=1"
#valgrind="valgrind --tool=drd --log-fd=1"
#valgrind="valgrind --tool=helgrind --log-fd=1"
#valgrind="valgrind --tool=exp-ptrcheck --log-fd=1"
#set -o xtrace
#export RSYSLOG_DEBUG="debug nostdout printmutexaction"
#export RSYSLOG_DEBUGLOG="log"
case $1 in
   'init')	$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
		cp $srcdir/testsuites/diag-common.conf diag-common.conf
		cp $srcdir/testsuites/diag-common2.conf diag-common2.conf
		rm -f rsyslog.action.*.include
		rm -f rsyslogd.started work-*.conf
		rm -f rsyslogd2.started work-*.conf
		rm -f work rsyslog.out.log rsyslog.out.log.save # common work files
		rm -f rsyslog.out.*.log
		rm -rf test-spool
		rm -f core.* vgcore.*
		mkdir test-spool
		;;
   'exit')	rm -f rsyslogd.started work-*.conf diag-common.conf
   		rm -f rsyslogd2.started diag-common2.conf rsyslog.action.*.include
		rm -f work rsyslog.out.log rsyslog.out.log.save # common work files
		rm -f rsyslog.out.*.log
		rm -rf test-spool
		;;
   'startup')   # start rsyslogd with default params. $2 is the config file name to use
   		# returns only after successful startup, $3 is the instance (blank or 2!)
		$valgrind ../tools/rsyslogd -c4 -u2 -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/$2 &
   		$srcdir/diag.sh wait-startup $3
		;;
   'wait-startup') # wait for rsyslogd startup ($2 is the instance)
		while test ! -f rsyslogd$2.started; do
			#true
			sleep 0.1 # if this is not supported by all platforms, use above!
		done
		echo "rsyslogd$2 started with pid " `cat rsyslog$2.pid`
		;;
   'wait-shutdown')  # actually, we wait for rsyslog.pid to be deleted. $2 is the
   		# instance
		while test -f rsyslog$2.pid; do
			#true
			sleep 0.1 # if this is not supported by all platforms, use above!
		done
		;;
   'wait-queueempty') # wait for main message queue to be empty. $2 is the instance.
		if [ "$2" == "2" ]
		then
			echo WaitMainQueueEmpty | java -classpath $abs_top_builddir DiagTalker
		else
			echo WaitMainQueueEmpty | java -classpath $abs_top_builddir DiagTalker 13501
		fi
		;;
   'shutdown-when-empty') # shut rsyslogd down when main queue is empty. $2 is the instance.
   		$srcdir/diag.sh wait-queueempty $2
		kill `cat rsyslog$2.pid`
		# note: we do not wait for the actual termination!
		;;
   'shutdown-immediate') # shut rsyslogd down without emptying the queue. $2 is the instance.
		kill `cat rsyslog.pid`
		# note: we do not wait for the actual termination!
		;;
   'tcpflood') # do a tcpflood run and check if it worked params are passed to tcpflood
		./tcpflood $2 $3 $4 $5 $6 $7 $8 $9
		if [ "$?" -ne "0" ]; then
		  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
		  cp rsyslog.out.log rsyslog.out.log.save
		  exit 1
		fi
		;;
   'injectmsg') # inject messages via our inject interface (imdiag)
		echo injecting $3 messages
		echo injectmsg $2 $3 $4 $5 | java -classpath $abs_top_builddir DiagTalker
		# TODO: some return state checking? (does it really make sense here?)
		;;
   'check-mainq-spool') # check if mainqueue spool files exist, if not abort (we just check .qi).
		echo There must exist some files now:
		ls -l test-spool
		if test ! -f test-spool/mainq.qi; then
		  echo "error: mainq.qi does not exist where expected to do so!"
		  ls -l test-spool
		  exit 1
		fi
		;;
   'seq-check') # do the usual sequence check to see if everything was properly received. $2 is the instance.
		rm -f work
		cp rsyslog.out.log work-presort
		sort < rsyslog.out.log > work
		# $4... are just to have the abilit to pass in more options...
		./chkseq -fwork -v -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  exit 1
		fi
		;;
   'seq-check2') # do the usual sequence check to see if everything was properly received. This is
   		# a duplicateof seq-check, but we could not change its calling conventions without
		# breaking a lot of exitings test cases, so we preferred to duplicate the code here.
		rm -f work2
		sort < rsyslog2.out.log > work2
		# $4... are just to have the abilit to pass in more options...
		./chkseq -fwork2 -v -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  exit 1
		fi
		;;
   'gzip-seq-check') # do the usual sequence check, but for gzip files
		rm -f work
		ls -l rsyslog.out.log
		gunzip < rsyslog.out.log | sort > work
		ls -l work
		# $4... are just to have the abilit to pass in more options...
		./chkseq -fwork -v -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  exit 1
		fi
		;;
   'nettester') # perform nettester-based tests
   		# use -v for verbose output!
		./nettester -t$2 -i$3
		if [ "$?" -ne "0" ]; then
		  exit 1
		fi
		;;
   *)		echo "invalid argument" $1
esac
