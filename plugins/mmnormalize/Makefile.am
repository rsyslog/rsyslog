pkglib_LTLIBRARIES = mmnormalize.la

if OS_AIX
LIBMATH=-lm
else
LIBMATH=
endif

mmnormalize_la_SOURCES = mmnormalize.c
mmnormalize_la_CPPFLAGS =  $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(LIBLOGNORM_CFLAGS)
mmnormalize_la_LDFLAGS = -module -avoid-version $(LIBLOGNORM_LIBS) $(LIBMATH)
mmnormalize_la_LIBADD = 

EXTRA_DIST = 
