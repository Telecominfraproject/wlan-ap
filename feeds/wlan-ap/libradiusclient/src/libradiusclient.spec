#/* SPDX-License-Identifier: BSD-3-Clause */
Summary: A library for RADIUS client.
Name: libradiusclient
Version: 0.1
Release: 1
Source: %{name}-%{version}.tar.gz
License: Proprietary
Group: System Environment/Libraries
BuildRoot: %{_builddir}/%{name}-%{version}-root
BuildRequires: libi802-devel

%description
A library for RADIUS client for use by AP components.

%package devel
Summary: Development files for libradiusclient.
Group: Development/Libraries
Requires: %{name} = %{version}

%description devel
This package contains the header files and documentation needed to
develop applications that use libradiusclient.

%prep
%setup -q

%build
	source ../../flex_env
make
#make doc

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT/usr
#make doc-install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/lib/libradiusclient.so.0

%files devel
%defattr(-,root,root)
/usr/include/radius/*
/usr/lib/libradiusclient.so
#/usr/share/man/man3/*
#/usr/share/doc/libradiusclient/*

