bin_PROGRAMS = mm_camcorder_testsuite

mm_camcorder_testsuite_SOURCES = mm_camcorder_testsuite.c

mm_camcorder_testsuite_CFLAGS = -I$(srcdir)/../src/include \
			     $(GLIB_CFLAGS)\
			     $(GST_CFLAGS)\
			     $(MM_COMMON_CFLAGS)\
			     $(MMTA_CFLAGS)\
			     $(MM_SOUND_CFLAGS)


############################################
#mm_camcorder_testsuite_CFLAGS += -DAPPSRC_TEST
############################################

mm_camcorder_testsuite_DEPENDENCIES = $(top_builddir)/src/libmmfcamcorder.la 
						
mm_camcorder_testsuite_LDADD = $(top_builddir)/src/libmmfcamcorder.la \
			     $(GLIB_LIBS)\
			     $(GST_LIBS)\
			     $(MM_COMMON_LIBS)\
			     $(MMTA_LIBS)\
			     $(MM_SOUND_LIBS)

