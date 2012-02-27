ifeq ($(ARCH),target)
	PKG_CONFIG_PATH=/usr/lib/pkgconfig
	export PKG_CONFIG_PATH
	CC=arm-linux-gcc -Wall
	CXX=arm-linux-g++ -Wall
else
	PKG_CONFIG_PATH=/usr/lib/pkgconfig
	export PKG_CONFIG_PATH
	CC=gcc -Wall
    CXX=gcc -Wall
endif

TS1=uts_mm_camcorder_cancel
TS2=uts_mm_camcorder_capture_start
TS3=uts_mm_camcorder_capture_stop
TS4=uts_mm_camcorder_commit
TS5=uts_mm_camcorder_create
TS6=uts_mm_camcorder_destroy
TS7=uts_mm_camcorder_get_attrs
TS8=uts_mm_camcorder_get_state
TS9=uts_mm_camcorder_pause
TS10=uts_mm_camcorder_realize
TS11=uts_mm_camcorder_record
TS12=uts_mm_camcorder_set_attrs
TS13=uts_mm_camcorder_set_message_callback
TS14=uts_mm_camcorder_set_video_capture_callback
TS15=uts_mm_camcorder_set_video_stream_callback
TS16=uts_mm_camcorder_start
TS17=uts_mm_camcorder_start_focusing
TS18=uts_mm_camcorder_stop
TS19=uts_mm_camcorder_stop_focusing 
TS20=uts_mm_camcorder_unrealize
TS21=uts_mm_camcorder_get_attr_info

LIBS = `pkg-config --libs mm-camcorder`

LIBS +=-L/usr/lib/:/usr/lib/pkgconfig
 
#LIBS +=/scratchbox/tetware/tetware-target/lib/tet3/tcm_s.o
#LIBS +=-L/scratchbox/tetware/tetware-target/lib/lib/tet3 -ltcm_s
#LIBS +=-L/scratchbox/tetware/tetware-target/lib/lib/tet3/ -lapi_s
LIBS +=$(TET_ROOT)/lib/tet3/tcm_s.o
LIBS +=-L$(TET_ROOT)/lib/tet3 -ltcm_s
LIBS +=-L$(TET_ROOT)/lib/tet3/ -lapi_s
#LIBS +=/scratchbox/users/root/home/share/work_box/TETware3.7/tetware-target/src/tet3/apishlib/libapi_s.so
#/scratchbox/TETware3.7/tetware-simulator/lib/tet3

INCS = -I. `pkg-config --cflags mm-camcorder`


#INCS += -I/scratchbox/tetware/tetware-target/lib/inc/tet3
INCS += -I$(TET_ROOT)/inc/tet3
#INCS += -I/usr/include/mmf -I/scratchbox/tetware/tetware-target/src/tet3/inc -I/scratchbox/tetware/tetware-target/inc/tet3
INCS += -I/usr/include/mmf

CFLAGS = $(INCS) 
CC += $(CFLAGS)
LDFLAGS = $(LIBS)


all: $(TS1)  $(TS2) $(TS3) $(TS4) $(TS5) $(TS6) $(TS7) $(TS8) $(TS9) $(TS10) $(TS11) $(TS12) $(TS13) $(TS14) $(TS15) $(TS16) $(TS17) $(TS18) $(TS19) $(TS20) $(TS21) $(TS22) $(TS23) $(TS24)


$(TS1): uts_mm_camcorder_cancel.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS1) uts_mm_camcorder_utils.c uts_mm_camcorder_cancel.c $(LDFLAGS)

$(TS2): uts_mm_camcorder_capture_start.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS2) uts_mm_camcorder_utils.c uts_mm_camcorder_capture_start.c $(LDFLAGS)

$(TS3): uts_mm_camcorder_capture_stop.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS3) uts_mm_camcorder_utils.c uts_mm_camcorder_capture_stop.c $(LDFLAGS)
	
$(TS4): uts_mm_camcorder_commit.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS4) uts_mm_camcorder_utils.c uts_mm_camcorder_commit.c $(LDFLAGS)
	
$(TS5): uts_mm_camcorder_create.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS5) uts_mm_camcorder_utils.c uts_mm_camcorder_create.c $(LDFLAGS)	
	
$(TS6): uts_mm_camcorder_destroy.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS6) uts_mm_camcorder_utils.c uts_mm_camcorder_destroy.c $(LDFLAGS)

$(TS7): uts_mm_camcorder_get_attrs.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS7) uts_mm_camcorder_utils.c uts_mm_camcorder_get_attrs.c $(LDFLAGS)	

$(TS8): uts_mm_camcorder_get_state.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS8) uts_mm_camcorder_utils.c uts_mm_camcorder_get_state.c $(LDFLAGS)	

$(TS9): uts_mm_camcorder_pause.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS9) uts_mm_camcorder_utils.c uts_mm_camcorder_pause.c $(LDFLAGS)

$(TS10): uts_mm_camcorder_realize.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS10) uts_mm_camcorder_utils.c uts_mm_camcorder_realize.c $(LDFLAGS)

$(TS11): uts_mm_camcorder_record.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS11) uts_mm_camcorder_utils.c uts_mm_camcorder_record.c $(LDFLAGS)

$(TS12): uts_mm_camcorder_set_attrs.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS12) uts_mm_camcorder_utils.c uts_mm_camcorder_set_attrs.c $(LDFLAGS)

$(TS13): uts_mm_camcorder_set_message_callback.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS13) uts_mm_camcorder_utils.c uts_mm_camcorder_set_message_callback.c $(LDFLAGS)

$(TS14): uts_mm_camcorder_set_video_capture_callback.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS14) uts_mm_camcorder_utils.c uts_mm_camcorder_set_video_capture_callback.c $(LDFLAGS)

$(TS15): uts_mm_camcorder_set_video_stream_callback.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS15) uts_mm_camcorder_utils.c uts_mm_camcorder_set_video_stream_callback.c $(LDFLAGS)

$(TS16): uts_mm_camcorder_start.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS16) uts_mm_camcorder_utils.c uts_mm_camcorder_start.c $(LDFLAGS)

$(TS17): uts_mm_camcorder_start_focusing.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS17) uts_mm_camcorder_utils.c uts_mm_camcorder_start_focusing.c $(LDFLAGS)

$(TS18): uts_mm_camcorder_stop.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS18) uts_mm_camcorder_utils.c uts_mm_camcorder_stop.c $(LDFLAGS)

$(TS19): uts_mm_camcorder_stop_focusing.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS19) uts_mm_camcorder_utils.c uts_mm_camcorder_stop_focusing.c $(LDFLAGS)

$(TS20): uts_mm_camcorder_unrealize.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS20) uts_mm_camcorder_utils.c uts_mm_camcorder_unrealize.c $(LDFLAGS)

$(TS21): uts_mm_camcorder_get_attr_info.[ch] uts_mm_camcorder_utils.[ch]
	$(CC) -o $(TS21) uts_mm_camcorder_utils.c uts_mm_camcorder_get_attr_info.c $(LDFLAGS)

clean:
	rm -rf *~ *.o $(TS1) $(TS2) $(TS3) $(TS4) $(TS5) $(TS6) $(TS7) $(TS8) $(TS9) $(TS10) $(TS11) $(TS12) $(TS13) $(TS14) $(TS15) $(TS16) $(TS17) $(TS18) $(TS19) $(TS20) $(TS21)
	


