pkglib_LTLIBRARIES = omoracle.la

omoracle_la_SOURCES = omoracle.c omoracle.h
omoracle_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(ORACLE_CFLAGS)
omoracle_la_LDFLAGS = -module -avoid-version
omoracle_la_LIBADD = $(ORACLE_LIBS)

#EXTRA_DIST = 
