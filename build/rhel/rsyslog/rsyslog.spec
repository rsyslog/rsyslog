%global _exec_prefix %{nil}
%global _libdir %{_exec_prefix}/%{_lib}
%define rsyslog_statedir %{_sharedstatedir}/rsyslog
%define rsyslog_pkidir %{_sysconfdir}/pki/rsyslog

Summary: Enhanced system logging and kernel message trapping daemon
Name: rsyslog
Version: 6.5.0
Release: 1%{?dist}
License: (GPLv3+ and ASL 2.0)
Group: System Environment/Daemons
URL: http://www.rsyslog.com/
Source0: %{name}-%{version}.tar.gz
Source1: rsyslog.init
Source2: rsyslog.conf
Source3: rsyslog.sysconfig
Source4: rsyslog.log

BuildRequires: zlib-devel
BuildRequires: pkgconfig
BuildRequires: flex
BuildRequires: bison

%if %{?rhel}%{!?rhel:0} >= 6
BuildRequires: libcurl-devel
Requires:      libcurl
%elseif %{?rhel}%{!?rhel:0} >= 5
BuildRequires: curl-devel
Requires:      curl
%endif

Requires: logrotate >= 3.5.2
Requires: bash >= 2.0
Requires(post): /sbin/chkconfig coreutils
Requires(post): /sbin/service
Requires(preun): /sbin/chkconfig
Requires(preun): /sbin/service
Requires(postun): /sbin/service

Provides: syslog
Obsoletes: sysklogd < 1.5-11

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%package elasticsearch
Summary: ElasticSearch output module for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release

%package mmjsonparse
Summary: JSON enhanced logging support
Group: System Environment/Daemons
Requires: %name = %version-%release

%package mmnormalize
Summary: Log normalization support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: libestr-devel libee-devel liblognorm-devel

%package imzmq3
Summary: ZMQ input module support
Group: Systems Environment/Daemons
Requires: %name = %version-%release
BuildRequires: czmq-devel
BuildRequires: zeromq-devel
%if %{?rhel}%{!?rhel:0} >= 6
BuildRequires: libuuid-devel
Requires:      libuuid
%elseif %{?rhel}%{!?rhel:0} >= 5
BuildRequires: e2fsprogs-devel
Requires:      e2fsprogs
%else
BuildRequires: uuid-devel
Requires:      uuid
%endif


%package omzmq3
Summary: ZMQ output module support
Group: Systems Environment/Daemons
Requires: %name = %version-%release
BuildRequires: czmq-devel
BuildRequires: zeromq-devel
%if %{?rhel}%{!?rhel:0} >= 6
BuildRequires: libuuid-devel
Requires:      libuuid
%elseif %{?rhel}%{!?rhel:0} >= 5
BuildRequires: e2fsprogs-devel
Requires:      e2fsprogs
%else
BuildRequires: uuid-devel
Requires:      uuid
%endif


%package libdbi
Summary: libdbi database support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: libdbi-devel

%package mysql
Summary: MySQL support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: mysql-devel >= 4.0

%package pgsql
Summary: PostgresSQL support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: postgresql-devel

%package gssapi
Summary: GSSAPI authentication and encryption support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: krb5-devel

%package relp
Summary: RELP protocol support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: librelp-devel

%package gnutls
Summary: TLS protocol support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: gnutls-devel

%package snmp
Summary: SNMP protocol support for rsyslog
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: net-snmp-devel

%package udpspoof
Summary: Provides the omudpspoof module
Group: System Environment/Daemons
Requires: %name = %version-%release
BuildRequires: libnet-devel

%description
Rsyslog is an enhanced, multi-threaded syslog daemon. It supports MySQL,
syslog/TCP, RFC 3195, permitted sender lists, filtering on any message part,
and fine grain output format control. It is compatible with stock sysklogd
and can be used as a drop-in replacement. Rsyslog is simple to set up, with
advanced features suitable for enterprise-class, encryption-protected syslog
relay chains.

%description elasticsearch
This module provides the capability for rsyslog to feed logs directly into
Elasticsearch.

%description mmjsonparse
This module provides the capability to recognize and parse JSON enhanced
syslog messages.

%description mmnormalize
This module provides the capability to normalize log messages via liblognorm.

