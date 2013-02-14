ACLOCAL_AMFLAGS='-I m4'

lib_LTLIBRARIES = libmmfcamcorder.la

includelibmmfcamcorderdir = $(includedir)/mmf

includelibmmfcamcorder_HEADERS = include/mm_camcorder.h

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
		 include/mm_camcorder_sound.h

libmmfcamcorder_la_SOURCES = mm_camcorder.c \
			     mm_camcorder_internal.c \
			     mm_camcorder_attribute.c \
			     mm_camcorder_stillshot.c \
			     mm_camcorder_videorec.c \
			     mm_camcorder_audiorec.c \
			     mm_camcorder_gstcommon.c \
			     mm_camcorder_platform.c \
			     mm_camcorder_configure.c \
			     mm_camcorder_util.c \
			     mm_camcorder_exifinfo.c \
			     mm_camcorder_sound.c

libmmfcamcorder_la_CFLAGS = -I$(srcdir)/include \
			     $(GST_CFLAGS) \
			     $(GST_PLUGIN_BASE_CFLAGS) \
			     $(GST_INTERFACES_CFLAGS) \
			     $(MM_LOG_CFLAGS) \
			     $(MMTA_CFLAGS) \
			     $(EXIF_CFLAGS) \
			     $(MM_COMMON_CFLAGS) \
			     $(MMSESSION_CFLAGS) \
			     $(MMSOUND_CFLAGS) \
			     $(AVSYSTEM_CFLAGS) \
			     $(SNDFILE_CFLAGS) \
			     $(CAMSRCJPEGENC_CFLAGS) \
			     $(VCONF_CFLAGS) \
			     $(MMUTIL_IMGP_CFLAGS) \
			     $(AUDIOSESSIONMGR_CFLAGS)

libmmfcamcorder_la_LIBADD = \
			    $(GST_LIBS) \
			    $(GST_PLUGIN_BASE_LIBS) \
			    $(GST_INTERFACES_LIBS) \
			    $(MM_COMMON_LIBS) \
			    $(MM_LOG_LIBS) \
			    $(MMTA_LIBS) \
			    $(EXIF_LIBS) \
			    $(MMSESSION_LIBS) \
			    $(MMSOUND_LIBS) \
			    $(AVSYSTEM_LIBS) \
			    $(SNDFILE_LIBS) \
			    $(CAMSRCJPEGENC_LIBS) \
			    $(VCONF_LIBS) \
			    $(MMUTIL_IMGP_LIBS) \
			    $(AUDIOSESSIONMGR_LIBS)

libmmfcamcorder_la_CFLAGS += $(MMLOGSVR_CFLAGS) -DMMF_LOG_OWNER=0x010 -DMMF_DEBUG_PREFIX=\"MMF-CAMCORDER\" -D_INTERNAL_SESSION_MANAGER_
libmmfcamcorder_la_LIBADD += $(MMLOGSVR_LIBS)

install-exec-hook:
					mkdir -p $(DESTDIR)$(prefix)/share/sounds/mm-camcorder && \
					cp $(srcdir)/../sounds/*.wav $(DESTDIR)$(prefix)/share/sounds/mm-camcorder/

