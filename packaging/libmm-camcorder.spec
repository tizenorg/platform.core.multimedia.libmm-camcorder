Name:       libmm-camcorder
Summary:    Camera and recorder library
Version:    0.6.13
Release:    1
Group:      libs
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/libmm-camcorder.manifest
Requires(post): /usr/bin/vconftool
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(libexif)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(mm-log)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(sndfile)
BuildRequires:  pkgconfig(mm-session)
BuildRequires:  pkgconfig(audio-session-mgr)
BuildRequires:  pkgconfig(camsrcjpegenc)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  gst-plugins-base-devel

%description
Camera and recorder library.


%package devel
Summary:    Camera and recorder development library
Group:      libdevel
Version:    %{version}
Requires:   %{name} = %{version}-%{release}

%description devel 
Camera and recorder development library.


%prep
%setup -q 


%build
export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS"
cp %{SOURCE1001} .
./autogen.sh
%configure --disable-static
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post
/sbin/ldconfig

vconftool set -t int memory/camera/state 0 -i -u 5000
vconftool set -t int file/camera/shutter_sound_policy 0 -u 5000

%postun -p /sbin/ldconfig

%files
%manifest libmm-camcorder.manifest
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*.so.*
/usr/share/sounds/mm-camcorder/*

%files devel
%manifest libmm-camcorder.manifest
%defattr(-,root,root,-)
%{_includedir}/mmf/mm_camcorder.h
%{_libdir}/pkgconfig/mm-camcorder.pc
%{_libdir}/*.so
