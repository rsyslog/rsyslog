if ENABLE_TESTBENCH
TESTRUNS = rt_init rscript
check_PROGRAMS = $(TESTRUNS) ourtail nettester tcpflood chkseq
TESTS = $(TESTRUNS) cfg.sh \
	arrayqueue.sh \
	linkedlistqueue.sh \
	da-mainmsg-q.sh \
	validation-run.sh \
	imtcp-multiport.sh \
	daqueue-persist.sh \
	diskqueue.sh \
	diskqueue-fsync.sh \
	manytcp.sh \
	queue-persist.sh 

if ENABLE_OMSTDOUT
TESTS += omod-if-array.sh \
	 proprepltest.sh \
	 parsertest.sh \
	 timestamp.sh \
	 inputname.sh \
	 threadingmq.sh \
	 threadingmqaq.sh \
	 discard.sh \
	 fieldtest.sh
endif

check_JAVA = DiagTalker.java

endif # if ENABLE_TESTBENCH

TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
DISTCLEANFILES=rsyslog.pid '$(abs_top_builddir)'/DiagTalker.class
test_files = testbench.h runtime-dummy.c

EXTRA_DIST= 1.rstest 2.rstest 3.rstest err1.rstest \
	   validation-run.sh \
	   testsuites/invalid.conf \
	   testsuites/valid.conf \
	   cfg.sh \
	   cfg1.cfgtest \
	   cfg1.testin \
	   cfg2.cfgtest \
	   cfg2.testin \
	   cfg3.cfgtest \
	   cfg3.testin \
	   cfg4.cfgtest \
	   cfg4.testin \
	   DevNull.cfgtest \
	   err1.rstest \
	   NoExistFile.cfgtest \
	   timestamp.sh \
	   testsuites/ts3164.conf \
	   testsuites/mon1digit.ts3164 \
	   testsuites/mon2digit.ts3164 \
	   testsuites/Jan.ts3164 \
	   testsuites/Feb.ts3164 \
	   testsuites/Mar.ts3164 \
	   testsuites/Apr.ts3164 \
	   testsuites/May.ts3164 \
	   testsuites/Jun.ts3164 \
	   testsuites/Jul.ts3164 \
	   testsuites/Aug.ts3164 \
	   testsuites/Sep.ts3164 \
	   testsuites/Oct.ts3164 \
	   testsuites/Nov.ts3164 \
	   testsuites/Dec.ts3164 \
	   testsuites/ts3339.conf \
	   testsuites/master.ts3339 \
	   testsuites/tsmysql.conf \
	   testsuites/master.tsmysql \
	   testsuites/tspgsql.conf \
	   testsuites/master.tspgsql \
	   testsuites/subsecond.conf \
	   testsuites/master.subsecond \
	   testsuites/parse1.conf \
	   testsuites/field1.conf \
	   testsuites/1.parse1 \
	   testsuites/2.parse1 \
	   testsuites/3.parse1 \
	   testsuites/4.parse1 \
	   testsuites/oversizeTag-1.parse1 \
	   testsuites/date1.parse1 \
	   testsuites/date2.parse1 \
	   testsuites/date3.parse1 \
	   testsuites/date4.parse1 \
	   testsuites/date5.parse1 \
   	   testsuites/rfc3164.parse1 \
	   testsuites/rfc5424-1.parse1 \
	   testsuites/rfc5424-2.parse1 \
	   testsuites/rfc5424-3.parse1 \
	   testsuites/rfc5424-4.parse1 \
	   testsuites/malformed.parse1 \
	   testsuites/reallife.parse1 \
	   testsuites/parse2.conf \
	   testsuites/reallife.parse2 \
	   testsuites/omod-if-array.conf \
	   testsuites/1.omod-if-array \
	   testsuites/1.field1 \
	   killrsyslog.sh \
	   parsertest.sh \
	   fieldtest.sh \
	   diskqueue.sh \
	   testsuites/diskqueue.conf \
	   arrayqueue.sh \
	   testsuites/arrayqueue.conf \
	   linkedlistqueue.sh \
	   testsuites/linkedlistqueue.conf \
	   da-mainmsg-q.sh \
	   testsuites/da-mainmsg-q.conf \
	   diskqueue-fsync.sh \
	   testsuites/diskqueue-fsync.conf \
	   imtcp-multiport.sh \
	   testsuites/imtcp-multiport.conf \
	   manytcp.sh \
	   testsuites/manytcp.conf \
	   inputname.sh \
	   testsuites/inputname_imtcp.conf \
	   testsuites/1.inputname_imtcp_12514 \
	   testsuites/1.inputname_imtcp_12515 \
	   testsuites/1.inputname_imtcp_12516 \
	   omod-if-array.sh \
	   discard.sh \
	   testsuites/discard.conf \
	   diag.sh \
	   testsuites/diag-common.conf \
           daqueue-persist.sh \
	   daqueue-persist-drvr.sh \
	   queue-persist.sh \
	   queue-persist-drvr.sh \
	   testsuites/queue-persist.conf \
	   threadingmq.sh \
	   testsuites/threadingmq.conf \
	   threadingmqaq.sh \
	   testsuites/threadingmqaq.conf \
	   proprepltest.sh \
	   testsuites/rfctag.conf \
	   testsuites/master.rfctag \
	   testsuites/nolimittag.conf \
	   testsuites/master.nolimittag \
	   DiagTalker.java \
	   cfg.sh

ourtail_SOURCES = ourtail.c
chkseq_SOURCES = chkseq.c

tcpflood_SOURCES = tcpflood.c
tcpflood_LDADD = $(SOL_LIBS)

nettester_SOURCES = nettester.c getline.c
nettester_LDADD = $(SOL_LIBS)

rt_init_SOURCES = rt-init.c $(test_files)
rt_init_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rt_init_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
rt_init_LDFLAGS = -export-dynamic

rscript_SOURCES = rscript.c getline.c $(test_files)
rscript_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rscript_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
rscript_LDFLAGS = -export-dynamic