%description imzmq3
This module provides the capability to use a zeromq socket for input.

%description omzmq3
This module provides the capability to use a zeromq socket for output.

%description libdbi
This module supports a large number of database systems via
libdbi. Libdbi abstracts the database layer and provides drivers for
many systems. Drivers are available via the libdbi-drivers project.

%description mysql
The rsyslog-mysql package contains a dynamic shared object that will add
MySQL database support to rsyslog.

%description pgsql
The rsyslog-pgsql package contains a dynamic shared object that will add
PostgreSQL database support to rsyslog.

%description gssapi
The rsyslog-gssapi package contains the rsyslog plugins which support GSSAPI
authentication and secure connections. GSSAPI is commonly used for Kerberos
authentication.

%description relp
The rsyslog-relp package contains the rsyslog plugins that provide
the ability to receive syslog messages via the reliable RELP
protocol.

%description gnutls
The rsyslog-gnutls package contains the rsyslog plugins that provide the
ability to receive syslog messages via upcoming syslog-transport-tls
IETF standard protocol.

%description snmp
The rsyslog-snmp package contains the rsyslog plugin that provides the
ability to send syslog messages as SNMPv1 and SNMPv2c traps.

%description udpspoof
This module is similar to the regular UDP forwarder, but permits to
spoof the sender address. Also, it enables to circle through a number
of source ports.

%prep
%setup -q
#%patch0 -p1
#%patch1 -p1
#%patch2 -p1
#%patch3 -p1
#%patch4 -p1
#%patch5 -p1

%build
%ifarch sparc64
#sparc64 need big PIE
export CFLAGS="$RPM_OPT_FLAGS -fPIE -DSYSLOGD_PIDNAME=\\\"syslogd.pid\\\""
export LDFLAGS="-pie -Wl,-z,relro -Wl,-z,now"
%else
export CFLAGS="$RPM_OPT_FLAGS -fpie -DSYSLOGD_PIDNAME=\\\"syslogd.pid\\\""
export LDFLAGS="-pie -Wl,-z,relro -Wl,-z,now"
%endif
export PKG_CONFIG=/usr/bin/pkg-config
%configure --disable-static \
        --disable-testbench \
        --enable-elasticsearch \
        --enable-mmjsonparse \
        --enable-mmnormalize \
        --enable-imzmq3 \
        --enable-omzmq3 \
        --enable-gnutls \
        --enable-gssapi-krb5 \
        --enable-imfile \
        --enable-impstats \
        --enable-imptcp \
        --enable-libdbi \
        --enable-mail \
        --enable-mysql \
        --enable-omprog \
        --enable-omudpspoof \
        --enable-omuxsock \
        --enable-pgsql \
        --enable-pmlastmsg \
        --enable-relp \
        --enable-snmp \
        --enable-unlimited-select
make

%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT

install -d -m 755 $RPM_BUILD_ROOT%{_initrddir}
install -d -m 755 $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig
install -d -m 755 $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d
install -d -m 755 $RPM_BUILD_ROOT%{_sysconfdir}/rsyslog.d
install -d -m 700 $RPM_BUILD_ROOT%{rsyslog_statedir}
install -d -m 700 $RPM_BUILD_ROOT%{rsyslog_pkidir}

install -p -m 755 %{SOURCE1} $RPM_BUILD_ROOT%{_initrddir}/rsyslog
install -p -m 644 %{SOURCE2} $RPM_BUILD_ROOT%{_sysconfdir}/rsyslog.conf
install -p -m 644 %{SOURCE3} $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/rsyslog
install -p -m 644 %{SOURCE4} $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d/syslog

