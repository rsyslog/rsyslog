pkglib_LTLIBRARIES = omrabbitmq.la

omrabbitmq_la_SOURCES = omrabbitmq.c
omrabbitmq_la_CPPFLAGS = $(RABBITMQ_CFLAGS) $(RSRT_CFLAGS) $(PTHREADS_CFLAGS)
omrabbitmq_la_LDFLAGS = -module -avoid-version
omrabbitmq_la_LIBADD = $(RABBITMQ_LIBS)

EXTRA_DIST =
