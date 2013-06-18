Summary: liblognorm - a tool to normalize log data
Name:    liblognorm
Version: 0.3.4
Release: 1%{?dist}
License: GPL
Group:      Networking/Admin
Source:     %{name}-%{version}.tar.gz
BuildRoot:  /var/tmp/%{name}-build
BuildRequires: libestr-devel, libee-devel
Requires: /sbin/ldconfig

%description
Briefly described, liblognorm is a tool to normalize log data.

People who need to take a look at logs often have a common problem. Logs from
different machines (from different vendors) usually have different formats for
their logs. Even if it is the same type of log (e.g. from firewalls), the log
entries are so different, that it is pretty hard to read these. This is where
liblognorm comes into the game. With this tool you can normalize all your logs.
All you need is liblognorm and its dependencies and a sample database that fits
the logs you want to normalize.

%package devel
Summary: includes for compiling against liblognorm
Group: Networking/Admin
Requires: %name = %version-%release
Requires: /usr/bin/pkg-config

%description devel
This package provides the include and pkg-config files for compiling
against liblognorm.

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
%{_libdir}/liblognorm.so
%{_libdir}/liblognorm.so.0
%{_libdir}/liblognorm.so.0.0.0

%files devel
%defattr(644,root,root,755)
%{_libdir}/pkgconfig/lognorm.pc
%{_includedir}/annot.h
%{_includedir}/liblognorm.h
%{_includedir}/lognorm.h
%{_includedir}/ptree.h
%{_includedir}/samp.h
%{_bindir}/normalizer
%{_libdir}/liblognorm.a
%{_libdir}/liblognorm.la

%changelog
* Tue Jun 12 2012 Abby Edwards <abby.lina.edwards@gmail.com> 0.3.4-1
- initial version, used to build latest git master
