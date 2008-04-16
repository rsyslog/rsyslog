sbin_PROGRAMS =
man_MANS = 
noinst_LTLIBRARIES = librsyslog.la
#pkglib_LTLIBRARIES = librsyslog.la

librsyslog_la_SOURCES = \
	sync.c \
	sync.h \
	expr.c \
	expr.h \
	ctok.c \
	ctok.h \
	ctok_token.c \
	ctok_token.h \
	stream.c \
	stream.h \
	var.c \
	var.h \
	wtp.c \
	wtp.h \
	wti.c \
	wti.h \
	sysvar.c \
	sysvar.h \
	vm.c \
	vm.h \
	vmstk.c \
	vmstk.h \
	vmprg.c \
	vmprg.h \
	vmop.c \
	vmop.h \
	queue.c \
	queue.h

librsyslog_la_CPPFLAGS = -I$(top_srcdir) $(pthreads_cflags)
#librsyslog_la_LDFLAGS = -module -avoid-version
librsyslog_la_LIBADD = 

#
# regular expression support
# 
if ENABLE_REGEXP
noinst_LTLIBRARIES += lmregexp.la
#pkglib_LTLIBRARIES += lmregexp.la
lmregexp_la_SOURCES = regexp.c regexp.h
lmregexp_la_CPPFLAGS = $(pthreads_cflags) $(rsrt_cflags)
lmregexp_la_LDFLAGS = -module -avoid-version $(rsrt_libs)
lmregexp_la_LIBADD =
endif

