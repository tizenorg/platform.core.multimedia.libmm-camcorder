Name:       libmm-camcorder
Summary:    camcorder library
Version:    0.5.38
Release:    2
Group:      Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/libmm-camcorder.manifest 
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(sndfile)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(libexif)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(mm-log)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(mm-session)
BuildRequires:  pkgconfig(audio-session-mgr)
BuildRequires:  pkgconfig(gstreamer-floatcast-0.10)
BuildRequires:  pkgconfig(gstreamer-check-0.10)
BuildRequires:  pkgconfig(camsrcjpegenc)
BuildRequires:  gst-plugins-base-devel

%description
camcorder library.


%package devel
Summary:    camcorder development library
Group:      libdevel
Version:    %{version}
Requires:   %{name} = %{version}-%{release}

%description devel 
camcorder development library.


%prep
%setup -q 


%build
cp %{SOURCE1001} .
./autogen.sh
%configure --disable-static
make %{?jobs:-j%jobs}

%install
%make_install


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest libmm-camcorder.manifest
%{_bindir}/*
%{_libdir}/*.so.*
/usr/share/sounds/mm-camcorder/*

%files devel
%manifest libmm-camcorder.manifest
%{_includedir}/mmf/mm_camcorder.h
%{_libdir}/pkgconfig/mm-camcorder.pc
%{_libdir}/*.so
