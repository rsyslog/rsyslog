pkglib_LTLIBRARIES = omotlp.la

omotlp_la_SOURCES = \
       omotlp.c \
       otlp_json.c \
       omotlp_http.c

noinst_HEADERS = \
       otlp_json.h \
       omotlp_http.h

omotlp_la_CPPFLAGS = \
       -I$(srcdir) \
       -I$(top_srcdir) \
       -I$(top_srcdir)/runtime \
       -I$(top_srcdir)/grammar \
       $(RSRT_CFLAGS) \
       $(OMOTLP_HTTP_CFLAGS)

omotlp_la_LDFLAGS = -module -avoid-version

omotlp_la_LIBADD = $(OMOTLP_HTTP_LIBS) $(ZLIB_LIBS)

EXTRA_DIST = \
        AGENTS.md \
        MODULE_METADATA.yaml
