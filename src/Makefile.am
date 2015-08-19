ACLOCAL_AMFLAGS='-I m4'

lib_LTLIBRARIES = libmmfcamcorder.la libmmfcamcorderclient.la

includelibmmfcamcorderdir = $(includedir)/mmf

includelibmmfcamcorder_HEADERS = include/mm_camcorder.h \
				 include/mm_camcorder_mused.h

includelibmmfcamcorderclientdir = $(includedir)/mmf

includelibmmfcamcorderclient_HEADERS = include/mm_camcorder_mused.h

noinst_HEADERS = include/mm_camcorder_audiorec.h \
		 include/mm_camcorder_attribute.h \
		 include/mm_camcorder_configure.h \
		 include/mm_camcorder_gstcommon.h \
		 include/mm_camcorder_internal.h \
		 include/mm_camcorder_platform.h \
		 include/mm_camcorder_stillshot.h \
		 include/mm_camcorder_videorec.h \
		 include/mm_camcorder_util.h \
		 include/mm_camcorder_exifinfo.h\
		 include/mm_camcorder_exifdef.h \
		 include/mm_camcorder_sound.h \
		 include/mm_camcorder_mused.h

libmmfcamcorder_la_SOURCES = mm_camcorder.c \
			     mm_camcorder_stillshot.c \
			     mm_camcorder_videorec.c \
			     mm_camcorder_audiorec.c \
			     mm_camcorder_configure.c \
			     mm_camcorder_exifinfo.c \
			     mm_camcorder_sound.c \
			     mm_camcorder_internal.c \
			     mm_camcorder_attribute.c \
			     mm_camcorder_gstcommon.c \
			     mm_camcorder_platform.c \
			     mm_camcorder_util.c

libmmfcamcorderclient_la_SOURCES = mm_camcorder.c \
			     mm_camcorder_stillshot.c \
			     mm_camcorder_videorec.c \
			     mm_camcorder_audiorec.c \
			     mm_camcorder_configure.c \
			     mm_camcorder_exifinfo.c \
			     mm_camcorder_sound.c \
			     mm_camcorder_internal.c \
			     mm_camcorder_attribute.c \
			     mm_camcorder_gstcommon.c \
			     mm_camcorder_platform.c \
			     mm_camcorder_util.c \
			     client/mm_camcorder_client.c

libmmfcamcorder_la_CFLAGS = -I$(srcdir)/include \
			     $(GST_CFLAGS) \
			     $(GST_PLUGIN_BASE_CFLAGS) \
			     $(GST_VIDEO_CFLAGS) \
			     $(GST_APP_CFLAGS) \
			     $(EXIF_CFLAGS) \
			     $(MM_COMMON_CFLAGS) \
			     $(MMSOUND_CFLAGS) \
			     $(SNDFILE_CFLAGS) \
			     $(VCONF_CFLAGS) \
			     $(MMUTIL_IMGP_CFLAGS) \
			     $(MMUTIL_JPEG_CFLAGS) \
			     $(TBM_CFLAGS) \
			     $(SYSTEMINFO_CFLAGS)

libmmfcamcorderclient_la_CFLAGS = -I$(srcdir)/include \
			     $(GST_CFLAGS) \
			     $(GST_PLUGIN_BASE_CFLAGS) \
			     $(GST_VIDEO_CFLAGS) \
			     $(GST_APP_CFLAGS) \
			     $(EXIF_CFLAGS) \
			     $(MM_COMMON_CFLAGS) \
			     $(MMSOUND_CFLAGS) \
			     $(SNDFILE_CFLAGS) \
			     $(VCONF_CFLAGS) \
			     $(MMUTIL_IMGP_CFLAGS) \
			     $(MMUTIL_JPEG_CFLAGS) \
			     $(TBM_CFLAGS) \
			     $(SYSTEMINFO_CFLAGS)

libmmfcamcorder_la_LIBADD = \
			    $(GLIB_LIBS) \
			    $(GIO_LIBS) \
			    $(GST_LIBS) \
			    $(GST_PLUGIN_BASE_LIBS) \
			    $(GST_VIDEO_LIBS) \
			    $(GST_APP_LIBS) \
			    $(MM_COMMON_LIBS) \
			    $(EXIF_LIBS) \
			    $(MMSOUND_LIBS) \
			    $(SNDFILE_LIBS) \
			    $(VCONF_LIBS) \
			    $(MMUTIL_IMGP_LIBS) \
			    $(MMUTIL_JPEG_LIBS) \
			    $(TBM_LIBS)

libmmfcamcorderclient_la_LIBADD = \
			    $(GLIB_LIBS) \
			    $(GIO_LIBS) \
			    $(GST_LIBS) \
			    $(GST_PLUGIN_BASE_LIBS) \
			    $(GST_VIDEO_LIBS) \
			    $(GST_APP_LIBS) \
			    $(MM_COMMON_LIBS) \
			    $(EXIF_LIBS) \
			    $(MMSOUND_LIBS) \
			    $(SNDFILE_LIBS) \
			    $(VCONF_LIBS) \
			    $(MMUTIL_IMGP_LIBS) \
			    $(MMUTIL_JPEG_LIBS) \
			    $(TBM_LIBS)

libmmfcamcorder_la_CFLAGS += -DMMF_LOG_OWNER=0x010 -D_FILE_OFFSET_BITS=64
libmmfcamcorder_la_CFLAGS += -fdata-sections -ffunction-sections -Wl,--gc-sections
libmmfcamcorder_la_LDFLAGS = -Wl,--gc-sections \
			     -Wl,-init,_mmcamcorder_constructor
libmmfcamcorder_la_LIBADD += $(SYSTEMINFO_LIBS)

libmmfcamcorderclient_la_CFLAGS += -DMMF_LOG_OWNER=0x010 -D_FILE_OFFSET_BITS=64
libmmfcamcorderclient_la_CFLAGS += -fdata-sections -ffunction-sections -Wl,--gc-sections
libmmfcamcorderclient_la_LDFLAGS = -Wl,--gc-sections \
			     -Wl,-init,_mmcamcorder_constructor
libmmfcamcorderclient_la_LIBADD += $(SYSTEMINFO_LIBS)

if WAYLAND_SUPPORT
libmmfcamcorder_la_CFLAGS += $(GST_WAYLAND_CFLAGS)
libmmfcamcorder_la_LIBADD += $(GST_WAYLAND_LIBS)
libmmfcamcorderclient_la_CFLAGS += $(GST_WAYLAND_CFLAGS)
libmmfcamcorderclient_la_LIBADD += $(GST_WAYLAND_LIBS)
endif

install-exec-hook:
					mkdir -p $(DESTDIR)$(prefix)/share/sounds/mm-camcorder && \
					cp $(srcdir)/../sounds/* $(DESTDIR)$(prefix)/share/sounds/mm-camcorder/

