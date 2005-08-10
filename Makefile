# Makefile for rsyslog
# Copyright (C) 2004, 2005 Rainer Gerhards and Adiscon GmbH
# This is the part of the makefile common to all distros.
# For details, see http://www.rsyslog.com/doc

#CC= gcc
#CFLAGS= -g -DSYSV -Wall
# Add the -DMTRACE macro if you would like to use mtrace()
# to hunt for memory leaks
# next 2 lines are debug settings
#LDFLAGS= -g -Wall -fno-omit-frame-pointer
#CFLAGS= -DSYSV -g -Wall -fno-omit-frame-pointer

CFLAGS= $(RPM_OPT_FLAGS) -O3 -DSYSV -fomit-frame-pointer -Wall -fno-strength-reduce -I/usr/local/include $(NOLARGEFILE) $(WITHDB)
LDFLAGS= -s

# There is one report that under an all ELF system there may be a need to
# explicilty link with libresolv.a.  If linking syslogd fails you may wish
# to try uncommenting the following define.
# LIBS = /usr/lib/libresolv.a

# The following define determines whether the package adheres to the
# file system standard.
FSSTND = -DFSSTND

# The following define establishes the name of the pid file for the
# rsyslogd daemon.  The library include file (paths.h) defines the
# name for the rsyslogd pid to be rsyslog.pid.
SYSLOGD_PIDNAME = -DSYSLOGD_PIDNAME=\"rsyslogd.pid\"

SYSLOGD_FLAGS= -DSYSLOG_INET -DSYSLOG_UNIXAF ${FSSTND} \
	${SYSLOGD_PIDNAME}
SYSLOG_FLAGS= -DALLOW_KERNEL_LOGGING

.c.o:
	${CC} ${CFLAGS} -c $(VPATH)$*.c

all: syslogd

test: syslog_tst tsyslogd

install: install_man install_exec

syslogd: syslogd.o pidfile.o template.o stringbuf.o srUtils.o outchannel.o
	${CC} ${LDFLAGS} -o syslogd syslogd.o pidfile.o template.o outchannel.o stringbuf.o srUtils.o ${LIBS}

srUtils.o: srUtils.c srUtils.h liblogging-stub.h
stringbuf.o: stringbuf.c stringbuf.h liblogging-stub.h
template.o: template.c template.h stringbuf.h liblogging-stub.h
outchannel.o: outchannel.c outchannel.h stringbuf.h liblogging-stub.h syslogd.h

syslogd.o: syslogd.c version.h template.h outchannel.h syslogd.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} -c $(VPATH)syslogd.c

syslog.o: syslog.c
	${CC} ${CFLAGS} ${SYSLOG_FLAGS} -c $(VPATH)syslog.c

clean:
	rm -f *.o *.log *~ *.orig syslogd

clobber: clean
	rm -f syslogd ksym syslog_tst oops_test TAGS tsyslogd tklogd

install_exec: syslogd
	${INSTALL} -b -s syslogd ${DESTDIR}${BINDIR}/rsyslogd

install_man:
	${INSTALL} $(VPATH)rsyslogd.8 ${DESTDIR}${MANDIR}/man8/rsyslogd.8
	${INSTALL} $(VPATH)rsyslog.conf.5 ${DESTDIR}${MANDIR}/man5/rsyslog.conf.5

# The following lines are some legacy from sysklogd, which we might need
# again in the future. So it is just commented out for now, eventually
# to be revived. rgerhards 2005-08-09

#syslog_tst: syslog_tst.o
#	${CC} ${LDFLAGS} -o syslog_tst syslog_tst.o

#tsyslogd: syslogd.c syslogd.h version.h template.o outchannel.o stringbuf.o srUtils.o
#	$(CC) $(CFLAGS) -g -DTESTING $(SYSLOGD_FLAGS) -o tsyslogd syslogd.c pidfile.o template.o outchannel.o stringbuf.o srUtils.o $(LIBS)
#syslog_tst.o: syslog_tst.c
#	${CC} ${CFLAGS} -c syslog_tst.c

