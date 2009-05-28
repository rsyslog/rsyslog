<<<<<<< HEAD:tests/Makefile.am
TESTRUNS = rt_init rscript
check_PROGRAMS = $(TESTRUNS) ourtail udptester
TESTS = $(TESTRUNS) cfg.sh parsertest.sh omod-if-array.sh \
	validation-run.sh
TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
DISTCLEANFILES=rsyslog.pid
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
	   testsuites/1.parse1 \
   	   testsuites/rfc3164.parse1 \
	   testsuites/rfc5424-1.parse1 \
	   testsuites/rfc5424-2.parse1 \
	   testsuites/rfc5424-3.parse1 \
	   testsuites/rfc5424-4.parse1 \
	   testsuites/omod-if-array.conf \
	   testsuites/1.omod-if-array \
	   parsertest.sh \
	   omod-if-array.sh \
	   cfg.sh

ourtail_SOURCES = ourtail.c

udptester_SOURCES = udptester.c getline.c
udptester_LDADD = $(SOL_LIBS)

rt_init_SOURCES = rt-init.c $(test_files)
rt_init_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rt_init_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
rt_init_LDFLAGS = -export-dynamic

rscript_SOURCES = rscript.c getline.c $(test_files)
rscript_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rscript_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS) $(SOL_LIBS)
rscript_LDFLAGS = -export-dynamic