#get rid of *.la
rm $RPM_BUILD_ROOT/%{_libdir}/rsyslog/*.la

%clean
rm -rf $RPM_BUILD_ROOT

%post
for n in /var/log/{messages,secure,maillog,spooler}
do
    [ -f $n ] && continue
    umask 066 && touch $n
done
if [ $1 -eq 1 ]; then
        # On install (not upgrade), enable (but don't start)
        /sbin/chkconfig --add rsyslog
        /sbin/chkconfig rsyslog on
fi

%preun
if [ $1 = 0 ]; then
        # On uninstall (not upgrade), disable and stop the units
        /sbin/chkconfig --del rsyslog >/dev/null 2>&1 || :
        /sbin/service rsyslog stop >/dev/null 2>&1 || :
fi

%postun
if [ $1 -ge 1 ] ; then
        # On upgrade (not uninstall), optionally, restart the daemon
        /sbin/service rsyslog restart >/dev/null 2>&1 || :
fi

%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING* NEWS README ChangeLog doc/*html
%dir %{_libdir}/rsyslog
%{_libdir}/rsyslog/imfile.so
%{_libdir}/rsyslog/imklog.so
%{_libdir}/rsyslog/immark.so
%{_libdir}/rsyslog/impstats.so
%{_libdir}/rsyslog/imptcp.so
%{_libdir}/rsyslog/imtcp.so
%{_libdir}/rsyslog/imudp.so
%{_libdir}/rsyslog/imuxsock.so
%{_libdir}/rsyslog/lmnet.so
%{_libdir}/rsyslog/lmnetstrms.so
%{_libdir}/rsyslog/lmnsd_ptcp.so
%{_libdir}/rsyslog/lmregexp.so
%{_libdir}/rsyslog/lmstrmsrv.so
%{_libdir}/rsyslog/lmtcpclt.so
%{_libdir}/rsyslog/lmtcpsrv.so
%{_libdir}/rsyslog/lmzlibw.so
%{_libdir}/rsyslog/omtesting.so
%{_libdir}/rsyslog/ommail.so
%{_libdir}/rsyslog/omprog.so
%{_libdir}/rsyslog/omruleset.so
%{_libdir}/rsyslog/omuxsock.so
%{_libdir}/rsyslog/pmlastmsg.so
%{_initrddir}/rsyslog

%config(noreplace) %{_sysconfdir}/rsyslog.conf
%config(noreplace) %{_sysconfdir}/sysconfig/rsyslog
%config(noreplace) %{_sysconfdir}/logrotate.d/syslog
%dir %{_sysconfdir}/rsyslog.d
%dir %{rsyslog_statedir}
%dir %{rsyslog_pkidir}
%{_sbindir}/rsyslogd
%{_mandir}/*/*

%attr(0755,root,root) %{_initrddir}/rsyslog

%files elasticsearch
%defattr(-,root,root)
%{_libdir}/rsyslog/omelasticsearch.so

%files mmjsonparse
%defattr(-,root,root)
%{_libdir}/rsyslog/mmjsonparse.so

%files mmnormalize
%defattr(-,root,root)
%{_libdir}/rsyslog/mmnormalize.so

%files imzmq3
%defattr(-,root,root)
%{_libdir}/rsyslog/imzmq3.so

%files omzmq3
%defattr(-,root,root)
%{_libdir}/rsyslog/omzmq3.so

%files libdbi
%defattr(-,root,root)
%{_libdir}/rsyslog/omlibdbi.so

%files mysql
%defattr(-,root,root)
%doc plugins/ommysql/createDB.sql
%{_libdir}/rsyslog/ommysql.so

%files pgsql
%defattr(-,root,root)
%doc plugins/ompgsql/createDB.sql
%{_libdir}/rsyslog/ompgsql.so

%files gssapi
%defattr(-,root,root)
%{_libdir}/rsyslog/lmgssutil.so
%{_libdir}/rsyslog/imgssapi.so
%{_libdir}/rsyslog/omgssapi.so

%files relp
%defattr(-,root,root)
%{_libdir}/rsyslog/imrelp.so
%{_libdir}/rsyslog/omrelp.so

%files gnutls
%defattr(-,root,root)
%{_libdir}/rsyslog/lmnsd_gtls.so

%files snmp
%defattr(-,root,root)
%{_libdir}/rsyslog/omsnmp.so

%files udpspoof
%defattr(-,root,root)
%{_libdir}/rsyslog/omudpspoof.so

%changelog

* Wed Jun 06 2012 Abigail Edwards <abby.lina.edwards@gmail.com> 6.5.0-1
- convert Fedora spec into RHEL 6 spec, as far as init scripts are concerned
- removed references to all Fedora specific patches, as they were mostly
  version specific
