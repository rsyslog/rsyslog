Name:		czmq
Version:	1.2.0
Release:	1%{?dist}
Summary:	High-level C binding for 0MQ (ZeroMQ).

Group:		Development/Libraries
License:	LGPL
URL:		http://czmq.zeromq.org/
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires: zeromq-devel
BuildRequires: xmlto, xmltoman
BuildRequires: asciidoc
Requires: zeromq
Requires(post): /sbin/ldconfig

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

%description
%{summary}

%package devel
Summary: Development bindings for 0MQ (ZeroMQ).
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
Development bindings for 0MQ (ZeroMQ).

%prep
%setup -q

%build
export CFLAGS="-Wno-unused-variable"
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%post
/sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0644,root,root,0755)
%doc AUTHORS COPYING* INSTALL README NEWS

%{_mandir}/man7/zloop.7.gz
%{_mandir}/man7/zsockopt.7.gz
%{_mandir}/man7/zhash.7.gz
%{_mandir}/man7/zctx.7.gz
%{_mandir}/man7/zsocket.7.gz
%{_mandir}/man7/zframe.7.gz
%{_mandir}/man7/zfile.7.gz
%{_mandir}/man7/zthread.7.gz
%{_mandir}/man7/czmq.7.gz
%{_mandir}/man7/zstr.7.gz
%{_mandir}/man7/zclock.7.gz
%{_mandir}/man7/zmsg.7.gz
%{_mandir}/man7/zlist.7.gz
%{_libdir}/libczmq.so*
%attr(0755,root,root) %{_bindir}/czmq_selftest

%files devel
%defattr(0644,root,root,0755)
%{_libdir}/libczmq.la
%{_libdir}/pkgconfig/libczmq.pc
%{_libdir}/libczmq.a
%{_includedir}/zsocket.h
%{_includedir}/czmq.h
%{_includedir}/zstr.h
%{_includedir}/zlist.h
%{_includedir}/zsockopt.h
%{_includedir}/zmsg.h
%{_includedir}/zclock.h
%{_includedir}/zthread.h
%{_includedir}/zloop.h
%{_includedir}/zframe.h
%{_includedir}/czmq_prelude.h
%{_includedir}/zhash.h
%{_includedir}/zctx.h
%{_includedir}/zfile.h

%changelog

* Tue Jun 12 2012 Abby Edwards <abby.lina.edwards@gmail.com> 1.2.0-1
- updated to 1.2.0, split devel package out
- updated CFLAGS to be compatible with gcc 4+

* Thu Sep 29 2011 Lars Kellogg-Stedman <lars@seas.harvard.edu> - 1.1.0-1
- initial package build

