
bin_PROGRAMS=klogd rfc3195d syslogd

klogd_SOURCES=klogd.c syslog.c pidfile.c ksym.c ksym_mod.c klogd.h ksyms.h pidfile.h module.h

rfc3195d_SOURCES=rfc3195d.c rsyslog.h

man_MANS = rfc3195d.8 rklogd.8 rsyslogd.8 rsyslog.conf.5 

syslogd_SOURCES=syslogd.c pidfile.c template.c outchannel.c stringbuf.c srUtils.c parse.c syslogd-types.h template.h outchannel.h syslogd.h stringbuf.h parse.h srUtils.h liblogging-stub.h
syslogd_CPPFLAGS=$(mysql_includes)
syslogd_LDADD=$(mysql_libs) $(zlib_libs) $(pthreads_libs)

EXTRA_DIST = doc/bugs.html doc/features.html doc/generic_design.html doc/history.html doc/how2help.html doc/install.html doc/ipv6.html doc/manual.html doc/property_replacer.html doc/rsyslog_conf.html doc/rsyslog_mysql.html doc/rsyslog_packages.html doc/rsyslog_php_syslog_ng.html doc/rsyslog_recording_pri.html doc/rsyslog_stunnel.html doc/status.html doc/syslog-protocol.html doc/version_naming.html redhat/rsyslog.conf redhat/rsyslog.init redhat/rsyslog.log redhat/rsyslog.sysconfig debian/rsyslogd freebsd/rsyslogd slackware/rc.rsyslogd rfc3195d.8 rklogd.8 rsyslogd.8 rsyslog.conf.5 
