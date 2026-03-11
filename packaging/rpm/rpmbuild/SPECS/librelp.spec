Summary: The Reliable Event Logging Protocol library
Name: librelp
Version: 1.11.0
Release: 1%{?dist}
License: GPLv3+
Group: System Environment/Libraries
URL: https://www.rsyslog.com/
Source0: https://download.rsyslog.com/librelp/%{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: libtool, gnutls-devel, openssl-devel

%description
Librelp is an easy to use library for the RELP protocol. RELP (stands
for Reliable Event Logging Protocol) is a general-purpose, extensible
logging protocol.

%package devel
Summary: Development files for the %{name} package
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
Librelp is an easy to use library for the RELP protocol. The
librelp-devel package contains the header files and libraries needed
to develop applications using librelp.

%prep
%setup -q

%build

%configure \
%if 0%{?rhel} < 6
--disable-tls \
%else
--enable-tls \
--enable-tls-openssl \
%endif
--disable-static

make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

rm $RPM_BUILD_ROOT/%{_libdir}/*.la

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun
if [ "$1" = "0" ] ; then
    /sbin/ldconfig
fi

%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING NEWS README doc/*html
%{_libdir}/librelp.so.*

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/librelp.so
%{_libdir}/pkgconfig/relp.pc

%changelog
* Tue Jan 10 2023 Florian Riedl
- Release build for librelp 1.11.0

* Tue Feb 16 2021 Florian Riedl
- Updated to librelp 1.10.0

* Tue Nov 24 2020 Florian Riedl
- Updated to librelp 1.9.0

* Tue Sep 29 2020 Florian Riedl
- Updated to librelp 1.8.0

* Tue Aug 25 2020 Florian Riedl
- Updated to librelp 1.7.0

* Tue Apr 21 2020 Florian Riedl
- Updated to librelp 1.6.0

* Tue Jan 14 2020 Florian Riedl
- Updated to librelp 1.5.0

* Tue Mar 05 2019 Florian Riedl
- Updated to librelp 1.4.0

* Tue Dec 11 2018 Florian Riedl
- Updated to librelp 1.3.0

* Tue Sep 18 2018 Florian Riedl
- Updated to librelp 1.2.18

* Thu Aug 02 2018 Florian Riedl
- Updated to librelp 1.2.17

* Mon May 14 2018 Florian Riedl
- Updated to librelp 1.2.16

* Wed Mar 21 2018 Florian Riedl
- Updated to librelp 1.2.15

* Mon May 29 2017 Florian Riedl
- Updated to librelp 1.2.14

* Tue Feb 21 2017 Florian Riedl
- Updated to librelp 1.2.13

* Tue Jul 19 2016 Florian Riedl
- Updated to librelp 1.2.12

* Thu Jun 23 2016 Florian Riedl
- Updated to librelp 1.2.11

* Wed Mar 30 2016 Florian Riedl
- Updated to librelp 1.2.10

* Thu Dec 17 2015 Florian Riedl
- Updated to librelp 1.2.9

* Mon Sep 07 2015 Florian Riedl
- Updated to librelp 1.2.8

* Fri May 02 2014 Andre Lorbach
- Updated to librelp 1.2.7

* Mon Mar 24 2014 Andre Lorbach
- Removed disable-tls option

* Thu Mar 20 2014 Andre Lorbach
- Updated to librelp 1.2.5

* Mon Mar 17 2014 Andre Lorbach
- Updated to librelp 1.2.4

* Thu Mar 13 2014 Andre Lorbach
- Final version of librelp 1.2.3

* Wed Mar 12 2014 Andre Lorbach
- Updated librelp 

* Tue Mar 11 2014 Andre Lorbach
- Updated to librelp 1.2.3

* Tue Jan 07 2014 Andre Lorbach
- Updated to librelp 1.2.2

* Mon Jul 15 2013 Andre Lorbach
- Updated to librelp 1.2.0

* Fri Jul 05 2013 Andre Lorbach
- Updated to librelp 1.1.5

* Tue Jul 03 2013 Andre Lorbach
- Updated to librelp 1.1.4

* Tue Jun 26 2013 Andre Lorbach
- Updated to librelp 1.1.3

* Tue Jun 11 2013 Andre Lorbach
- Updated to librelp 1.1.1 with TLS Support

* Tue May 07 2013 Andre Lorbach
- Updated to librelp 1.0.6

* Fri Apr 10 2013 Andre Lorbach
- Updated to librelp 1.0.3

* Fri Mar 15 2013 Andre Lorbach
- Updated to librelp 1.0.2

* Thu Sep 06 2012 Andre Lorbach
- Updated to librelp 1.0.1

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.0.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Thu Jul 15 2010 Tomas Heinrich <theinric@redhat.com> - 1.0.0-1
- upgrade to upstream version 1.0.0

* Sat Jul 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.1.1-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.1.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Wed May  7 2008 Tomas Heinrich <theinric@redhat.com> 0.1.1-2
- removed "BuildRequires: autoconf automake"

* Tue Apr 29 2008 Tomas Heinrich <theinric@redhat.com> 0.1.1-1
- initial build
