# Makefile for rsyslog

CC= gcc
#CFLAGS= -g -DSYSV -Wall
LDFLAGS= -g -Wall -fno-omit-frame-pointer
CFLAGS= -DSYSV -g -Wall -fno-omit-frame-pointer
#CFLAGS= $(RPM_OPT_FLAGS) -O3 -DSYSV -fomit-frame-pointer -Wall -fno-strength-reduce
#CFLAGS= $(RPM_OPT_FLAGS) -O3 -DSYSV -fomit-frame-pointer -Wall -fno-strength-reduce -DWITH_DB
#LDFLAGS= -s

# Look where your install program is.
INSTALL = /usr/bin/install
BINDIR = /usr/sbin
MANDIR = /usr/man

# Uncommenting the following to use mysql.
#LIBS = -lmysqlclient #/var/lib/mysql/mysql 

# There is one report that under an all ELF system there may be a need to
# explicilty link with libresolv.a.  If linking syslogd fails you may wish
# to try uncommenting the following define.
# LIBS = /usr/lib/libresolv.a

# The following define determines whether the package adheres to the
# file system standard.
FSSTND = -DFSSTND

# The following define establishes ownership for the man pages.
# Avery tells me that there is a difference between Debian and
# Slackware.  Rather than choose sides I am leaving it up to the user.
MAN_OWNER = root
# MAN_OWNER = man

# The following define establishes the name of the pid file for the
# syslogd daemon.  The library include file (paths.h) defines the
# name for the syslogd pid to be syslog.pid.  A number of people have
# suggested that this should be syslogd.pid.  You may cast your
# ballot below.
SYSLOGD_PIDNAME = -DSYSLOGD_PIDNAME=\"syslogd.pid\"

SYSLOGD_FLAGS= -DSYSLOG_INET -DSYSLOG_UNIXAF -DNO_SCCS ${FSSTND} \
	${SYSLOGD_PIDNAME}
SYSLOG_FLAGS= -DALLOW_KERNEL_LOGGING
DEB =

.c.o:
	${CC} ${CFLAGS} -c $*.c

all: syslogd

test: syslog_tst ksym oops_test tsyslogd

#install: install_man install_exec
install: install_exec

syslogd: syslogd.o pidfile.o template.o stringbuf.o srUtils.o
	${CC} ${LDFLAGS} -o syslogd syslogd.o pidfile.o template.o stringbuf.o srUtils.o ${LIBS}

syslog_tst: syslog_tst.o
	${CC} ${LDFLAGS} -o syslog_tst syslog_tst.o

tsyslogd: syslogd.c version.h template.o stringbuf.o srUtils.o
	$(CC) $(CFLAGS) -g -DTESTING $(SYSLOGD_FLAGS) -o tsyslogd syslogd.c pidfile.o template.o stringbuf.o srUtils.o $(LIBS)

srUtils.o: srUtils.c srUtils.h liblogging-stub.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c srUtils.c

stringbuf.o: stringbuf.c stringbuf.h liblogging-stub.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c stringbuf.c

template.o: template.c template.h stringbuf.h liblogging-stub.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c template.c

syslogd.o: syslogd.c version.h template.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c syslogd.c

syslog.o: syslog.c
	${CC} ${CFLAGS} ${SYSLOG_FLAGS} -c syslog.c

syslog_tst.o: syslog_tst.c
	${CC} ${CFLAGS} -c syslog_tst.c

clean:
	rm -f *.o *.log *~ *.orig

clobber: clean
	rm -f syslogd klogd ksym syslog_tst oops_test TAGS tsyslogd tklogd

install-replace: syslogd
	${INSTALL} -b -m 500 -s syslogd ${BINDIR}/syslogd

install_exec: syslogd
	cp ${BINDIR}/syslogd ${BINDIR}/syslogd-previous
	${INSTALL} -b -m 500 -s syslogd ${BINDIR}/syslogd

# man not yet supported ;)
#install_man:
#	${INSTALL} -o ${MAN_OWNER} -g ${MAN_OWNER} -m 644 sysklogd.8 ${MANDIR}/man8/sysklogd.8
#	${INSTALL} -o ${MAN_OWNER} -g ${MAN_OWNER} -m 644 syslogd.8 ${MANDIR}/man8/syslogd.8
#	${INSTALL} -o ${MAN_OWNER} -g ${MAN_OWNER} -m 644 syslog.conf.5 ${MANDIR}/man5/syslog.conf.5
#	${INSTALL} -o ${MAN_OWNER} -g ${MAN_OWNER} -m 644 klogd.8 ${MANDIR}/man8/klogd.8