- added syntax check option to the init script, made a successful check
  a prequisite to starting the daemon
- added several "--enable-*" directives to configure to create as many
  subpackages as possible; enduser will need to have requisite dependencies
  installed, some of which I had to write separate specs for.

* Wed May 23 2012 Tomas Heinrich <theinric@redhat.com> 5.8.11-1
- upgrade to new upstream stable version 5.8.11
- add impstats and imptcp modules
- include new license text files
- consider lock file in 'status' action
- add patch to update information on debugging in the man page
- add patch to prevent debug output to stdout after forking
- add patch to support ssl certificates with domain names longer than 128 chars

* Fri Mar 30 2012 Jon Ciesla <limburgher@gmail.com> 5.8.7-2
- libnet rebuild.

* Mon Jan 23 2012 Tomas Heinrich <theinric@redhat.com> 5.8.7-1
- upgrade to new upstream version 5.8.7
- change license from 'GPLv3+' to '(GPLv3+ and ASL 2.0)'
  http://blog.gerhards.net/2012/01/rsyslog-licensing-update.html
- use a specific version for obsoleting sysklogd
- add patches for better sysklogd compatibility (taken from upstream)

* Sat Jan 14 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 5.8.6-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Tue Oct 25 2011 Tomas Heinrich <theinric@redhat.com> 5.8.6-1
- upgrade to new upstream version 5.8.6
- obsolete sysklogd
  Resolves: #748495

* Tue Oct 11 2011 Tomas Heinrich <theinric@redhat.com> 5.8.5-3
- modify logrotate configuration to omit boot.log
  Resolves: #745093

* Mon Sep 06 2011 Tomas Heinrich <theinric@redhat.com> 5.8.5-2
- add systemd-units to BuildRequires for the _unitdir macro definition

* Mon Sep 05 2011 Tomas Heinrich <theinric@redhat.com> 5.8.5-1
- upgrade to new upstream version (CVE-2011-3200)

* Fri Jul 22 2011 Tomas Heinrich <theinric@redhat.com> 5.8.2-3
- move the SysV init script into a subpackage
- Resolves: 697533

* Mon Jul 11 2011 Tomas Heinrich <theinric@redhat.com> 5.8.2-2
- rebuild for net-snmp-5.7 (soname bump in libnetsnmp)

* Mon Jun 27 2011 Tomas Heinrich <theinric@redhat.com> 5.8.2-1
- upgrade to new upstream version 5.8.2

* Mon Jun 13 2011 Tomas Heinrich <theinric@redhat.com> 5.8.1-2
- scriptlet correction
- use macro in unit file's path

