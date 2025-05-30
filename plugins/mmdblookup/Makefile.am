pkglib_LTLIBRARIES = mmdblookup.la

if MMDBLOOKUP_USE_DUMMY
mmdblookup_la_SOURCES = dummy.c
mmdblookup_la_LDFLAGS = -module -avoid-version
else
mmdblookup_la_SOURCES = mmdblookup.c
mmdblookup_la_LDFLAGS = -module -avoid-version -lmaxminddb
endif

mmdblookup_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
mmdblookup_la_LIBADD = 

EXTRA_DIST = 
