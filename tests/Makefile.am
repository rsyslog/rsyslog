TESTRUNS = rt_init rscript
check_PROGRAMS = $(TESTRUNS) ourtail nettester tcpflood chkseq
TESTS = $(TESTRUNS) cfg.sh manytcp.sh diskqueue.sh imtcp-multiport.sh
if ENABLE_OMSTDOUT
TESTS += omod-if-array.sh parsertest.sh inputname.sh fieldtest.sh
endif
TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
DISTCLEANFILES=rsyslog.pid
test_files = testbench.h runtime-dummy.c

EXTRA_DIST= 1.rstest 2.rstest 3.rstest err1.rstest \
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
	   testsuites/1.parse1 \
	   testsuites/rfc3164.parse1 \
	   testsuites/rfc5424-1.parse1 \
	   testsuites/rfc5424-2.parse1 \
	   testsuites/rfc5424-3.parse1 \
	   testsuites/rfc5424-4.parse1 \
	   testsuites/omod-if-array.conf \
	   testsuites/1.omod-if-array \
	   killrsyslog.sh \
	   parsertest.sh \
	   diskqueue.sh \
	   testsuites/diskqueue.conf \
	   imtcp-multiport.sh \
	   testsuites/imtcp-multiport.conf \
	   manytcp.sh \
	   testsuites/manytcp.conf \
	   fieldtest.sh \
	   testsuites/field1.conf \
	   testsuites/1.field1 \
	   inputname.sh \
	   testsuites/inputname_imtcp.conf \
	   testsuites/1.inputname_imtcp_12514 \
	   testsuites/1.inputname_imtcp_12515 \
	   testsuites/1.inputname_imtcp_12516 \
	   omod-if-array.sh \
	   waitqueueempty.sh \
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
