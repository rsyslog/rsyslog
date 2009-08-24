if ENABLE_TESTBENCH
TESTRUNS = rt_init rscript
check_PROGRAMS = $(TESTRUNS) ourtail nettester tcpflood chkseq
TESTS = $(TESTRUNS) cfg.sh \
	validation-run.sh \
	imtcp-multiport.sh \
	diskqueue.sh \
	manytcp.sh \
	queue-persist.sh 

if ENABLE_OMSTDOUT
TESTS += omod-if-array.sh parsertest.sh inputname.sh fieldtest.sh
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
	   testsuites/parse1.conf \
	   testsuites/field1.conf \
	   testsuites/1.parse1 \
	   testsuites/2.parse1 \
	   testsuites/3.parse1 \
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
	   testsuites/omod-if-array.conf \
	   testsuites/1.omod-if-array \
	   testsuites/1.field1 \
	   killrsyslog.sh \
	   parsertest.sh \
	   fieldtest.sh \
	   diskqueue.sh \
	   testsuites/diskqueue.conf \
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
	   diag.sh \
	   testsuites/diag-common.conf \
	   queue-persist.sh \
	   queue-persist-drvr.sh \
	   testsuites/queue-persist.conf \
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
