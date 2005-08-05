# Makefile for rsyslog

CC= gcc
#CFLAGS= -g -DSYSV -Wall
# Add the -DMTRACE macro if you would like to use mtrace()
# to hunt for memory leaks
# next 2 lines are debug settings
#LDFLAGS= -g -Wall -fno-omit-frame-pointer
#CFLAGS= -DSYSV -g -Wall -fno-omit-frame-pointer

# uncomment the following line if you would like
# to DISABLE large file support (or if your compiler
# does not provide this feature)
#NOLARGEFILE = -DNOLARGEFILE

# the next two lines are essentially the same, but -DWITH_DB
# enables the MySQL code. By default, that one is commented out
# change the comment chars to activate it if you need MySQL!
# In this case, also look down further to uncomment the libs
CFLAGS= $(RPM_OPT_FLAGS) -O3 -DSYSV -fomit-frame-pointer -Wall -fno-strength-reduce $(NOLARGEFILE)
#CFLAGS= $(RPM_OPT_FLAGS) -O3 -DSYSV -fomit-frame-pointer -Wall -fno-strength-reduce -DWITH_DB -I/usr/local/include $(NOLARGEFILE)
LDFLAGS= -s

INSTALL = install
BINDIR = /usr/sbin
MANDIR = /usr/share/man

# Uncomment the following to use mysql.
#LIBS = -lmysqlclient -L/usr/local/lib/mysql 

# There is one report that under an all ELF system there may be a need to
# explicilty link with libresolv.a.  If linking syslogd fails you may wish
# to try uncommenting the following define.
# LIBS = /usr/lib/libresolv.a

# The following define determines whether the package adheres to the
# file system standard.
FSSTND = -DFSSTND

# The following define establishes the name of the pid file for the
# rsyslogd daemon.  The library include file (paths.h) defines the
# name for the rsyslogd pid to be rsyslog.pid.  A number of people have
# suggested that this should be rsyslogd.pid.  You may cast your
# ballot below.
SYSLOGD_PIDNAME = -DSYSLOGD_PIDNAME=\"rsyslogd.pid\"

SYSLOGD_FLAGS= -DSYSLOG_INET -DSYSLOG_UNIXAF ${FSSTND} \
	${SYSLOGD_PIDNAME}
SYSLOG_FLAGS= -DALLOW_KERNEL_LOGGING
DEB =

.c.o:
	${CC} ${CFLAGS} -c $*.c

all: syslogd

test: syslog_tst tsyslogd

install: install_man install_exec

syslogd: syslogd.o pidfile.o template.o stringbuf.o srUtils.o outchannel.o
	${CC} ${LDFLAGS} -o syslogd syslogd.o pidfile.o template.o outchannel.o stringbuf.o srUtils.o ${LIBS}

syslog_tst: syslog_tst.o
	${CC} ${LDFLAGS} -o syslog_tst syslog_tst.o

tsyslogd: syslogd.c syslogd.h version.h template.o outchannel.o stringbuf.o srUtils.o
	$(CC) $(CFLAGS) -g -DTESTING $(SYSLOGD_FLAGS) -o tsyslogd syslogd.c pidfile.o template.o outchannel.o stringbuf.o srUtils.o $(LIBS)

srUtils.o: srUtils.c srUtils.h liblogging-stub.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c srUtils.c

stringbuf.o: stringbuf.c stringbuf.h liblogging-stub.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c stringbuf.c

template.o: template.c template.h stringbuf.h liblogging-stub.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c template.c

outchannel.o: outchannel.c outchannel.h stringbuf.h liblogging-stub.h syslogd.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c outchannel.c

syslogd.o: syslogd.c version.h template.h outchannel.h syslogd.h
	${CC} ${CFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c syslogd.c

syslog.o: syslog.c
	${CC} ${CFLAGS} ${SYSLOG_FLAGS} -c syslog.c

syslog_tst.o: syslog_tst.c
	${CC} ${CFLAGS} -c syslog_tst.c

clean:
	rm -f *.o *.log *~ *.orig

clobber: clean
	rm -f syslogd klogd ksym syslog_tst oops_test TAGS tsyslogd tklogd

install_exec: syslogd
	${INSTALL} -b -s syslogd ${DESTDIR}${BINDIR}/rsyslogd

install_man:
	${INSTALL} rsyslogd.8 ${DESTDIR}${MANDIR}/man8/rsyslogd.8
	${INSTALL} rsyslog.conf.5 ${DESTDIR}${MANDIR}/man5/rsyslog.conf.5
