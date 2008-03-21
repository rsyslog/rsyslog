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
	relpclt.c \
	relpclt.h \
	relpsrv.c \
	relpsrv.h \
	scsyslog.c \
	scrsp.c \
	copen.c \
	cclose.c \
	dbllinklist.h \
	cmdif.h
librelp_la_CPPFLAGS = $(PTHREADS_CFLAGS)
librelp_la_LIBADD = 

include_HEADERS = librelp.h
