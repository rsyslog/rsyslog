pkglib_LTLIBRARIES = impstats.la

impstats_la_SOURCES = impstats.c

if ENABLE_IMPSTATS_PUSH
impstats_la_SOURCES += \
	impstats_push.c \
	prometheus_remote_write.c

nodist_impstats_la_SOURCES = remote.pb-c.c

noinst_HEADERS = \
	impstats_push.h \
	prometheus_remote_write.h

nodist_noinst_HEADERS = remote.pb-c.h
endif

impstats_la_CPPFLAGS = $(RSRT_CFLAGS) -I$(top_srcdir) $(PTHREADS_CFLAGS)

if ENABLE_IMPSTATS_PUSH
impstats_la_CPPFLAGS += $(IMPSTATS_PUSH_CFLAGS) -DENABLE_IMPSTATS_PUSH
endif

impstats_la_LDFLAGS = -module -avoid-version

impstats_la_LIBADD =

if ENABLE_IMPSTATS_PUSH
impstats_la_LIBADD += $(IMPSTATS_PUSH_LIBS)
endif

if ENABLE_IMPSTATS_PUSH
BUILT_SOURCES = remote.pb-c.c remote.pb-c.h

remote.pb-c.c remote.pb-c.h: remote.proto
	$(PROTOC_C) --proto_path=$(srcdir) --c_out=. $<

CLEANFILES = remote.pb-c.c remote.pb-c.h
endif

EXTRA_DIST = \
	remote.proto 
