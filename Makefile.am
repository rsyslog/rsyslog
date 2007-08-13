
sbin_PROGRAMS=rklogd rfc3195d rsyslogd

rklogd_SOURCES=klogd.c syslog.c pidfile.c ksym.c ksym_mod.c klogd.h ksyms.h pidfile.h module.h

rfc3195d_SOURCES=rfc3195d.c rsyslog.h

man_MANS = rfc3195d.8 rklogd.8 rsyslogd.8 rsyslog.conf.5 

rsyslogd_SOURCES=syslogd.c pidfile.c template.c outchannel.c stringbuf.c srUtils.c parse.c syslogd-types.h template.h outchannel.h syslogd.h stringbuf.h parse.h srUtils.h liblogging-stub.h net.c net.h msg.c msg.h omshell.c omshell.h omusrmsg.c omusrmsg.h ommysql.c ommysql.h omfwd.c omfwd.h tcpsyslog.c tcpsyslog.h omfile.h omfile.c omdiscard.c omdiscard.h modules.c modules.h module-template.h objomsr.c objomsr.h cfsysline.c cfsysline.h linkedlist.c linkedlist.h iminternal.c iminternal.h action.c action.h
rsyslogd_CPPFLAGS=$(mysql_includes)
rsyslogd_LDADD=$(mysql_libs) $(zlib_libs) $(pthreads_libs)

EXTRA_DIST = doc/bugs.html doc/features.html doc/generic_design.html doc/history.html doc/how2help.html doc/install.html doc/ipv6.html doc/manual.html doc/property_replacer.html doc/rsyslog_conf.html doc/rsyslog_mysql.html doc/rsyslog_packages.html doc/rsyslog_php_syslog_ng.html doc/rsyslog_recording_pri.html doc/rsyslog_stunnel.html doc/status.html doc/syslog-protocol.html doc/version_naming.html doc/contributors.html redhat/rsyslog.conf redhat/rsyslog.init redhat/rsyslog.log redhat/rsyslog.sysconfig freebsd/rsyslogd slackware/rc.rsyslogd rfc3195d.8 rklogd.8 rsyslogd.8 rsyslog.conf.5 createDB.sql contrib/README contrib/delete_mysql doc/rsconf1_actionexeconlyifpreviousissuspended.html doc/rsconf1_actionresumeinterval.html doc/rsconf1_allowedsender.html doc/rsconf1_controlcharacterescapeprefix.html doc/rsconf1_debugprintcfsyslinehandlerlist.html doc/rsconf1_debugprintmodulelist.html doc/rsconf1_debugprinttemplatelist.html doc/rsconf1_dircreatemode.html doc/rsconf1_dirgroup.html doc/rsconf1_dirowner.html doc/rsconf1_dropmsgswithmaliciousdnsptrrecords.html doc/rsconf1_droptrailinglfonreception.html doc/rsconf1_dynafilecachesize.html doc/rsconf1_escapecontrolcharactersonreceive.html doc/rsconf1_failonchownfailure.html doc/rsconf1_filecreatemode.html doc/rsconf1_filegroup.html doc/rsconf1_fileowner.html doc/rsconf1_includeconfig.html doc/rsconf1_mainmsgqueuesize.html doc/rsconf1_modload.html doc/rsconf1_repeatedmsgreduction.html doc/rsconf1_resetconfigvariables.html doc/rsconf1_umask.html
