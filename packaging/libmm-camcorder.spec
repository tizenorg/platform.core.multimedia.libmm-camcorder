%bcond_with wayland

Name:       libmm-camcorder
Summary:    Camera and recorder library
Version:    0.10.33
Release:    0
Group:      Multimedia/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(libexif)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(mmutil-jpeg)
BuildRequires:  pkgconfig(gstreamer-base-1.0)
BuildRequires:  pkgconfig(gstreamer-video-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
%if %{with wayland}
BuildRequires:  pkgconfig(gstreamer-wayland-1.0)
BuildRequires:  pkgconfig(wayland-client)
%endif
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(storage)
BuildRequires:  pkgconfig(murphy-resource)
BuildRequires:  pkgconfig(murphy-glib)
BuildRequires:  pkgconfig(ttrace)

%description
Camera and recorder function supported library.


%package devel
Summary:    Camera and recorder library for development
Group:      libdevel
Version:    %{version}
Requires:   %{name} = %{version}-%{release}

%description devel
Camera and recorder function supported library for development.


%prep
%setup -q


%build
%if %{with wayland}
export CFLAGS+=" -DHAVE_WAYLAND"
%endif
export CFLAGS+=" -D_LARGEFILE64_SOURCE"
./autogen.sh
%configure \
%if %{with wayland}
	--enable-wayland \
%endif
	--disable-static
make %{?jobs:-j%jobs}

%install
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}
%make_install


%post
/sbin/ldconfig

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
%{_includedir}/mmf/mm_camcorder_client.h
%{_libdir}/pkgconfig/mm-camcorder.pc
%{_libdir}/*.so
