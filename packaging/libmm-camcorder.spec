Name:       libmm-camcorder
Summary:    Camera and recorder library
Version:    0.9.1
Release:    0
Group:      Multimedia/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Requires(post): /usr/bin/vconftool
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(libexif)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(mm-log)
BuildRequires:  pkgconfig(gstreamer-base-1.0)
BuildRequires:  pkgconfig(sndfile)
BuildRequires:  pkgconfig(camsrcjpegenc)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  gst-plugins-base-devel
BuildRequires:  gstreamer-devel

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
./autogen.sh
%configure --disable-static
make %{?jobs:-j%jobs}

%install
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}
%make_install


%post
/sbin/ldconfig

vconftool set -t int memory/camera/state 0 -i -u 5000 -s system::vconf_multimedia
vconftool set -t int file/camera/shutter_sound_policy 0 -u 5000 -s system::vconf_inhouse
chsmack -a "device::camera" /usr/share/sounds/mm-camcorder/camera_resource
chsmack -a "pulseaudio::record" /usr/share/sounds/mm-camcorder/recorder_resource

%postun -p /sbin/ldconfig

%files
%manifest libmm-camcorder.manifest
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*.so.*
/usr/share/sounds/mm-camcorder/*
%{_datadir}/license/%{name}

%files devel
%defattr(-,root,root,-)
%{_includedir}/mmf/mm_camcorder.h
%{_libdir}/pkgconfig/mm-camcorder.pc
%{_libdir}/*.so
