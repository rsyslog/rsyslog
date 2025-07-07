pkglib_LTLIBRARIES = mmaitag.la

mmaitag_la_SOURCES = mmaitag.c ai_provider.h ai_provider_gemini.c ai_provider_gemini_mock.c
mmaitag_la_CPPFLAGS = $(RSRT_CFLAGS) $(PTHREADS_CFLAGS) $(CURL_CFLAGS)
mmaitag_la_LDFLAGS = -module -avoid-version
mmaitag_la_LIBADD = $(CURL_LIBS)

EXTRA_DIST =
