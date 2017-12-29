pkglib_LTLIBRARIES = mmdblookup.la

mmdblookup_la_SOURCES = mmdblookup.c
mmdblookup_la_CPPFLAGS = $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(LIBMAXMINDDB_CFLAGS)
mmdblookup_la_LDFLAGS = -module -avoid-version -l:libmaxminddb.a
mmdblookup_la_LIBADD =