* Fri May 20 2011 Tomas Heinrich <theinric@redhat.com> 5.8.1-1
- upgrade to new upstream version
- correct systemd scriptlets (#705829)

* Mon May 16 2011 Bill Nottingham <notting@redhat.com> - 5.7.9-3
- combine triggers (as rpm will only execute one) - fixes upgrades (#699198)

* Tue Apr 05 2011 Tomas Heinrich <theinric@redhat.com> 5.7.10-1
- upgrade to new upstream version 5.7.10

* Wed Mar 23 2011 Dan Horák <dan@danny.cz> - 5.7.9-2
- rebuilt for mysql 5.5.10 (soname bump in libmysqlclient)

* Fri Mar 18 2011 Tomas Heinrich <theinric@redhat.com> 5.7.9-1
- upgrade to new upstream version 5.7.9
- enable compilation of several new modules,
  create new subpackages for some of them
- integrate changes from Lennart Poettering
  to add support for systemd
  - add rsyslog-5.7.9-systemd.patch to tweak the upstream
    service file to honour configuration from /etc/sysconfig/rsyslog

* Fri Mar 18 2011 Dennis Gilmore <dennis@ausil.us> - 5.6.2-3
- sparc64 needs big PIE

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 5.6.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Mon Dec 20 2010 Tomas Heinrich <theinric@redhat.com> 5.6.2-1
- upgrade to new upstream stable version 5.6.2
- drop rsyslog-5.5.7-remove_include.patch; applied upstream
- provide omsnmp module
- use correct name for lock file (#659398)
- enable specification of the pid file (#579411)
- init script adjustments

* Wed Oct 06 2010 Tomas Heinrich <theinric@redhat.com> 5.5.7-1
- upgrade to upstream version 5.5.7
- update configuration and init files for the new major version
- add several directories for storing auxiliary data
- add ChangeLog to documentation
- drop unlimited-select.patch; integrated upstream
- add rsyslog-5.5.7-remove_include.patch to fix compilation

* Tue Sep 07 2010 Tomas Heinrich <theinric@redhat.com> 4.6.3-2
- build rsyslog with PIE and RELRO

* Thu Jul 15 2010 Tomas Heinrich <theinric@redhat.com> 4.6.3-1
- upgrade to new upstream stable version 4.6.3

* Wed Apr 07 2010 Tomas Heinrich <theinric@redhat.com> 4.6.2-1
- upgrade to new upstream stable version 4.6.2
- correct the default value of the OMFileFlushOnTXEnd directive

* Thu Feb 11 2010 Tomas Heinrich <theinric@redhat.com> 4.4.2-6
- modify rsyslog-4.4.2-unlimited-select.patch so that
  running autoreconf is not needed
- remove autoconf, automake, libtool from BuildRequires
- change exec-prefix to nil

* Wed Feb 10 2010 Tomas Heinrich <theinric@redhat.com> 4.4.2-5
- remove '_smp_mflags' make argument as it seems to be
  producing corrupted builds

* Mon Feb 08 2010 Tomas Heinrich <theinric@redhat.com> 4.4.2-4
- redefine _libdir as it doesn't use _exec_prefix

* Thu Dec 17 2009 Tomas Heinrich <theinric@redhat.com> 4.4.2-3
- change exec-prefix to /

* Wed Dec 09 2009 Robert Scheck <robert@fedoraproject.org> 4.4.2-2
- run libtoolize to avoid errors due mismatching libtool version

* Thu Dec 03 2009 Tomas Heinrich <theinric@redhat.com> 4.4.2-1
- upgrade to new upstream stable version 4.4.2
- add support for arbitrary number of open file descriptors

* Mon Sep 14 2009 Tomas Heinrich <theinric@redhat.com> 4.4.1-2
- adjust init script according to guidelines (#522071)

* Thu Sep 03 2009 Tomas Heinrich <theinric@redhat.com> 4.4.1-1
- upgrade to new upstream stable version

* Fri Aug 21 2009 Tomas Mraz <tmraz@redhat.com> - 4.2.0-3
- rebuilt with new openssl

* Sun Jul 26 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 4.2.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Tue Jul 14 2009 Tomas Heinrich <theinric@redhat.com> 4.2.0-1
- upgrade

* Mon Apr 13 2009 Tomas Heinrich <theinric@redhat.com> 3.21.11-1
- upgrade

* Tue Mar 31 2009 Lubomir Rintel <lkundrak@v3.sk> 3.21.10-4
- Backport HUPisRestart option

* Wed Mar 18 2009 Tomas Heinrich <theinric@redhat.com> 3.21.10-3
- fix variables' type conversion in expression-based filters (#485937)

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.21.10-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Tue Feb 10 2009 Tomas Heinrich <theinric@redhat.com> 3.21.10-1
- upgrade

* Sat Jan 24 2009 Caolán McNamara <caolanm@redhat.com> 3.21.9-3
- rebuild for dependencies

* Tue Jan 07 2009 Tomas Heinrich <theinric@redhat.com> 3.21.9-2
- fix several legacy options handling
- fix internal message output (#478612)

* Mon Dec 15 2008 Peter Vrabec <pvrabec@redhat.com> 3.21.9-1
- update is fixing $AllowedSender security issue

* Mon Sep 15 2008 Peter Vrabec <pvrabec@redhat.com> 3.21.3-4
- use RPM_OPT_FLAGS
- use same pid file and logrotate file as syslog-ng (#441664)
- mark config files as noreplace (#428155)

* Mon Sep 01 2008 Tomas Heinrich <theinric@redhat.com> 3.21.3-3
- fix a wrong module name in the rsyslog.conf manual page (#455086)
- expand the rsyslog.conf manual page (#456030)

* Thu Aug 28 2008 Tomas Heinrich <theinric@redhat.com> 3.21.3-2
- fix clock rollback issue (#460230)

* Wed Aug 20 2008 Peter Vrabec <pvrabec@redhat.com> 3.21.3-1
- upgrade to bugfix release

* Wed Jul 23 2008 Peter Vrabec <pvrabec@redhat.com> 3.21.0-1
- upgrade

* Mon Jul 14 2008 Peter Vrabec <pvrabec@redhat.com> 3.19.9-2
- adjust default config file

* Fri Jul 11 2008 Lubomir Rintel <lkundrak@v3.sk> 3.19.9-1
- upgrade

* Wed Jun 25 2008 Peter Vrabec <pvrabec@redhat.com> 3.19.7-3
- rebuild because of new gnutls

* Fri Jun 13 2008 Peter Vrabec <pvrabec@redhat.com> 3.19.7-2
- do not translate Oopses (#450329)

* Fri Jun 13 2008 Peter Vrabec <pvrabec@redhat.com> 3.19.7-1
- upgrade

* Wed May 28 2008 Peter Vrabec <pvrabec@redhat.com> 3.19.4-1
- upgrade

* Mon May 26 2008 Peter Vrabec <pvrabec@redhat.com> 3.19.3-1
- upgrade to new upstream release

* Wed May 14 2008 Tomas Heinrich <theinric@redhat.com> 3.16.1-1
- upgrade

* Tue Apr 08 2008 Peter Vrabec <pvrabec@redhat.com> 3.14.1-5
- prevent undesired error description in legacy 
  warning messages

* Tue Apr 08 2008 Peter Vrabec <pvrabec@redhat.com> 3.14.1-4
- adjust symbol lookup method to 2.6 kernel 

* Tue Apr 08 2008 Peter Vrabec <pvrabec@redhat.com> 3.14.1-3
- fix segfault of expression based filters

* Mon Apr 07 2008 Peter Vrabec <pvrabec@redhat.com> 3.14.1-2
- init script fixes (#441170,#440968)

* Fri Apr 04 2008 Peter Vrabec <pvrabec@redhat.com> 3.14.1-1
- upgrade

* Mon Mar 25 2008 Peter Vrabec <pvrabec@redhat.com> 3.12.4-1
- upgrade

* Wed Mar 19 2008 Peter Vrabec <pvrabec@redhat.com> 3.12.3-1
- upgrade 
- fix some significant memory leaks

* Tue Mar 11 2008 Peter Vrabec <pvrabec@redhat.com> 3.12.1-2
- init script fixes (#436854)
- fix config file parsing (#436722)

* Thu Mar 06 2008 Peter Vrabec <pvrabec@redhat.com> 3.12.1-1
- upgrade

* Wed Mar 05 2008 Peter Vrabec <pvrabec@redhat.com> 3.12.0-1
- upgrade

* Mon Feb 25 2008 Peter Vrabec <pvrabec@redhat.com> 3.11.5-1
- upgrade

* Fri Feb 01 2008 Peter Vrabec <pvrabec@redhat.com> 3.11.0-1
- upgrade to the latests development release
- provide PostgresSQL support
- provide GSSAPI support

* Mon Jan 21 2008 Peter Vrabec <pvrabec@redhat.com> 2.0.0-7
- change from requires sysklogd to conflicts sysklogd

* Fri Jan 18 2008 Peter Vrabec <pvrabec@redhat.com> 2.0.0-6
- change logrotate file
- use rsyslog own pid file

* Thu Jan 17 2008 Peter Vrabec <pvrabec@redhat.com> 2.0.0-5
- fixing bad descriptor (#428775)

* Wed Jan 16 2008 Peter Vrabec <pvrabec@redhat.com> 2.0.0-4
- rename logrotate file

* Wed Jan 16 2008 Peter Vrabec <pvrabec@redhat.com> 2.0.0-3
- fix post script and init file

* Wed Jan 16 2008 Peter Vrabec <pvrabec@redhat.com> 2.0.0-2
- change pid filename and use logrotata script from sysklogd

* Tue Jan 15 2008 Peter Vrabec <pvrabec@redhat.com> 2.0.0-1
- upgrade to stable release
- spec file clean up

* Wed Jan 02 2008 Peter Vrabec <pvrabec@redhat.com> 1.21.2-1
- new upstream release

* Thu Dec 06 2007 Release Engineering <rel-eng at fedoraproject dot org> - 1.19.11-2
- Rebuild for deps

* Thu Nov 29 2007 Peter Vrabec <pvrabec@redhat.com> 1.19.11-1
- new upstream release
- add conflicts (#400671)

* Mon Nov 19 2007 Peter Vrabec <pvrabec@redhat.com> 1.19.10-1
- new upstream release

* Wed Oct 03 2007 Peter Vrabec <pvrabec@redhat.com> 1.19.6-3
- remove NUL character from recieved messages

* Tue Sep 25 2007 Tomas Heinrich <theinric@redhat.com> 1.19.6-2
- fix message suppression (303341)

* Tue Sep 25 2007 Tomas Heinrich <theinric@redhat.com> 1.19.6-1
- upstream bugfix release

* Tue Aug 28 2007 Peter Vrabec <pvrabec@redhat.com> 1.19.2-1
- upstream bugfix release
- support for negative app selector, patch from 
  theinric@redhat.com

* Fri Aug 17 2007 Peter Vrabec <pvrabec@redhat.com> 1.19.0-1
- new upstream release with MySQL support(as plugin)

* Wed Aug 08 2007 Peter Vrabec <pvrabec@redhat.com> 1.18.1-1
- upstream bugfix release

* Mon Aug 06 2007 Peter Vrabec <pvrabec@redhat.com> 1.18.0-1
- new upstream release

* Thu Aug 02 2007 Peter Vrabec <pvrabec@redhat.com> 1.17.6-1
- upstream bugfix release

* Mon Jul 30 2007 Peter Vrabec <pvrabec@redhat.com> 1.17.5-1
- upstream bugfix release
- fix typo in provides 

* Wed Jul 25 2007 Jeremy Katz <katzj@redhat.com> - 1.17.2-4
- rebuild for toolchain bug

* Tue Jul 24 2007 Peter Vrabec <pvrabec@redhat.com> 1.17.2-3
- take care of sysklogd configuration files in %%post

* Tue Jul 24 2007 Peter Vrabec <pvrabec@redhat.com> 1.17.2-2
- use EVR in provides/obsoletes sysklogd

* Mon Jul 23 2007 Peter Vrabec <pvrabec@redhat.com> 1.17.2-1
- upstream bug fix release

* Fri Jul 20 2007 Peter Vrabec <pvrabec@redhat.com> 1.17.1-1
- upstream bug fix release
- include html docs (#248712)
- make "-r" option compatible with sysklogd config (248982)

* Tue Jul 17 2007 Peter Vrabec <pvrabec@redhat.com> 1.17.0-1
- feature rich upstream release

* Thu Jul 12 2007 Peter Vrabec <pvrabec@redhat.com> 1.15.1-2
- use obsoletes and hadle old config files

* Wed Jul 11 2007 Peter Vrabec <pvrabec@redhat.com> 1.15.1-1
- new upstream bugfix release

* Tue Jul 10 2007 Peter Vrabec <pvrabec@redhat.com> 1.15.0-1
- new upstream release introduce capability to generate output 
  file names based on templates

* Tue Jul 03 2007 Peter Vrabec <pvrabec@redhat.com> 1.14.2-1
- new upstream bugfix release

* Mon Jul 02 2007 Peter Vrabec <pvrabec@redhat.com> 1.14.1-1
- new upstream release with IPv6 support

* Tue Jun 26 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.5-3
- add BuildRequires for  zlib compression feature

* Mon Jun 25 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.5-2
- some spec file adjustments.
- fix syslog init script error codes (#245330)

* Fri Jun 22 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.5-1
- new upstream release

* Fri Jun 22 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.4-2
- some spec file adjustments.

* Mon Jun 18 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.4-1
- upgrade to new upstream release

* Wed Jun 13 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.2-2
- DB support off

* Tue Jun 12 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.2-1
- new upstream release based on redhat patch

* Fri Jun 08 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.1-2
- rsyslog package provides its own kernel log. daemon (rklogd)

* Mon Jun 04 2007 Peter Vrabec <pvrabec@redhat.com> 1.13.1-1
- Initial rpm build
