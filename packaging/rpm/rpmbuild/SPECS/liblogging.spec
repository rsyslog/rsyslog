Name:		liblogging
Version:	1.0.6
Release:	1%{?dist}
Summary:	LibLogging stdlog library
License:	BSD-2-Clause
Group:		System Environment/Libraries
URL:		http://www.liblogging.org
Source0:	https://download.rsyslog.com/liblogging/liblogging-%{version}.tar.gz 
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRoot:	%{_tmppath}/liblogging-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:	libtool, chrpath

%description
LibLogging stdlog library
Libstdlog component is used for standard logging (syslog replacement)
purposes via multiple channels (driver support is planned)

%package devel
Summary:	Development files for LibLogging stdlog library
Group:		Development/Libraries
Requires:	%{name}%{?_isa} = %{version}-%{release}
#Requires:	libee-devel%{?_isa} libestr-devel%{?_isa}
Requires:	pkgconfig

%description devel
The liblogging-devel package includes header files, libraries necessary for
developing programs which use liblogging library.

%prep
%setup -q -n liblogging-%{version}

%build
%configure --disable-journal
V=1 make

%install
make install INSTALL="install -p" DESTDIR="$RPM_BUILD_ROOT"
rm -f %{buildroot}%{_libdir}/*.{a,la}
chrpath -d %{buildroot}%{_libdir}/liblogging-stdlog.so.0.1.0

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun
if [ "$1" = "0" ] ; then
    /sbin/ldconfig
fi

%files
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/liblogging-stdlog.so.*
%{_bindir}/stdlogctl
%{_mandir}/*/*

%files devel
%{_libdir}/liblogging-stdlog.so
%{_includedir}/liblogging/*.h
%{_libdir}/pkgconfig/liblogging-stdlog.pc


%changelog
* Mon Mar 06 2017 Florian Riedl
- New RPMs for 1.0.6

* Tue Dec 09 2014 Florian Riedl
- New RPMs for 1.0.5

* Wed Mar 02 2014 Andre Lorbach
- New RPMs for 1.0.4

* Tue Feb 04 2014 Andre Lorbach
- New RPMs for 1.0.1

* Tue Jan 21 2014 Andre Lorbach
- Initial Version
