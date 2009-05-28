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
#set -o xtrace
#export RSYSLOG_DEBUG="debug nostdout printmutexaction"
#export RSYSLOG_DEBUGLOG="log"
case $1 in
   'init')	$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
		cp $srcdir/testsuites/diag-common.conf diag-common.conf
		rm -f rsyslogd.started work-*.conf
		rm -f work rsyslog.out.log rsyslog.out.log.save # common work files
		rm -rf test-spool
		mkdir test-spool
		;;
   'exit')	rm -f rsyslogd.started work-*.conf diag-common.conf
		rm -f work rsyslog.out.log rsyslog.out.log.save # common work files
		rm -rf test-spool
		;;
   'startup')   # start rsyslogd with default params. $2 is the config file name to use
   		# returns only after successful startup
		$valgrind ../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/$2 &
   		$srcdir/diag.sh wait-startup
		;;
   'wait-startup') # wait for rsyslogd startup
		while test ! -f rsyslogd.started; do
			true
		done
		echo "rsyslogd started with pid " `cat rsyslog.pid`
		;;
   'wait-shutdown')  # actually, we wait for rsyslog.pid to be deleted
		while test -f rsyslog.pid; do
			true
		done
		;;
   'wait-queueempty') # wait for main message queue to be empty
		echo WaitMainQueueEmpty | java -classpath $abs_top_builddir DiagTalker
		;;
   'shutdown-when-empty') # shut rsyslogd down when main queue is empty
   		$srcdir/diag.sh wait-queueempty
		kill `cat rsyslog.pid`
		# note: we do not wait for the actual termination!
		;;
   'shutdown-immediate') # shut rsyslogd down without emptying the queue
		kill `cat rsyslog.pid`
		# note: we do not wait for the actual termination!
		;;
   'tcpflood') # do a tcpflood run and check if it worked params are passed to tcpflood
		./tcpflood $2 $3 $4 $5 $6 $7 $8
		if [ "$?" -ne "0" ]; then
		  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
		  cp rsyslog.out.log rsyslog.out.log.save
		  exit 1
		fi
		;;
   'injectmsg') # inject messages via our inject interface (imdiag)
		echo injectmsg $2 $3 $4 $5 | java -classpath $abs_top_builddir DiagTalker
		# TODO: some return state checking? (does it really make sense here?)
		;;
   'check-mainq-spool') # check if mainqueue spool files exist, if not abort (we just check .qi)
		echo There must exist some files now:
		ls -l test-spool
		if test ! -f test-spool/mainq.qi; then
		  echo "error: mainq.qi does not exist where expected to do so!"
		  ls -l test-spool
		  exit 1
		fi
		;;
   'seq-check') # do the usual sequence check to see if everything was properly received
		rm -f work
		sort < rsyslog.out.log > work
		./chkseq -fwork -e$2 $3
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  exit 1
		fi
		;;
   *)		echo "invalid argument" $1
esac
