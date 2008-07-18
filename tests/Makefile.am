check_PROGRAMS = rt_init rscript
TESTS = $(check_PROGRAMS)

test_files = testbench.h runtime-dummy.c
EXTRA_DIST=1.rstest 2.rstest err1.rstest

rt_init_SOURCES = rt-init.c $(test_files)
rt_init_CPPFLAGS =  -I$(top_srcdir) $(pthreads_cflags) $(rsrt_cflags) 
rt_init_LDADD = $(rsrt_libs) $(zlib_libs) $(pthreads_libs)
rt_init_LDFLAGS = -export-dynamic

rscript_SOURCES = rscript.c $(test_files)
rscript_CPPFLAGS =  -I$(top_srcdir) $(pthreads_cflags) $(rsrt_cflags) 
rscript_LDADD = $(rsrt_libs) $(zlib_libs) $(pthreads_libs)
rscript_LDFLAGS = -export-dynamic
