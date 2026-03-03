Name:		liblognorm5
Version:	2.0.7
Release:	1%{?dist}
Provides:	liblognorm5 = %{version}-%{release}
Obsoletes:	liblognorm <= 2.0.3
Obsoletes:	liblognorm5 <= 2.0.3
Summary:	Fast samples-based log normalization library
License:	LGPLv2+
Group:		System Environment/Libraries
URL:		http://www.liblognorm.com
Source0:	https://www.liblognorm.com/files/download/liblognorm-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRoot:	%{_tmppath}/liblognorm-%{version}-%{release}-root-%(%{__id_u} -n)
#Patch0:		liblognorm-0.3.4-rename-to-lognormalizer.patch
#Patch1:		liblognorm-0.3.4-pc-file.patch
BuildRequires:	libtool, libestr-devel
#BuildRequires:	libee-devel
BuildRequires:	chrpath
%if 0%{?rhel} >= 8
BuildRequires: python3-docutils
%else
BuildRequires: python-docutils
%endif
BuildRequires:	libfastjson4-devel

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
Summary:	Development tools for programs using liblognorm library
Group:		Development/Libraries
Requires:	%{name}%{?_isa} = %{version}-%{release}
#Requires:	libee-devel%{?_isa} libestr-devel%{?_isa}
Requires:	pkgconfig

%description devel
The liblognorm-devel package includes header files, libraries necessary for
developing programs which use liblognorm library.

%package utils
Summary:	Lognormalizer utility for normalizing log files
Group:		Development/Libraries
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description utils
The lognormalizer is the core of liblognorm, it is a utility for normalizing
log files.

%prep
%setup -q -n liblognorm-%{version}
#%patch0 -p1 -b .rename-to-lognormalizer.patch
#%patch1 -p1 -b .pc-file.patch

%build
export PKG_CONFIG_PATH=%{_libdir}/pkgconfig:/%{_lib}/pkgconfig
%configure --disable-docs
V=1 make

%install
make install INSTALL="install -p" DESTDIR=%{buildroot}
rm -f %{buildroot}/%{_libdir}/*.{a,la}
chrpath -d %{buildroot}/%{_bindir}/lognormalizer
for f in %{buildroot}/%{_libdir}/liblognorm.so.*; do [ -f "$f" ] && chrpath -d "$f"; done

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun
if [ "$1" = "0" ] ; then
    /sbin/ldconfig
fi

%files
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/lib*.so.*

%files devel
%{_libdir}/lib*.so
%{_includedir}/*.h
#%{_libdir}/pkgconfig/*.pc
%{_libdir}/pkgconfig/lognorm.pc

%files utils
%{_bindir}/lognormalizer


%changelog
* Tue Oct 14 2025 Florian Riedl
- Build for release 2.0.7

* Wed Nov 07 2018 Florian Riedl
- Build for release 2.0.6

* Wed Oct 18 2017 Florian Riedl
- Repack for spec file changes

* Wed Oct 04 2017 Florian Riedl
- Build for release 2.0.4

* Wed Mar 22 2017 Florian Riedl
- Build for release 2.0.3

* Tue Nov 15 2016 Florian Riedl
- Build for release 2.0.2

* Wed Aug 03 2016 Florian Riedl
- Build for release 2.0.1

* Wed Jul 27 2016 Florian Riedl
- Build for release 2.0.0

* Mon Jul 20 2015 Florian Riedl
- Build for release 1.1.2

* Mon Mar 09 2015 Florian Riedl
- Build for release 1.1.1

* Mon Jan 12 2015 Florian Riedl
- Build for release 1.1.0

* Tue Apr 15 2014 Andre Lorbach
- Fixed problem with doc building

* Fri Apr 11 2014 Andre Lorbach
- Build for release 1.0.1

* Tue Nov 26 2013 Andre Lorbach
- Build for release 1.0.0

* Thu Mar 21 2013 Andre Lorbach
- Build for release 0.3.6
- Patch files no more needed with this release

* Wed Jan 16 2013 Andre Lorbach
- Adapted spec file to EHEL 6

* Fri Oct 05 2012 mdarade <mdarade@redhat.com> - 0.3.4-4
- Modified description of main & util package 

* Thu Sep 20 2012 Mahaveer Darade <mdarade@redhat.com> - 0.3.4-3
- Renamed normalizer binary to lognormalizer
- Updated pc file to exclude lee and lestr

* Mon Aug 27 2012 mdarade <mdarade@redhat.com> - 0.3.4-2
- Updated BuildRequires to contain libestr-devel

* Wed Aug  1 2012 Milan Bartos <mbartos@redhat.com> - 0.3.4-1
- initial port
