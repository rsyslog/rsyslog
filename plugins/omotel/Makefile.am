pkglib_LTLIBRARIES = omotel.la

omotel_la_SOURCES = \
       omotel.c \
       otlp_json.c \
       omotel_http.c \
       otlp_protobuf.c

OTLP_PB_COMMON_C = opentelemetry/proto/common/v1/common.pb-c.c
OTLP_PB_COMMON_H = opentelemetry/proto/common/v1/common.pb-c.h
OTLP_PB_RESOURCE_C = opentelemetry/proto/resource/v1/resource.pb-c.c
OTLP_PB_RESOURCE_H = opentelemetry/proto/resource/v1/resource.pb-c.h
OTLP_PB_LOGS_C = opentelemetry/proto/logs/v1/logs.pb-c.c
OTLP_PB_LOGS_H = opentelemetry/proto/logs/v1/logs.pb-c.h

noinst_HEADERS = \
       otlp_json.h \
       omotel_http.h \
       otlp_protobuf.h

if ENABLE_OMOTEL
nodist_omotel_la_SOURCES = \
       $(OTLP_PB_COMMON_C) \
       $(OTLP_PB_RESOURCE_C) \
       $(OTLP_PB_LOGS_C)

nodist_noinst_HEADERS = \
       $(OTLP_PB_COMMON_H) \
       $(OTLP_PB_RESOURCE_H) \
       $(OTLP_PB_LOGS_H)

omotel_la_CPPFLAGS = \
       -I$(srcdir) \
       -I$(builddir) \
       -I$(top_srcdir) \
       -I$(top_srcdir)/runtime \
       -I$(top_srcdir)/grammar \
       $(RSRT_CFLAGS) \
       $(OMOTEL_HTTP_CFLAGS) \
       $(OMOTEL_PROTOBUF_CFLAGS)

omotel_la_LDFLAGS = -module -avoid-version

omotel_la_LIBADD = $(OMOTEL_HTTP_LIBS) $(ZLIB_LIBS) $(OMOTEL_PROTOBUF_LIBS)

BUILT_SOURCES = $(OTLP_PB_COMMON_C) $(OTLP_PB_COMMON_H) \
                $(OTLP_PB_RESOURCE_C) $(OTLP_PB_RESOURCE_H) \
                $(OTLP_PB_LOGS_C) $(OTLP_PB_LOGS_H)

$(OTLP_PB_COMMON_C) $(OTLP_PB_COMMON_H): $(srcdir)/proto/opentelemetry/proto/common/v1/common.proto
	$(PROTOC_C) --proto_path=$(srcdir)/proto --c_out=$(builddir) $<

$(OTLP_PB_RESOURCE_C) $(OTLP_PB_RESOURCE_H): $(srcdir)/proto/opentelemetry/proto/resource/v1/resource.proto $(OTLP_PB_COMMON_H)
	$(PROTOC_C) --proto_path=$(srcdir)/proto --c_out=$(builddir) $<

$(OTLP_PB_LOGS_C) $(OTLP_PB_LOGS_H): $(srcdir)/proto/opentelemetry/proto/logs/v1/logs.proto $(OTLP_PB_COMMON_H) $(OTLP_PB_RESOURCE_H)
	$(PROTOC_C) --proto_path=$(srcdir)/proto --c_out=$(builddir) $<
endif

CLEANFILES = $(OTLP_PB_COMMON_C) $(OTLP_PB_COMMON_H) \
             $(OTLP_PB_RESOURCE_C) $(OTLP_PB_RESOURCE_H) \
             $(OTLP_PB_LOGS_C) $(OTLP_PB_LOGS_H)

EXTRA_DIST = \
        AGENTS.md \
        MODULE_METADATA.yaml \
        proto/README \
        proto/opentelemetry/proto/common/v1/common.proto \
        proto/opentelemetry/proto/resource/v1/resource.proto \
        proto/opentelemetry/proto/logs/v1/logs.proto
