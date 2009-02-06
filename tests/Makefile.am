check_PROGRAMS = rt_init rscript
TESTS = $(check_PROGRAMS) cfg.sh
TESTS_ENVIRONMENT = RSYSLOG_MODDIR='$(abs_top_builddir)'/runtime/.libs/
#TESTS = $(check_PROGRAMS)

test_files = testbench.h runtime-dummy.c
EXTRA_DIST=1.rstest 2.rstest err1.rstest \
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
	   NoExistFile.cfgtest

rt_init_SOURCES = rt-init.c $(test_files)
rt_init_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rt_init_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS)
rt_init_LDFLAGS = -export-dynamic

rscript_SOURCES = rscript.c $(test_files)
rscript_CPPFLAGS =  -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
rscript_LDADD = $(RSRT_LIBS) $(ZLIB_LIBS) $(PTHREADS_LIBS)
rscript_LDFLAGS = -export-dynamic
