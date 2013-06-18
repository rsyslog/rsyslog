Summary: libestr - some essentials for string handling (and a bit more)
Name:    libestr
Version: 0.1.2
Release: 1%{?dist}
License: GPL
Group:      Networking/Admin
Source:     %{name}-%{version}.tar.gz
BuildRoot:  /var/tmp/%{name}-build
Requires: /sbin/ldconfig

%description
libestr is a string handling library used in rsyslog.

%package devel
Summary: includes for compilation against libestr
Group: Networking/Admin
Requires: %name = %version-%release
Requires: /usr/bin/pkg-config

%description devel
This package provides the include files and pkg-config information for
compiling against libestr.

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
%{_libdir}/libestr.so
%{_libdir}/libestr.so.0
%{_libdir}/libestr.so.0.0.0

%files devel
%defattr(644,root,root,755)
%{_includedir}/libestr.h
%{_libdir}/pkgconfig/libestr.pc
%{_libdir}/libestr.a
%{_libdir}/libestr.la

%changelog
* Tue Jun 12 2012 Abby Edwards <abby.lina.edwards@gmail.com> 0.1.2-1
- initial version, used to build latest git master
