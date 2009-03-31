TESTRUNS = rt_init rscript parsertest
check_PROGRAMS = $(TESTRUNS) ourtail
TESTS = $(TESTRUNS) cfg.sh
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
	   testruns/parser.conf \
	   testruns/1.parse1 \
   	   testruns/rfc3164.parse1 \
	   testruns/rfc5424-1.parse1 \
	   testruns/rfc5424-2.parse1 \
	   testruns/rfc5424-3.parse1 \
	   testruns/rfc5424-4.parse1 \
	   cfg.sh

ourtail_SOURCES = ourtail.c

parsertest_SOURCES = parsertest.c getline.c
parsertest_LDADD = $(SOL_LIBS)

rt_init_SOURCES = rt-init.c $(test_files)
rt_init_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rt_init_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
rt_init_LDFLAGS = -export-dynamic

rscript_SOURCES = rscript.c getline.c $(test_files)
rscript_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rscript_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
rscript_LDFLAGS = -export-dynamic
