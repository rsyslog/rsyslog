Summary: librelp - a reliable logging library
Name:    librelp
Version: 1.0.1
Release: 1%{?dist}
License: GPL
Group:      Networking/Admin
Source:     %{name}-%{version}.tar.gz
BuildRoot:  /var/tmp/%{name}-build
Requires: /sbin/ldconfig

%description
librelp is an easy to use library for the RELP protocol. RELP in turn provides reliable
event logging over the network (and consequently RELP stands for Reliable Event Logging
Protocol). RELP was initiated by Rainer Gerhards after he was finally upset by the lossy
nature of plain tcp syslog and wanted a cure for all these dangling issues.

RELP (and hence) librelp assures that no message is lost, not even when connections
break and a peer becomes unavailable. The current version of RELP has a minimal window
of opportunity for message duplication after a session has been broken due to network
problems. In this case, a few messages may be duplicated (a problem that also exists
with plain tcp syslog). Future versions of RELP will address this shortcoming.

%package devel
Summary: includes for compilation against librelp
Group: Networking/Admin
Requires: %name = %version-%release
Requires: /usr/bin/pkg-config

%description devel
This package provides include and pkg-config files for compiling against
librelp.

%prep
%setup -q  -n %{name}-%{version}

%build
%configure CFLAGS="%{optflags}" --prefix=%{_prefix} --mandir=%{_mandir} --infodir=%{_infodir}
%{__make}

%install
rm -rf $RPM_BUILD_ROOT
%{__make} install DESTDIR=$RPM_BUILD_ROOT

%post
/sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%{_libdir}/librelp.so.0.0.0
%{_libdir}/librelp.so.0
%{_libdir}/librelp.so

%files devel
%defattr(644,root,root,755)
%{_includedir}/librelp.h
%{_libdir}/pkgconfig/relp.pc
%{_libdir}/librelp.a
%{_libdir}/librelp.la

%changelog
* Tue Jun 12 2012 Abby Edwards <abby.lina.edwards@gmail.com> 1.0.1-1
- initial version, used to build latest git master
