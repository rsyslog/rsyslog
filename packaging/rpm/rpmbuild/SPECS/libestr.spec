Summary: Some essentials for string handling (and a bit more)
Name:    libestr
Version: 0.1.11
Release: 1%{?dist}
License: GPLv2+
Group:      Networking/Admin
URL: https://libestr.adiscon.com/
Source0: https://libestr.adiscon.com/files/download/%{name}-%{version}.tar.gz
BuildRoot:  /var/tmp/%{name}-build
BuildRequires: libtool
Requires: /sbin/ldconfig

%description
Libestr - Some essentials for string handling (and a bit more)

%package devel
Summary: Development files for the %{name} package
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
Libestr - Some essentials for string handling (and a bit more)
The libestr-devel package contains the header files and libraries 
needed to develop applications using libestr.

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
* Tue Oct 30 2018 Florian Riedl
- updated package to 0.1.11

* Tue Dec 09 2014 Florian Riedl
- updated package to 0.1.10

* Mon Oct 28 2013 Andre Lorbach <alorbach@adiscon.com> 0.1.9-1
- updated to 0.1.9

* Thu Oct 17 2013 Andre Lorbach <alorbach@adiscon.com> 0.1.8-1
- updated to 0.1.8

* Fri Oct 11 2013 Andre Lorbach <alorbach@adiscon.com> 0.1.7-1
- updated to 0.1.7

* Fri Sep 13 2013 Andre Lorbach <alorbach@adiscon.com> 0.1.6-1
- updated to 0.1.6

* Wed Mar 20 2013 Andre Lorbach <alorbach@adiscon.com> 0.1.5-1
- updated to 0.1.5

* Wed Jul 18 2012 Andre Lorbach <alorbach@adiscon.com> 0.1.3-1
- updated to 0.1.3

* Tue Jun 12 2012 Abby Edwards <abby.lina.edwards@gmail.com> 0.1.2-1
- initial version, used to build latest git master
