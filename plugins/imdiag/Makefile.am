pkglib_LTLIBRARIES = imdiag.la

imdiag_la_SOURCES = imdiag.c
imdiag_la_CPPFLAGS = -I$(top_srcdir) $(PTHREADS_CFLAGS) $(RSRT_CFLAGS)
if ENABLE_TESTBENCH
imdiag_la_CPPFLAGS += -DENABLE_TESTBENCH
endif
imdiag_la_LDFLAGS = -module -avoid-version
imdiag_la_LIBADD = 
