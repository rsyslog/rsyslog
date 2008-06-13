check_PROGRAMS = rt_init rscript_parse
TESTS = $(check_PROGRAMS)

test_files = testbench.h runtime-dummy.c

rt_init_SOURCES = rt-init.c $(test_files)
rt_init_CPPFLAGS =  -I$(top_srcdir) $(pthreads_cflags) $(rsrt_cflags) 
rt_init_LDADD = $(rsrt_libs) $(zlib_libs) $(pthreads_libs)
rt_init_LDFLAGS = -export-dynamic

rscript_parse_SOURCES = rscript-parse.c $(test_files)
rscript_parse_CPPFLAGS =  -I$(top_srcdir) $(pthreads_cflags) $(rsrt_cflags) 
rscript_parse_LDADD = $(rsrt_libs) $(zlib_libs) $(pthreads_libs)
rscript_parse_LDFLAGS = -export-dynamic
