Name:       libmm-camcorder
Summary:    camcorder library
Version:    0.5.5
Release:    1
Group:      libs
License:    Samsung
URL:        N/A
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(mm-common)
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
./autogen.sh
%configure --disable-static
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*.so.*
%{_datadir}/edje/*
/usr/share/sounds/mm-camcorder/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/mmf/mm_camcorder.h
%{_libdir}/pkgconfig/mm-camcorder.pc
%{_libdir}/*.so
