pkglib_LTLIBRARIES = omotel.la

omotel_la_SOURCES = \
       omotel.c \
       otlp_json.c \
       omotel_http.c

noinst_HEADERS = \
       otlp_json.h \
       omotel_http.h

omotel_la_CPPFLAGS = \
       -I$(srcdir) \
       -I$(top_srcdir) \
       -I$(top_srcdir)/runtime \
       -I$(top_srcdir)/grammar \
       $(RSRT_CFLAGS) \
       $(OMOTEL_HTTP_CFLAGS)

omotel_la_LDFLAGS = -module -avoid-version

omotel_la_LIBADD = $(OMOTEL_HTTP_LIBS) $(ZLIB_LIBS)

EXTRA_DIST = \
        AGENTS.md \
        MODULE_METADATA.yaml
