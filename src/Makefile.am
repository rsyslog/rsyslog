# Uncomment for debugging
DEBUG = -g
PTHREADS_CFLAGS = -pthread

#CFLAGS += $(DEBUG)

lib_LTLIBRARIES = librelp.la

librelp_la_SOURCES = \
	librelp.h \
	relp.c \
	relp.h \
	relpsess.c \
	relpsess.h \
	relpframe.c \
	relpframe.h \
	tcp.c \
	tcp.h \
	sendq.c \
	sendq.h \
	sendbuf.c \
	sendbuf.h \
	offers.c \
	offers.h \
	relpclt.c \
	relpclt.h \
	relpsrv.c \
	relpsrv.h \
	scsyslog.c \
	scrsp.c \
	copen.c \
	cclose.c \
	cserverclose.c \
	dbllinklist.h \
	cmdif.h
librelp_la_CPPFLAGS = $(PTHREADS_CFLAGS)
librelp_la_LIBADD = $(rt_libs)
librelp_la_LDFLAGS = -version-info 0:0:0

include_HEADERS = librelp.h
