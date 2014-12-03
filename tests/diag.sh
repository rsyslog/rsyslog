# this shell script provides commands to the common diag system. It enables
# test scripts to wait for certain conditions and initiate certain actions.
# needs support in config file.
# NOTE: this file should be included with "source diag.sh", as it otherwise is
# not always able to convey back states to the upper-level test driver
# begun 2009-05-27 by rgerhards
# This file is part of the rsyslog project, released under GPLv3
#valgrind="valgrind --malloc-fill=ff --free-fill=fe --log-fd=1"
#valgrind="valgrind --tool=drd --log-fd=1"
#valgrind="valgrind --tool=helgrind --log-fd=1"
#valgrind="valgrind --tool=exp-ptrcheck --log-fd=1"
#set -o xtrace
#export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
#export RSYSLOG_DEBUGLOG="log"
TB_TIMEOUT_STARTSTOP=3000 # timeout for start/stop rsyslogd in tenths (!) of a second 3000 => 5 min
case $1 in
   'init')	$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
		cp $srcdir/testsuites/diag-common.conf diag-common.conf
		cp $srcdir/testsuites/diag-common2.conf diag-common2.conf
		rm -f rsyslogd.started work-*.conf rsyslog.random.data
		rm -f rsyslogd2.started work-*.conf
		rm -f work rsyslog.out.log rsyslog2.out.log rsyslog.out.log.save # common work files
		rm -rf test-spool test-logdir stat-file1
		rm -f rsyslog.out.*.log work-presort rsyslog.pipe
		rm -f rsyslog.input rsyslog.empty
		rm -f rsyslog.errorfile
		rm -f core.* vgcore.*
		# Note: rsyslog.action.*.include must NOT be deleted, as it
		# is used to setup some parameters BEFORE calling init. This
		# happens in chained test scripts. Delete on exit is fine,
		# though.
		mkdir test-spool
		;;
   'exit')	rm -f rsyslogd.started work-*.conf diag-common.conf
   		rm -f rsyslogd2.started diag-common2.conf rsyslog.action.*.include
		rm -f work rsyslog.out.log rsyslog2.out.log rsyslog.out.log.save # common work files
		rm -rf test-spool test-logdir stat-file1
		rm -f rsyslog.out.*.log rsyslog.random.data work-presort rsyslog.pipe
		rm -f rsyslog.input rsyslog.conf.tlscert stat-file1 rsyslog.empty
		rm -f rsyslog.errorfile
		echo  -------------------------------------------------------------------------------
		;;
   'es-init')   # initialize local Elasticsearch *testbench* instance for the next
                # test. NOTE: do NOT put anything useful on that instance!
		curl -XDELETE localhost:9200/rsyslog_testbench
		;;
   'es-getdata') # read data from ES to a local file so that we can process
   		# it with out regular tooling.
		# Note: param 2 MUST be number of records to read (ES does
		# not return the full set unless you tell it explicitely).
		curl localhost:9200/rsyslog_testbench/_search?size=$2 > work
		python $srcdir/es_response_get_msgnum.py > rsyslog.out.log
		;;
   'startup')   # start rsyslogd with default params. $2 is the config file name to use
   		# returns only after successful startup, $3 is the instance (blank or 2!)
		$valgrind ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/$2 &
   		$srcdir/diag.sh wait-startup $3 || exit $?
		;;
   'startup-vg') # start rsyslogd with default params under valgrind control. $2 is the config file name to use
   		# returns only after successful startup, $3 is the instance (blank or 2!)
		valgrind --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/$2 &
   		$srcdir/diag.sh wait-startup $3 || exit $?
		echo startup-vg still running
		;;
   'startup-vg-noleak') # same as startup-vg, except that --leak-check is set to "none". This
   		# is meant to be used in cases where we have to deal with libraries (and such
		# that) we don't can influence and where we cannot provide suppressions as
		# they are platform-dependent. In that case, we can't test for leak checks
		# (obviously), but we can check for access violations, what still is useful.
		valgrind --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=no ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/$2 &
   		$srcdir/diag.sh wait-startup $3 || exit $?
		echo startup-vg still running
		;;
   'wait-startup') # wait for rsyslogd startup ($2 is the instance)
		i=0
		while test ! -f rsyslog$2.pid; do
			./msleep 100 # wait 100 milliseconds
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on startup (pid file)"
			   exit 1
			fi
		done
		i=0
		while test ! -f rsyslogd$2.started; do
			./msleep 100 # wait 100 milliseconds
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on startup ('started' file)"
			   exit 1
			fi
		done
		echo "rsyslogd$2 started with pid " `cat rsyslog$2.pid`
		;;
   'wait-shutdown')  # actually, we wait for rsyslog.pid to be deleted. $2 is the
   		# instance
		i=0
		while test -f rsyslog$2.pid; do
			./msleep 100 # wait 100 milliseconds
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on shutdown"
			   echo "Instance is possibly still running and may need"
			   echo "manual cleanup."
			   exit 1
			fi
		done
		if [ -e core.* ]
		then
		   echo "ABORT! core file exists, starting interactive shell"
		   bash
		   exit 1
		fi
		;;
   'wait-shutdown-vg')  # actually, we wait for rsyslog.pid to be deleted. $2 is the
   		# instance
		wait `cat rsyslog.pid`
		export RSYSLOGD_EXIT=$?
		echo rsyslogd run exited with $RSYSLOGD_EXIT
		if [ -e vgcore.* ]
		then
		   echo "ABORT! core file exists, starting interactive shell"
		   bash
		   exit 1
		fi
		;;
   'check-exit-vg') # wait for main message queue to be empty. $2 is the instance.
		if [ "$RSYSLOGD_EXIT" -eq "10" ]
		then
			echo "valgrind run FAILED with exceptions - terminating"
			exit 1
		fi
		;;
   'get-mainqueuesize') # show the current main queue size
		if [ "$2" == "2" ]
		then
			echo getmainmsgqueuesize | ./diagtalker -p13501
		else
			echo getmainmsgqueuesize | ./diagtalker
		fi
		;;
   'wait-queueempty') # wait for main message queue to be empty. $2 is the instance.
		if [ "$2" == "2" ]
		then
			echo WaitMainQueueEmpty | ./diagtalker -p13501
		else
			echo WaitMainQueueEmpty | ./diagtalker
		fi
		;;
   'shutdown-when-empty') # shut rsyslogd down when main queue is empty. $2 is the instance.
		if [ "$2" == "2" ]
		then
		   echo Shutting down instance 2
		fi
   		$srcdir/diag.sh wait-queueempty $2 || exit $?
		./msleep 1000 # wait a bit (think about slow testbench machines!)
		kill `cat rsyslog$2.pid`
		# note: we do not wait for the actual termination!
		;;
   'shutdown-immediate') # shut rsyslogd down without emptying the queue. $2 is the instance.
		kill `cat rsyslog.pid`
		# note: we do not wait for the actual termination!
		;;
   'tcpflood') # do a tcpflood run and check if it worked params are passed to tcpflood
		shift 1
		eval ./tcpflood $*
		if [ "$?" -ne "0" ]; then
		  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
		  cp rsyslog.out.log rsyslog.out.log.save
		  exit 1
		fi
		;;
   'injectmsg') # inject messages via our inject interface (imdiag)
		echo injecting $3 messages
		echo injectmsg $2 $3 $4 $5 | ./diagtalker
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
		sort -g < rsyslog.out.log > work
		# $4... are just to have the abilit to pass in more options...
		# add -v to chkseq if you need more verbose output
		./chkseq -fwork -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  exit 1
		fi
		;;
   'seq-check2') # do the usual sequence check to see if everything was properly received. This is
   		# a duplicateof seq-check, but we could not change its calling conventions without
		# breaking a lot of exitings test cases, so we preferred to duplicate the code here.
		rm -f work2
		sort -g < rsyslog2.out.log > work2
		# $4... are just to have the abilit to pass in more options...
		# add -v to chkseq if you need more verbose output
		./chkseq -fwork2 -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  exit 1
		fi
		rm -f work2
		;;
   'content-check') 
		cat rsyslog.out.log | grep -qF "$2"
		if [ "$?" -ne "0" ]; then
		    exit 1
		fi
		;;
   'assert-content-missing') 
		cat rsyslog.out.log | grep -qF "$2"
		if [ "$?" -eq "0" ]; then
		    exit 1
		fi
		;;
   'gzip-seq-check') # do the usual sequence check, but for gzip files
		rm -f work
		ls -l rsyslog.out.log
		gunzip < rsyslog.out.log | sort -g > work
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
		./nettester -t$2 -i$3 $4
		if [ "$?" -ne "0" ]; then
		  exit 1
		fi
		;;
   'setzcat')   # find out name of zcat tool
		if [ `uname` == SunOS ]; then
		   ZCAT=gzcat
		else
		   ZCAT=zcat
		fi
		;;
   'generate-HOSTNAME')   # generate the HOSTNAME file
		source $srcdir/diag.sh startup gethostname.conf || exit $?
		source $srcdir/diag.sh tcpflood -m1 -M "\"<128>\"" || exit $?
		./msleep 100
		source $srcdir/diag.sh shutdown-when-empty || exit $? # shut down rsyslogd when done processing messages
		source $srcdir/diag.sh wait-shutdown || exit $?	# we need to wait until rsyslogd is finished!
		;;
   *)		echo "invalid argument" $1
esac
