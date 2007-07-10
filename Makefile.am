
bin_PROGRAMS=klogd rfc3195d syslogd

klogd_SOURCES=klogd.c syslog.c pidfile.c ksym.c 

rfc3195d_SOURCES=rfc3195d.c

syslogd_SOURCES=syslogd.c pidfile.c template.c outchannel.c stringbuf.c srUtils.c parse.c 
syslogd_CPPFLAGS=$(mysql_includes)
syslogd_LDADD=$(mysql_libs) $(zlib_libs) $(pthreads_libs)

