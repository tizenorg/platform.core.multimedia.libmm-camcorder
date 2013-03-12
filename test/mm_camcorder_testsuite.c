/*
 * mm_camcorder_testsuite
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* ===========================================================================================
EDIT HISTORY FOR MODULE
	This section contains comments describing changes made to the module.
	Notice that changes are listed in reverse chronological order.
when		who						what, where, why
---------	--------------------	----------------------------------------------------------
10/08/07	soyeon.kang@samsung.com		Created
10/10/07	wh01.cho@samsung.com   		Created
12/30/08	jh1979.park@samsung.com		Modified
08/31/11	sc11.lee@samsung.com		Modified (Reorganized for easy look)
*/


/*=======================================================================================
|  INCLUDE FILES                                                                        |
=======================================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gst/gst.h>
#include <sys/time.h>
#include "../src/include/mm_camcorder.h"
#include "../src/include/mm_camcorder_internal.h"
#include "../src/include/mm_camcorder_util.h"
#include <gst/interfaces/colorbalance.h>
#include <mm_ta.h>

/*-----------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:                                       |
-----------------------------------------------------------------------*/
#define EXPORT_API __attribute__((__visibility__("default")))

#define PACKAGE "mm_camcorder_testsuite"

GMainLoop *g_loop;
GIOChannel *stdin_channel;
int resolution_set;
int g_current_state;
int src_w, src_h;
GstCaps *filtercaps;
bool isMultishot;
MMCamPreset cam_info;
int mmcamcorder_state;
int mmcamcorder_print_state;
int multishot_num;
static int audio_stream_cb_cnt;
static int video_stream_cb_cnt;
static GTimer *timer = NULL;

/*-----------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:                                       |
-----------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
|    IMPORTED VARIABLE DECLARATIONS:                                    |
-----------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
|    IMPORTED FUNCTION DECLARATIONS:                                    |
-----------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
|    LOCAL #defines:                                                    |
-----------------------------------------------------------------------*/
#define test_ffmux_mp4

// FULLHD(1080P)
#define SRC_W_1920							1920
#define SRC_H_1080							1080


// 2M
#define SRC_W_1600							1600
#define SRC_H_1200							1200

//VGA
#define SRC_W_640							640
#define SRC_H_480							480

//QVGA
#define SRC_W_320							320 					// video input width 
#define SRC_H_240							240						// video input height

//QCIF
#define SRC_W_176							176 					// video input width 
#define SRC_H_144							144						// video input height

//QQVGA
#define SRC_W_160							160						// video input width 
#define SRC_H_120							120						// video input heith

//special resolution
#define SRC_W_400							400					// video input width 
#define SRC_H_300							300						// video input height

#define SRC_W_192							192 					// video input width 
#define SRC_H_256							256						// video input height

#define SRC_W_144							144					// video input width 
#define SRC_H_176							176						// video input height

#define SRC_W_300 							300
#define SRC_W_400 							400


#define DISPLAY_X_0							0						//for direct FB 
#define DISPLAY_Y_0							0						//for direct FB 

#define DISPLAY_W_320						320					//for direct FB 
#define DISPLAY_H_240						240						//for direct FB 


#define SRC_VIDEO_FRAME_RATE_15         15    // video input frame rate
#define SRC_VIDEO_FRAME_RATE_30         30    // video input frame rate
#define IMAGE_ENC_QUALITY               85    // quality of jpeg 
#define IMAGE_CAPTURE_COUNT_STILL       1     // the number of still-shot
#define IMAGE_CAPTURE_COUNT_MULTI       3     // default the number of multi-shot
#define IMAGE_CAPTURE_COUNT_INTERVAL    100   // mili seconds

#define MAX_FILE_SIZE_FOR_MMS           (250 * 1024)

#define EXT_JPEG                        "jpg"
#define EXT_MP4                         "mp4"
#define EXT_3GP                         "3gp"
#define EXT_AMR                         "amr"
#define EXT_MKV                         "mkv"

#define STILL_CAPTURE_FILE_PATH_NAME    "/opt/media/StillshotCapture"
#define MULTI_CAPTURE_FILE_PATH_NAME    "/opt/media/MultishotCapture"
#define IMAGE_CAPTURE_THUMBNAIL_PATH    "/opt/media/thumbnail.jpg"
#define IMAGE_CAPTURE_SCREENNAIL_PATH   "/opt/media/screennail.yuv"
#define IMAGE_CAPTURE_EXIF_PATH         "/opt/media/exif.raw"
#define TARGET_FILENAME_PATH            "/opt/media/"
#define TARGET_FILENAME_VIDEO           "/opt/media/test_rec_video.3gp"
#define TARGET_FILENAME_AUDIO           "/opt/media/test_rec_audio.amr"
#define CAPTURE_FILENAME_LEN            256

#define AUDIO_SOURCE_SAMPLERATE_AAC     44100
#define AUDIO_SOURCE_SAMPLERATE_AMR     8000
#define AUDIO_SOURCE_FORMAT             MM_CAMCORDER_AUDIO_FORMAT_PCM_S16_LE
#define AUDIO_SOURCE_CHANNEL_AAC        2
#define AUDIO_SOURCE_CHANNEL_AMR        1
#define VIDEO_ENCODE_BITRATE            40000000 /* bps */

#define DEFAULT_CAM_DEVICE              MM_VIDEO_DEVICE_CAMERA1

/*
 * D E B U G   M E S S A G E
 */
#define MMF_DEBUG                       "** (mmcamcorder testsuite) DEBUG: "
#define MMF_ERR                         "** (mmcamcorder testsuite) ERROR: "
#define MMF_INFO                        "** (mmcamcorder testsuite) INFO: "
#define MMF_WARN                        "** (mmcamcorder testsuite) WARNING: "
#define MMF_TIME                        "** (mmcamcorder testsuite) TIME: "

#define CHECK_MM_ERROR(expr) \
do {\
	int ret = 0; \
	ret = expr; \
	if (ret != MM_ERROR_NONE) {\
		printf("[%s:%d] error code : %x \n", __func__, __LINE__, ret); \
		return; \
	}\
} while(0)

#define time_msg_t(fmt,arg...)	\
do { \
	fprintf(stderr,"\x1b[44m\x1b[37m"MMF_TIME"[%s:%05d]  " fmt ,__func__, __LINE__, ##arg); \
	fprintf(stderr,"\x1b[0m\n"); \
} while(0)

 #define debug_msg_t(fmt,arg...)\
 do { \
 	fprintf(stderr, MMF_DEBUG"[%s:%05d]  " fmt "\n",__func__, __LINE__, ##arg); \
 } while(0)
 
#define err_msg_t(fmt,arg...)	\
do { \
	fprintf(stderr, MMF_ERR"[%s:%05d]  " fmt "\n",__func__, __LINE__, ##arg); \
} while(0)

#define info_msg_t(fmt,arg...)	\
do { \
	fprintf(stderr, MMF_INFO"[%s:%05d]  " fmt "\n",__func__, __LINE__, ##arg); \
} while(0)

#define warn_msg_t(fmt,arg...)	\
do { \
	fprintf(stderr, MMF_WARN"[%s:%05d]  " fmt "\n",__func__, __LINE__, ##arg); \
} while(0)

#ifndef SAFE_FREE
#define SAFE_FREE(x)       if(x) {g_free(x); x = NULL;}
#endif


GTimeVal previous;
GTimeVal current;
GTimeVal result;
//temp

/**
 * Enumerations for command
 */
#define SENSOR_WHITEBALANCE_NUM		10
#define SENSOR_COLOR_TONE_NUM		37
#define SENSOR_FLIP_NUM			3
#define SENSOR_PROGRAM_MODE_NUM		15
#define SENSOR_FOCUS_NUM		6
#define SENSOR_INPUT_ROTATION		4
#define SENSOR_AF_SCAN_NUM		4
#define SENSOR_ISO_NUM			8
#define SENSOR_EXPOSURE_NUM		9
#define SENSOR_IMAGE_FORMAT		9


/*-----------------------------------------------------------------------
|    LOCAL CONSTANT DEFINITIONS:                                        |
-----------------------------------------------------------------------*/
enum 
{
	MODE_VIDEO_CAPTURE,	/* recording and image capture mode */
	MODE_AUDIO,		/* audio recording*/ 
	MODE_NUM,		
};

enum
{
	MENU_STATE_MAIN,
	MENU_STATE_SETTING,
	MENU_STATE_NUM,
};

/*-----------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:					|
-----------------------------------------------------------------------*/
typedef struct _cam_handle
{
	MMHandleType camcorder;
	int mode;                       /* image(capture)/video(recording) mode */
	bool isMultishot;               /* flag for multishot mode */
	int stillshot_count;            /* total stillshot count */
	int multishot_count;            /* total multishot count */
	char *stillshot_filename;       /* stored filename of  stillshot  */
	char *multishot_filename;       /* stored filename of  multishot  */
	int menu_state;
	int fps;
	bool isMute;
	unsigned long long elapsed_time;
} cam_handle_t;

typedef struct _cam_xypair
{
	char* attr_subcat_x;
	char* attr_subcat_y;
	int x;
	int y;
} cam_xypair_t;

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/
static cam_handle_t *hcamcorder ;

char *wb[SENSOR_WHITEBALANCE_NUM]={	
	"None",
	"Auto",
	"Daylight",
	"Cloudy",
	"Fluoroscent",	
	"Incandescent",
	"Shade",
	"Horizon",
	"Flash",
	"Custom",
};

char *ct[SENSOR_COLOR_TONE_NUM] = {
	"NONE",
	"MONO",
	"SEPIA",
	"NEGATIVE",
	"BLUE",
	"GREEN",
	"AQUA",
	"VIOLET",
	"ORANGE",
	"GRAY",
	"RED",
	"ANTIQUE",
	"WARM",
	"PINK",
	"YELLOW",
	"PURPLE",
	"EMBOSS",
	"OUTLINE",
	"SOLARIZATION",
	"SKETCH",
	"WASHED",
	"VINTAGE_WARM",
	"VINTAGE_COLD",
	"POSTERIZATION",
	"CARTOON",
	"SELECTVE_COLOR_RED",
	"SELECTVE_COLOR_GREEN",
	"SELECTVE_COLOR_BLUE",
	"SELECTVE_COLOR_YELLOW",
	"SELECTVE_COLOR_RED_YELLOW",
};

char *flip[SENSOR_FLIP_NUM] = {
	"Horizontal",
	"Vertical",
	"Not flipped",
};

char *program_mode[SENSOR_PROGRAM_MODE_NUM] = {
	"NORMAL",
	"PORTRAIT",
	"LANDSCAPE",
	"SPORTS",
	"PARTY_N_INDOOR",
	"BEACH_N_INDOOR",
	"SUNSET",
	"DUSK_N_DAWN",
	"FALL_COLOR",
	"NIGHT_SCENE",
	"FIREWORK",
	"TEXT",
	"SHOW_WINDOW",
	"CANDLE_LIGHT",
	"BACKLIGHT",
};

char *focus_mode[SENSOR_FOCUS_NUM] = {
	"None",
	"Pan",
	"Auto",
	"Manual",
	"Touch Auto",
	"Continuous Auto",
};

char *camera_rotation[SENSOR_INPUT_ROTATION] = {
	"None",
	"90",
	"180",
	"270",
};

char *af_scan[SENSOR_AF_SCAN_NUM] = {
	"None",
	"Normal",
	"Macro mode",
	"Full mode",
};

char *iso_name[SENSOR_ISO_NUM] = {
	"ISO Auto",
	"ISO 50",
	"ISO 100",
	"ISO 200",
	"ISO 400",
	"ISO 800",
	"ISO 1600",
	"ISO 3200",
};

char *exposure_mode[SENSOR_EXPOSURE_NUM] = {
	"AE off",
	"AE all mode",
	"AE center 1 mode",
	"AE center 2 mode",
	"AE center 3 mode",
	"AE spot 1 mode",	
	"AE spot 2 mode",		
	"AE custom 1 mode",
	"AE custom 2 mode",
};

char *image_fmt[SENSOR_IMAGE_FORMAT] = {
	"NV12",
	"NV12T",
	"NV16",
	"NV21",
	"YUYV",
	"UYVY",
	"422P",
	"I420",
	"YV12",
};

char *face_zoom_mode[] = {
	"Face Zoom OFF",
	"Face Zoom ON",
};

char *display_mode[] = {
	"Default",
	"Primary Video ON and Secondary Video Full Screen",
	"Primary Video OFF and Secondary Video Full Screen",
};

char *output_mode[] = {
	"Letter Box mode",
	"Original Size mode",
	"Full Screen mode",
	"Cropped Full Screen mode",
	"ROI mode",
};

char *rotate_mode[] = {
	"0",
	"90",
	"180",
	"270",
};

char* strobe_mode[] = {
	"Strobe OFF",
	"Strobe ON",
	"Strobe Auto",
	"Strobe RedEyeReduction",
	"Strobe SlowSync",
	"Strobe FrontCurtain",
	"Strobe RearCurtain",
	"Strobe Permanent",
};

char *detection_mode[2] = {
	"Face Detection OFF",
	"Face Detection ON",
};

char *wdr_mode[] = {
	"WDR OFF",
	"WDR ON",
	"WDR AUTO",
};

char *hdr_mode[] = {
	"HDR OFF",
	"HDR ON",
	"HDR ON and Original",
};

char *ahs_mode[] = {
	"Anti-handshake OFF",
	"Anti-handshake ON",
	"Anti-handshake AUTO",
	"Anti-handshake MOVIE",
};

char *vs_mode[] = {
	"Video-stabilization OFF",
	"Video-stabilization ON",
};

char *visible_mode[] = {
	"Display OFF",
	"Display ON", 
};


/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
static void print_menu();
void  get_me_out();
static gboolean cmd_input(GIOChannel *channel);
//static bool filename_callback (int format, char *filename, void *user_param);
static gboolean msg_callback(int message, void *msg_param, void *user_param);
static gboolean init(int type);
static gboolean mode_change();
int camcordertest_set_attr_int(char* attr_subcategory, int value);


static inline void flush_stdin()
{
	int ch;
	while((ch=getchar()) != EOF && ch != '\n');
}

void
cam_utils_convert_YUYV_to_UYVY(unsigned char* dst, unsigned char* src, gint length)
{
	int i = 0;
	
	//memset dst
	memset(dst, 0x00, length);
	memcpy(dst, src + 1, length-1);

	for ( i = 0 ; i < length; i ++)
	{
		if ( !(i % 2) )
		{
			dst[i+1] = src[i];
		}
	}
}

#ifdef USE_AUDIO_STREAM_CB
static int camcordertest_audio_stream_cb(MMCamcorderAudioStreamDataType *stream, void *user_param)
{
	audio_stream_cb_cnt++;
	printf("audio_stream cb is called (stream:%p, data:%p, format:%d, channel:%d, volume_dB:%f, length:%d, timestamp:%d)\n",
	       stream, stream->data, stream->format, stream->channel, stream->volume_dB, stream->length, stream->timestamp);

	return TRUE;
}
#endif /* USE_AUDIO_STREAM_CB */

static int camcordertest_video_stream_cb(MMCamcorderVideoStreamDataType *stream, void *user_param)
{
	video_stream_cb_cnt++;

	printf("VIDEO STREAM CALLBACK total length :%u, size %dx%d\n", stream->length_total, stream->width, stream->height);

	return TRUE;
}

static void _file_write(char *path, void *data, int size)
{
	FILE *fp = NULL;

	if (!path || !data || size <= 0) {
		printf("ERROR %p %p %d\n", path, data, size);
		return;
	}

	fp = fopen(path, "w");
	if (fp == NULL) {
		printf("open error! [%s], errno %d\n", path, errno);
		return;
	} else {
		printf("open success [%s]\n", path);
		if (fwrite(data, size, 1, fp) != 1) {
			printf("write error! errno %d\n", errno);
		} else {
			printf("write success [%s]\n", path);
		}

		fclose(fp);
		fp = NULL;
	}
}


static int
camcordertest_video_capture_cb(MMCamcorderCaptureDataType *main, MMCamcorderCaptureDataType *thumb, void *data)
{
	int nret = 0;
	int scrnl_size = 0;
	int exif_size = 0;
	char m_filename[CAPTURE_FILENAME_LEN];
	MMCamcorderCaptureDataType *scrnl = NULL;
	unsigned char *exif_data = NULL;

	if (main == NULL) {
		warn_msg_t("Capture callback : Main image buffer is NULL!!!");
		return FALSE;
	}

	if (hcamcorder->isMultishot) {
		snprintf(m_filename, CAPTURE_FILENAME_LEN, "%s%03d.jpg", hcamcorder->multishot_filename, hcamcorder->multishot_count++);
	} else {
		snprintf(m_filename, CAPTURE_FILENAME_LEN, "%s%03d.jpg", hcamcorder->stillshot_filename, hcamcorder->stillshot_count++);
	}

	debug_msg_t("hcamcorder->isMultishot=%d =>1: MULTI, 0: STILL, filename : %s",
	            hcamcorder->isMultishot, m_filename);

	if (main->format != MM_PIXEL_FORMAT_ENCODED) {
		unsigned int dst_size = 0;
		void *dst = NULL;

		nret = _mmcamcorder_encode_jpeg(main->data, main->width, main->height, main->format,
		                                main->length, 90, &dst, &dst_size);
		if (nret) {
			_file_write(m_filename, dst, dst_size);
		} else {
			printf("Failed to encode YUV(%d) -> JPEG. \n", main->format);
		}

		free(dst);
		dst = NULL;
	} else if (!hcamcorder->isMultishot) {

		printf("MM_PIXEL_FORMAT_ENCODED main->data=%p main->length=%d, main->width=%d, main->heigtht=%d \n",
		       main->data, main->length, main->width, main->height);

		/* main image */
		_file_write(m_filename, main->data, main->length);

		/* thumbnail */
		if (thumb != NULL) {
			_file_write(IMAGE_CAPTURE_THUMBNAIL_PATH, thumb->data, thumb->length);
		}

		/* screennail */
		mm_camcorder_get_attributes(hcamcorder->camcorder, NULL,
		                            "captured-screennail", &scrnl, &scrnl_size,
		                            NULL);
		if (scrnl != NULL) {
			_file_write(IMAGE_CAPTURE_SCREENNAIL_PATH, scrnl->data, scrnl->length);
		} else {
			printf( "Screennail buffer is NULL.\n" );
		}

		/* EXIF data */
		mm_camcorder_get_attributes(hcamcorder->camcorder, NULL,
		                            "captured-exif-raw-data", &exif_data, &exif_size,
		                            NULL);
		if (exif_data) {
			_file_write(IMAGE_CAPTURE_EXIF_PATH, exif_data, exif_size);
		}
	}

	return TRUE;
}

static gboolean test_idle_capture_start()
{
	int err;

	camcordertest_set_attr_int(MMCAM_CAPTURE_FORMAT, MM_PIXEL_FORMAT_ENCODED);
	camcordertest_set_attr_int(MMCAM_IMAGE_ENCODER, MM_IMAGE_CODEC_JPEG);

	g_timer_reset(timer);
	err = mm_camcorder_capture_start(hcamcorder->camcorder);

	if (err < 0) 
	{
//		if(hcamcorder->isMultishot == TRUE)
//			hcamcorder->isMultishot = FALSE;
		warn_msg_t("Multishot mm_camcorder_capture_start = %x", err);
	}	
	return FALSE;
}

int camcordertest_set_attr_int(char * attr_subcategory, int value)
{
	char * err_attr_name = NULL;
	int err;

	if (hcamcorder) {
		if (hcamcorder->camcorder) {
			debug_msg_t("camcordertest_set_attr_int(%s, %d)", attr_subcategory, value);

			err = mm_camcorder_set_attributes(hcamcorder->camcorder, &err_attr_name,
			                                  attr_subcategory, value,
			                                  NULL);
			if (err != MM_ERROR_NONE) {
				err_msg_t("camcordertest_set_attr_int : Error(%s:%x)!!!!!!!", err_attr_name, err);
				SAFE_FREE(err_attr_name);
				return FALSE;
			}

			//success
			return TRUE;
		}

		debug_msg_t("camcordertest_set_attr_int(!hcamcorder->camcorde)");
	}

	debug_msg_t("camcordertest_set_attr_int(!hcamcorder)");

	return FALSE;
}

int camcordertest_set_attr_xypair(cam_xypair_t pair)
{
	char * err_attr_name = NULL;
	int err;

	if (hcamcorder)
	{
		if (hcamcorder->camcorder)
		{
			debug_msg_t("camcordertest_set_attr_xypair((%s, %s), (%d, %d))",  pair.attr_subcat_x, pair.attr_subcat_y, pair.x, pair.y);

			err = mm_camcorder_set_attributes(hcamcorder->camcorder, &err_attr_name,
									pair.attr_subcat_x, pair.x,
									pair.attr_subcat_y, pair.y,
									NULL);
			if (err < 0) 
			{
				err_msg_t("camcordertest_set_attr_xypair : Error(%s:%x)!!", err_attr_name, err);
				SAFE_FREE (err_attr_name);
				return FALSE;
			}

			//success
			return TRUE;
		}

		debug_msg_t("camcordertest_set_attr_xypair(!hcamcorder->camcorde)");
	}

	debug_msg_t("camcordertest_set_attr_xypair(!hcamcorder)");
	return FALSE;
}

int camcordertest_get_attr_valid_intarray(char * attr_name, int ** array, int *count)
{
	MMCamAttrsInfo info;
	int err;

	if (hcamcorder)
	{
		if (hcamcorder->camcorder)
		{
			debug_msg_t("camcordertest_get_attr_valid_intarray(%s)", attr_name);

			err = mm_camcorder_get_attribute_info(hcamcorder->camcorder, attr_name, &info);
			if (err != MM_ERROR_NONE) {
				err_msg_t("camcordertest_get_attr_valid_intarray : Error(%x)!!", err);
				return FALSE;
			} else {
				if (info.type == MM_CAM_ATTRS_TYPE_INT) {
					if (info.validity_type == MM_CAM_ATTRS_VALID_TYPE_INT_ARRAY) {
						*array = info.int_array.array;
						*count = info.int_array.count;
						debug_msg_t("INT ARRAY - default value : %d", info.int_array.def);
						return TRUE;
					}
				}

				err_msg_t("camcordertest_get_attr_valid_intarray : Type mismatched!!");
				return FALSE;
			}
			//success
		}

		debug_msg_t("camcordertest_get_attr_valid_intarray(!hcamcorder->camcorde)");
	}

	debug_msg_t("camcordertest_get_attr_valid_intarray(!hcamcorder)");
	return FALSE;
}

int camcordertest_get_attr_valid_intrange(char * attr_name, int *min, int *max)
{
	MMCamAttrsInfo info;
	int err;

	if (hcamcorder)
	{
		if (hcamcorder->camcorder)
		{
			debug_msg_t("camcordertest_get_attr_valid_intrange(%s)", attr_name);

			err = mm_camcorder_get_attribute_info(hcamcorder->camcorder, attr_name, &info);
			if (err != MM_ERROR_NONE) {
				err_msg_t("camcordertest_get_attr_valid_intarray : Error(%x)!!",  err);
				return FALSE;
			} else {
				if (info.type == MM_CAM_ATTRS_TYPE_INT) {
					if (info.validity_type == MM_CAM_ATTRS_VALID_TYPE_INT_RANGE) {
						*min = info.int_range.min;
						*max = info.int_range.max;
						debug_msg_t("INT RANGE - default : %d", info.int_range.def);
						return TRUE;
					}
				}

				err_msg_t("camcordertest_get_attr_valid_intarray : Type mismatched!!");
				return FALSE;
			}
			//success

		}

		debug_msg_t("camcordertest_get_attr_valid_intarray(!hcamcorder->camcorde)");
	}

	debug_msg_t("camcordertest_get_attr_valid_intarray(!hcamcorder)");
	return FALSE;
}


void  get_me_out()
{
	mm_camcorder_capture_stop(hcamcorder->camcorder);
}

static void print_menu()
{
	switch(hcamcorder->menu_state)
	{
		case MENU_STATE_MAIN:
			if (hcamcorder->mode == MODE_VIDEO_CAPTURE) {
				g_print("\n\t=======================================\n");
				if ( cam_info.videodev_type == MM_VIDEO_DEVICE_CAMERA1 )
					g_print("\t   Video Capture (Front camera)\n");
				else if(  cam_info.videodev_type == MM_VIDEO_DEVICE_CAMERA0 )
					g_print("\t   Video Capture (Rear camera)\n");
				g_print("\t=======================================\n");

				if(mmcamcorder_print_state <= MM_CAMCORDER_STATE_PREPARE) {
					g_print("\t   '1' Take a photo\n");
					g_print("\t   '2' Start Recording\n");
					g_print("\t   '3' Setting\n");
					g_print("\t   '4' Print FPS\n");
					g_print("\t   'b' back\n");
				} else if(mmcamcorder_print_state == MM_CAMCORDER_STATE_RECORDING) {
					g_print("\t   'p' Pause Recording\n");
					g_print("\t   'c' Cancel\n");
					g_print("\t   's' Save\n");
					g_print("\t   'n' Capture video snapshot\n");
				} else if(mmcamcorder_print_state == MM_CAMCORDER_STATE_PAUSED) {
					g_print("\t   'r' Resume Recording\n");
					g_print("\t   'c' Cancel\n");
					g_print("\t   's' Save\n");
					g_print("\t   'n' Capture video snapshot\n");
				}
				g_print("\t=======================================\n");
			} else if (hcamcorder->mode == MODE_AUDIO) {
				g_print("\n\t=======================================\n");
				g_print("\t   Audio Recording\n");
				g_print("\t=======================================\n");
				if(mmcamcorder_print_state <= MM_CAMCORDER_STATE_PREPARE) {
					g_print("\t   '1' Start Recording\n");
					g_print("\t   'b' back\n");
				}
				else if(mmcamcorder_print_state == MM_CAMCORDER_STATE_RECORDING) {
				g_print("\t   'p' Pause Recording\n");
				g_print("\t   'c' Cancel\n");
				g_print("\t   's' Save\n");
				}
				else if(mmcamcorder_print_state == MM_CAMCORDER_STATE_PAUSED) {
					g_print("\t   'r' Resume Recording\n");
					g_print("\t   'c' Cancel\n");
					g_print("\t   's' Save\n");
				}
				g_print("\t=======================================\n");
			}
			break;

		case MENU_STATE_SETTING:
			g_print("\n\t=======================================\n");
			g_print("\t   Video Capture > Setting\n");
			g_print("\t=======================================\n");
			g_print("\t  >>>>>>>>>>>>>>>>>>>>>>>>>>>> [Camera]  \n");
			g_print("\t     '1' Capture resolution \n");
			g_print("\t     '2' Digital zoom level \n");
			g_print("\t     '3' Optical zoom level \n");
			g_print("\t     '4' AF mode \n");
			g_print("\t     '5' AF scan range \n");
			g_print("\t     '6' Exposure mode \n");
			g_print("\t     '7' Exposure value \n");
			g_print("\t     '8' F number \n");
			g_print("\t     '9' Shutter speed \n");
			g_print("\t     'i' ISO \n");
			g_print("\t     'r' Rotate camera input \n");
			g_print("\t     'f' Flip camera input \n");
			g_print("\t     'j' Jpeg quality \n");
			g_print("\t     'p' Picture format \n");
			g_print("\t     'E' EXIF orientation \n");
			g_print("\t  >>>>>>>>>>>>>>>>>>>> [Display/Filter]\n");
			g_print("\t     'v' Visible \n");
			g_print("\t     'n' Display mode \n");
			g_print("\t     'o' Output mode \n");
			g_print("\t     'y' Rotate display \n");
			g_print("\t     'Y' Flip display \n");
			g_print("\t     'g' Brightness \n");
			g_print("\t     'c' Contrast \n");
			g_print("\t     's' Saturation \n");
			g_print("\t     'h' Hue \n");
			g_print("\t     'a' Sharpness \n");
			g_print("\t     'w' White balance \n");
			g_print("\t     't' Color tone \n");
			g_print("\t     'd' WDR \n");
			g_print("\t     'e' EV program mode \n");
			g_print("\t  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>> [etc.]\n");
			g_print("\t     'z' Strobe (Flash) \n");
			g_print("\t     'x' Capture mode (Still/Multishot/HDR)\n");
			g_print("\t     'l' Face detection \n");
			g_print("\t     'k' Anti-handshake \n");
			g_print("\t     'K' Video-stabilization \n");
			g_print("\t     'u' Touch AF area \n");
			g_print("\t     'm' Stream callback function \n");
			g_print("\t     'b' back\n");
			g_print("\t=======================================\n");
			break;

		default:
			warn_msg_t("unknow menu state !!\n");
			break;
	}

	return;
}

static void main_menu(gchar buf)
{
	int err = 0;
	int current_fps = 0;
	int average_fps = 0;
	char *err_attr_name = NULL;

	if (hcamcorder->mode == MODE_VIDEO_CAPTURE) {
		if (mmcamcorder_state == MM_CAMCORDER_STATE_PREPARE) {
			switch (buf) {
				case '1' : //Capture
					if(hcamcorder->isMultishot) {
						int interval = 0;
						flush_stdin();
						printf("\ninput interval(ms) \n");
						err = scanf("%d", &interval);
						if (err == EOF) {
							printf("\nscanf error : errno %d\n", errno);
							interval = 300;
						}
						err = mm_camcorder_set_attributes(hcamcorder->camcorder, &err_attr_name,
						                                  MMCAM_CAPTURE_INTERVAL, interval,
						                                  NULL);
						if (err != MM_ERROR_NONE) {
							err_msg_t("Attribute setting fail : (%s:%x)", err_attr_name, err);
							SAFE_FREE (err_attr_name);
						}
					} else {
						err = mm_camcorder_set_attributes(hcamcorder->camcorder, &err_attr_name,
						                                  MMCAM_CAPTURE_COUNT, IMAGE_CAPTURE_COUNT_STILL,
						                                  NULL);
						if (err != MM_ERROR_NONE) {
							err_msg_t("Attribute setting fail : (%s:%x)", err_attr_name, err);
							SAFE_FREE (err_attr_name);
						}
					}

					g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)test_idle_capture_start, NULL, NULL);
					break;

				case '2' : // Start Recording
					g_print("*Recording start!\n");
					video_stream_cb_cnt = 0;
					audio_stream_cb_cnt = 0;

					g_timer_reset(timer);
					err = mm_camcorder_record(hcamcorder->camcorder);

					if (err != MM_ERROR_NONE) {
						warn_msg_t("Rec start mm_camcorder_record 0x%x", err);
					}

					mmcamcorder_print_state = MM_CAMCORDER_STATE_RECORDING;
					break;

				case '3' : // Setting
					hcamcorder->menu_state = MENU_STATE_SETTING;
					break;

				case '4' : // Print frame rate
					current_fps = _mmcamcorder_video_current_framerate(hcamcorder->camcorder);
					average_fps = _mmcamcorder_video_average_framerate(hcamcorder->camcorder);
					g_print("\tVideo Frame Rate[Current : %d.0 fps, Average : %d.0 fps]\n", current_fps, average_fps);
					break;

				case 'b' : // back
					hcamcorder->menu_state = MENU_STATE_MAIN;
					mode_change();
					break;

				default:
					g_print("\t Invalid input \n");
					break;
			}
		} else if (mmcamcorder_state == MM_CAMCORDER_STATE_RECORDING || mmcamcorder_state == MM_CAMCORDER_STATE_PAUSED) {
			switch (buf) {
				if (mmcamcorder_state == MM_CAMCORDER_STATE_RECORDING) {
					case 'p'  : // Pause Recording
						g_print("*Pause!\n");
						err = mm_camcorder_pause(hcamcorder->camcorder);

						if (err < 0) {
							warn_msg_t("Rec pause mm_camcorder_pause  = %x", err);
						}
						mmcamcorder_print_state = MM_CAMCORDER_STATE_PAUSED;
						break;

				} else {
					case 'r'  : // Resume Recording
						g_print("*Resume!\n");
						err = mm_camcorder_record(hcamcorder->camcorder);
						if (err < 0) {
							warn_msg_t("Rec start mm_camcorder_record  = %x", err);
						}
						mmcamcorder_print_state = MM_CAMCORDER_STATE_RECORDING;
						break;
				}

				case 'c' : // Cancel
					g_print("*Cancel Recording !\n");

					err = mm_camcorder_cancel(hcamcorder->camcorder);

					if (err < 0) {
						warn_msg_t("Cancel recording mm_camcorder_cancel  = %x", err);
					}
					mmcamcorder_print_state = MM_CAMCORDER_STATE_PREPARE;
					break;

				case 's' : // Save
					g_print("*Save Recording!\n");
					g_timer_reset(timer);

					err = mm_camcorder_commit(hcamcorder->camcorder);

					if (err < 0) {
						warn_msg_t("Save recording mm_camcorder_commit  = %x", err);
					}
					mmcamcorder_print_state = MM_CAMCORDER_STATE_PREPARE;
					break;

				case 'n' : /* Capture video snapshot */
					err = mm_camcorder_capture_start(hcamcorder->camcorder);
					break;

				default :
					g_print("\t Invalid input \n");
					break;
			} //switch
		} else {
			err_msg_t("Wrong camcorder state, check status!!");
		}
	} else if (hcamcorder->mode == MODE_AUDIO) {
		if (mmcamcorder_state == MM_CAMCORDER_STATE_PREPARE) {
			switch(buf) {
				case '1' : //  Start Recording
					g_print("*Recording start!\n");
					g_timer_reset(timer);
					err = mm_camcorder_record(hcamcorder->camcorder);

					if (err < 0) {
						warn_msg_t("Rec start mm_camcorder_record  = %x", err);
					}
					mmcamcorder_print_state = MM_CAMCORDER_STATE_RECORDING;
					break;

				case 'b' : // back
						hcamcorder->menu_state = MENU_STATE_MAIN;
						mode_change();
						break;

				default :
					g_print("\t Invalid input \n");
					break;
			}
		} else if (mmcamcorder_state == MM_CAMCORDER_STATE_RECORDING || mmcamcorder_state == MM_CAMCORDER_STATE_PAUSED) {
			switch(buf) {
				if (mmcamcorder_state == MM_CAMCORDER_STATE_RECORDING) {
					case 'p' : // Pause Recording
						g_print("*Pause!\n");
						err = mm_camcorder_pause(hcamcorder->camcorder);

						if (err < 0) 	{
							warn_msg_t("Rec pause mm_camcorder_pause  = %x", err);
						}
						mmcamcorder_print_state = MM_CAMCORDER_STATE_PAUSED;
						break;
				} else {
					case 'r'  : // Resume Recording
						g_print("*Resume!\n");
						err = mm_camcorder_record(hcamcorder->camcorder);
						if (err < 0) {
							warn_msg_t("Rec start mm_camcorder_record  = %x", err);
						}
						mmcamcorder_print_state = MM_CAMCORDER_STATE_RECORDING;
						break;
				}

				case 'c' : // Cancel
					g_print("*Cancel Recording !\n");
					err = mm_camcorder_cancel(hcamcorder->camcorder);

					if (err < 0) 	{
						warn_msg_t("Cancel recording mm_camcorder_cancel  = %x", err);
					}
					mmcamcorder_print_state = MM_CAMCORDER_STATE_PREPARE;
					break;

				case 's' : //  Save
					g_print("*Save Recording!\n");
					g_timer_reset(timer);
					err = mm_camcorder_commit(hcamcorder->camcorder);

					if (err < 0)	{
						warn_msg_t("Save recording mm_camcorder_commit  = %x", err);
					}
					mmcamcorder_print_state = MM_CAMCORDER_STATE_PREPARE;
					break;

				default :
					g_print("\t Invalid input \n");
					break;
			}
		} else {
			err_msg_t("Wrong camcorder state, check status!!");
		}
	}
	else {
		g_print("\t Invalid mode, back to upper menu \n");
		hcamcorder->menu_state = MENU_STATE_MAIN;
		mode_change();
	}
}


static void setting_menu(gchar buf)
{
	gboolean bret = FALSE;
	int index=0;
	int min = 0;
	int max = 0;
	int width_count = 0;
	int height_count = 0;
	int i=0;
	int count = 0;
	int value = 0;
	int* array = NULL;
	int *width_array = NULL;
	int *height_array = NULL;
	char *err_attr_name = NULL;
	cam_xypair_t input_pair;
	int err = MM_ERROR_NONE;
	int x = 0, y = 0, width = 0, height = 0;
	gint error_num=0;

	if (hcamcorder->mode == MODE_VIDEO_CAPTURE) {
		switch (buf) {
			/* Camera setting */
			case '1' : // Setting > Capture Resolution setting
				/* check recommend preview resolution */
				camcordertest_get_attr_valid_intarray(MMCAM_RECOMMEND_CAMERA_WIDTH, &width_array, &width_count);
				camcordertest_get_attr_valid_intarray(MMCAM_RECOMMEND_CAMERA_HEIGHT, &height_array, &height_count);
				if(width_count != height_count) {
					err_msg_t("System has wrong information!!\n");
				} else if (width_count == 0) {
					g_print("MMCAM_RECOMMEND_CAMERA_WIDTH/HEIGHT Not supported!!\n");
				} else {
					g_print("\n - MMCAM_RECOMMEND_CAMERA_WIDTH and HEIGHT (count %d) -\n", width_count);
					g_print("\t NORMAL ratio : %dx%d\n",
					        width_array[MM_CAMCORDER_PREVIEW_TYPE_NORMAL], height_array[MM_CAMCORDER_PREVIEW_TYPE_NORMAL]);
					if (width_count >= 2) {
						g_print("\t WIDE ratio   : %dx%d\n\n",
						        width_array[MM_CAMCORDER_PREVIEW_TYPE_WIDE], height_array[MM_CAMCORDER_PREVIEW_TYPE_WIDE]);
					} else {
						g_print("\t There is ONLY NORMAL resolution\n\n");
					}
				}

				g_print("*Select the resolution!\n");
				camcordertest_get_attr_valid_intarray("capture-width", &width_array, &width_count);
				camcordertest_get_attr_valid_intarray("capture-height", &height_array, &height_count);

				if(width_count != height_count) {
					err_msg_t("System has wrong information!!\n");
				} else if (width_count == 0) {
					g_print("Not supported!!\n");
				} else {
					g_print("\n Select  resolution \n");
					flush_stdin();

					for ( i = 0; i < width_count; i++) {
						g_print("\t %d. %d*%d\n", i+1, width_array[i], height_array[i]);
					}
					err = scanf("%d",&index);
					if (err == EOF) {
						printf("\nscanf error : errno %d\n", errno);
					} else {
						if( index > 0 && index <= width_count ) {
							//Set capture size first
							input_pair.attr_subcat_x = "capture-width";
							input_pair.attr_subcat_y = "capture-height";
							input_pair.x = width_array[index-1];
							input_pair.y = height_array[index-1];
							bret = camcordertest_set_attr_xypair(input_pair);
						}
					}
				}
				break;

			case '2' : // Setting > Digital zoom level
				g_print("*Digital zoom level  !\n");
				camcordertest_get_attr_valid_intrange("camera-digital-zoom", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Digital zoom level (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("camera-digital-zoom", index);
				}
				break;

			case '3' : // Setting > Optical zoom level
				g_print("*Optical zoom level  !\n");
				camcordertest_get_attr_valid_intrange("camera-optical-zoom", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Optical zoom level (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("camera-optical-zoom", index);
				}
				break;

			case '4' : // Setting > AF mode
				g_print("\t0. AF Mode setting !\n");
				g_print("\t1. AF Start !\n");
				g_print("\t2. AF Stop !\n\n");

				flush_stdin();
				err = scanf("%d", &index);

				switch (index) {
				case 0:
				{
					g_print("*Focus mode !\n");
					camcordertest_get_attr_valid_intarray("camera-focus-mode", &array, &count);

					if(count <= 0) {
						g_print("Not supported !! \n");
					} else {
						g_print("\n Select Focus mode \n");
						flush_stdin();
						for (i = 0 ; i < count ; i++) {
							g_print("\t %d. %s\n", array[i], focus_mode[array[i]]);
						}
						err = scanf("%d",&index);
						bret = camcordertest_set_attr_int("camera-focus-mode", index);
					}
				}
					break;
				case 1:
					mm_camcorder_start_focusing(hcamcorder->camcorder);
					break;
				case 2:
					mm_camcorder_stop_focusing(hcamcorder->camcorder);
					break;
				default:
					g_print("Wrong Input[%d] !! \n", index);
					break;
				}
				break;

			case '5' : // Setting > AF scan range
				g_print("*AF scan range !\n");
				camcordertest_get_attr_valid_intarray("camera-af-scan-range", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select AF scan range \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], af_scan[array[i]]);
					}
					err = scanf("%d",&index);
					camcordertest_set_attr_int("camera-af-scan-range", index);
				}
				break;

			case '6' : // Setting > Exposure mode
				g_print("* Exposure mode!\n");
				camcordertest_get_attr_valid_intarray("camera-exposure-mode", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select  Exposure mode \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], exposure_mode[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("camera-exposure-mode", index);
				}
				break;

			case '7' :  // Setting > Exposure value
				g_print("*Exposure value  !\n");
				camcordertest_get_attr_valid_intrange("camera-exposure-value", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Exposure value(%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("camera-exposure-value", index);
				}
				break;

			case '8' : // Setting > F number
				g_print("Not supported !! \n");
				break;

			case '9' : // Setting > Shutter speed
				g_print("*Shutter speed !\n");
				camcordertest_get_attr_valid_intarray("camera-shutter-speed", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select Shutter speed \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %d \n", i+1, array[i]);
					}
					err = scanf("%d",&index);

					if( index > 0 && index <= count ) {
						bret = camcordertest_set_attr_int("camera-shutter-speed", array[index-1]);
					}
				}
				break;

			case 'i' : // Setting > ISO
				g_print("*ISO !\n");
				camcordertest_get_attr_valid_intarray("camera-iso", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select ISO \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], iso_name[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("camera-iso", index);
				}
				break;

			case 'r' : // Setting > Rotate camera input when recording
				g_print("*Rotate camera input\n");
				camcordertest_get_attr_valid_intrange(MMCAM_CAMERA_ROTATION, &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					for (i = min ; i <= max ; i++) {
						g_print("\t %d. %s\n", i, camera_rotation[i]);
					}
					err = scanf("%d",&index);
					CHECK_MM_ERROR(mm_camcorder_stop(hcamcorder->camcorder));
					bret = camcordertest_set_attr_int(MMCAM_CAMERA_ROTATION, index);
					CHECK_MM_ERROR(mm_camcorder_start(hcamcorder->camcorder));
				}
				break;

			case 'f' : // Setting > Flip camera input
				flush_stdin();
				g_print("*Flip camera input\n");
				g_print(" 0. Flip NONE\n");
				g_print(" 1. Flip HORIZONTAL\n");
				g_print(" 2. Flip VERTICAL\n");
				g_print(" 3. Flip BOTH\n");

				err = scanf("%d", &index);

				CHECK_MM_ERROR(mm_camcorder_stop(hcamcorder->camcorder));
				CHECK_MM_ERROR(mm_camcorder_unrealize(hcamcorder->camcorder));

				camcordertest_set_attr_int(MMCAM_CAMERA_FLIP, index);

				CHECK_MM_ERROR(mm_camcorder_realize(hcamcorder->camcorder));
				CHECK_MM_ERROR(mm_camcorder_start(hcamcorder->camcorder));
				break;

			case 'j' : // Setting > Jpeg quality
				g_print("*Jpeg quality !\n");
				camcordertest_get_attr_valid_intrange("image-encoder-quality", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Jpeg quality (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("image-encoder-quality", index);
				}
				break;

			case 'p' : // Setting > Picture format
				g_print("* Picture format!\n");
				camcordertest_get_attr_valid_intarray("camera-format", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select Picture format \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], image_fmt[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("camera-format", index);
					CHECK_MM_ERROR(mm_camcorder_stop(hcamcorder->camcorder));
					CHECK_MM_ERROR(mm_camcorder_unrealize(hcamcorder->camcorder));
					CHECK_MM_ERROR(mm_camcorder_realize(hcamcorder->camcorder));
					CHECK_MM_ERROR(mm_camcorder_start(hcamcorder->camcorder));
				}
				break;

			case 'E' : // Setting > EXIF orientation
				g_print("* EXIF Orientation\n");

				g_print("\t 1. TOP_LEFT\n");
				g_print("\t 2. TOP_RIGHT(flipped)\n");
				g_print("\t 3. BOTTOM_RIGHT\n");
				g_print("\t 4. BOTTOM_LEFT(flipped)\n");
				g_print("\t 5. LEFT_TOP(flipped)\n");
				g_print("\t 6. RIGHT_TOP\n");
				g_print("\t 7. RIGHT_BOTTOM(flipped)\n");
				g_print("\t 8. LEFT_BOTTOM\n");

				flush_stdin();
				err = scanf("%d", &index);

				if (index < 1 || index > 8) {
					g_print("Wrong INPUT[%d]!! \n", index);
				} else {
					camcordertest_set_attr_int(MMCAM_TAG_ORIENTATION, index);
				}

				break;

			/* Display / Filter setting */
			case 'v' : // Display visible
				g_print("* Display visible setting !\n");
				camcordertest_get_attr_valid_intarray( "display-visible", &array, &count );

				if( count < 1 ) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select Display visible \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], visible_mode[array[i]]);
					}
					err = scanf("%d",&value);
					bret = camcordertest_set_attr_int( "display-visible", value );
				}
				break;

			case 'n' : //  Setting > Display mode
				g_print("* Display mode!\n");
				camcordertest_get_attr_valid_intarray(MMCAM_DISPLAY_MODE, &array, &count);

				if (count <= 0 || count > 255) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select Display mode\n");
					for (i = 0 ; i < count ; i++) {
						g_print("\t %d. %s\n", array[i], display_mode[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int(MMCAM_DISPLAY_MODE, index);
				}
				break;

			case 'o' : //  Setting > Output mode
				g_print("* Output mode!\n");
				camcordertest_get_attr_valid_intrange("display-geometry-method", &min, &max);

				if( min > max ) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select Output mode(%d ~ %d)\n", min, max);
					for( i = min ; i <= max ; i++ ) {
						g_print( "%d. %s\n", i, output_mode[i] );
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("display-geometry-method", index);
				}
				break;

			case 'y' : // Setting > Rotate Display
				camcordertest_get_attr_valid_intrange(MMCAM_DISPLAY_ROTATION, &min, &max);

				if( min > max ) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select Rotate mode(%d ~ %d)\n", min, max);
					g_print("\t0. 0\n\t1. 90\n\t2. 180\n\t3. 270\n\n");
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int(MMCAM_DISPLAY_ROTATION, index);
				}
				break;

			case 'Y' : // Setting > Flip Display
				flush_stdin();
				g_print("\n Select Rotate mode(%d ~ %d)\n", min, max);
				g_print("\t0. NONE\n\t1. HORIZONTAL\n\t2. VERTICAL\n\t3. BOTH\n\n");
				err = scanf("%d",&index);
				camcordertest_set_attr_int(MMCAM_DISPLAY_FLIP, index);
				break;

			case 'g' : // Setting > Brightness
				g_print("*Brightness !\n");
				camcordertest_get_attr_valid_intrange("filter-brightness", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  brightness (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-brightness", index);
				}
				break;

			case 'c' : // Setting > Contrast
				g_print("*Contrast !\n");
				camcordertest_get_attr_valid_intrange("filter-contrast", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Contrast (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-contrast", index);
				}
				break;

			case 's' : // Setting > Saturation
				g_print("*Saturation !\n");
				camcordertest_get_attr_valid_intrange("filter-saturation", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Saturation (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-saturation", index);
				}
				break;

			case 'h' : // Setting > Hue
				g_print("*Hue !\n");
				camcordertest_get_attr_valid_intrange("filter-hue", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Hue (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-hue", index);
				}
				break;

			case 'a' : // Setting > Sharpness
				g_print("*Sharpness !\n");
				camcordertest_get_attr_valid_intrange("filter-sharpness", &min, &max);

				if(min >= max) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select  Sharpness (%d ~ %d)\n", min, max);
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-sharpness", index);
				}
				break;

			case 'w' : // Setting > White balance
				g_print("*White balance !\n");
				camcordertest_get_attr_valid_intarray("filter-wb", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					flush_stdin();
					g_print("\n Select White balance \n");
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], wb[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-wb", index);
				}
				break;

			case 't' : // Setting > Color tone
				g_print("*Color tone !\n");
				camcordertest_get_attr_valid_intarray("filter-color-tone", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select Color tone \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], ct[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-color-tone", index);
				}
				break;

			case 'd' : // Setting > WDR
				g_print("*WDR !\n");
				camcordertest_get_attr_valid_intarray("camera-wdr", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				}
				else {
					g_print("\n Select WDR Mode \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], wdr_mode[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("camera-wdr", index);
				}
				break;

			case 'e' : // Setting > EV program mode
				g_print("* EV program mode!\n");
				camcordertest_get_attr_valid_intarray("filter-scene-mode", &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select EV program mode \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], program_mode[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("filter-scene-mode", index);
				}
				break;

			/* ext. setting */
			case 'z' : // Setting > Strobe setting
				g_print("*Strobe Mode\n");
				camcordertest_get_attr_valid_intarray("strobe-mode", &array, &count);
				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select Strobe Mode \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], strobe_mode[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int("strobe-mode", index);
				}
				break;

			case 'x' : // Setting > Capture mode ,Muitishot?
				g_print("*Select Capture mode!\n");
				flush_stdin();
				g_print(" \n\t1. Stillshot mode\n\t2. Multishot mode\n\t3. HDR capture\n");
				err = scanf("%d",&index);

				switch (index) {
				case 1:
					g_print("stillshot mode selected and capture callback is set!!!!\n");
					hcamcorder->isMultishot = FALSE;
					camcordertest_set_attr_int(MMCAM_CAMERA_HDR_CAPTURE, 0);
					break;
				case 2:
					g_print("Multilshot mode selected!!\n");

					camcordertest_set_attr_int(MMCAM_CAMERA_HDR_CAPTURE, 0);

					index = 0;
					min = 0;
					max = 0;

					camcordertest_get_attr_valid_intrange("capture-count", &min, &max);
					if(min >= max) {
						g_print("Not supported !! \n");
					} else {
						flush_stdin();
						//g_print("\n Check Point!!! [Change resolution to 800x480]\n");
						g_print("Select  mulitshot number (%d ~ %d)\n", min, max);
						err = scanf("%d",&index);
						if( index >= min && index <= max ) {
							multishot_num = index;
							mm_camcorder_set_attributes(hcamcorder->camcorder, &err_attr_name,
							                            MMCAM_CAPTURE_COUNT, multishot_num,
							                            NULL);
							hcamcorder->isMultishot = TRUE;
						} else {
							g_print("Wrong input value, Multishot setting failed!!\n");
						}
					}
					break;
				case 3:
					g_print("HDR Capture mode selected\n");
					hcamcorder->isMultishot = FALSE;

					camcordertest_get_attr_valid_intarray(MMCAM_CAMERA_HDR_CAPTURE, &array, &count);
					if(count <= 0) {
						g_print("Not supported !! \n");
					} else {
						g_print("\nSelect HDR capture mode\n");
						flush_stdin();
						for ( i = 0; i < count; i++) {
							g_print("\t %d. %s\n", array[i], hdr_mode[array[i]]);
						}
						err = scanf("%d",&index);
						bret = camcordertest_set_attr_int(MMCAM_CAMERA_HDR_CAPTURE, index);
					}
					break;
				default:
					g_print("Wrong input, select again!!\n");
					break;
				}
				break;

			case 'l' : // Setting > Face detection setting
				//hcamcorder->menu_state = MENU_STATE_SETTING_DETECTION;
				g_print("* Face detect mode !\n");

				camcordertest_get_attr_valid_intarray("detect-mode", &array, &count);
				if(count <= 0 || count > 256) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Face detect mode  \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s \n", array[i], detection_mode[array[i]]);
					}
					err = scanf("%d",&index);

					if( index >= 0 && index < count ) {
						bret = camcordertest_set_attr_int("detect-mode", array[index]);
					} else {
						g_print("Wrong input value. Try again!!\n");
						bret = FALSE;
					}
				}
				break;

			case 'k' : // Setting > Anti-handshake
				g_print("*Anti-handshake !\n");
				camcordertest_get_attr_valid_intarray(MMCAM_CAMERA_ANTI_HANDSHAKE, &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select Anti-handshake mode \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], ahs_mode[array[i]]);
					}
					err = scanf("%d",&index);
					bret = camcordertest_set_attr_int(MMCAM_CAMERA_ANTI_HANDSHAKE, index);
				}
				break;

			case 'K' : // Setting > Video-stabilization
				g_print("*Video-stabilization !\n");
				camcordertest_get_attr_valid_intarray(MMCAM_CAMERA_VIDEO_STABILIZATION, &array, &count);

				if(count <= 0) {
					g_print("Not supported !! \n");
				} else {
					g_print("\n Select Video-stabilization mode \n");
					flush_stdin();
					for ( i = 0; i < count; i++) {
						g_print("\t %d. %s\n", array[i], vs_mode[array[i]]);
					}
					err = scanf("%d",&index);

					if (index == MM_CAMCORDER_VIDEO_STABILIZATION_ON) {
						g_print("\n Restart preview with NV12 and 720p resolution\n");

						err = mm_camcorder_stop(hcamcorder->camcorder);
						if (err == MM_ERROR_NONE) {
							err = mm_camcorder_unrealize(hcamcorder->camcorder);
						}

						input_pair.attr_subcat_x = MMCAM_CAMERA_WIDTH;
						input_pair.attr_subcat_y = MMCAM_CAMERA_HEIGHT;
						input_pair.x = 1280;
						input_pair.y = 720;
						camcordertest_set_attr_xypair(input_pair);
						camcordertest_set_attr_int(MMCAM_CAMERA_FORMAT, MM_PIXEL_FORMAT_NV12);
						camcordertest_set_attr_int(MMCAM_CAMERA_VIDEO_STABILIZATION, index);

						if (err == MM_ERROR_NONE) {
							err = mm_camcorder_realize(hcamcorder->camcorder);
							if (err == MM_ERROR_NONE) {
								err = mm_camcorder_start(hcamcorder->camcorder);
							}
						}

						if (err != MM_ERROR_NONE) {
							g_print("\n Restart FAILED! %x\n", err);
						}
					} else {
						camcordertest_set_attr_int(MMCAM_CAMERA_VIDEO_STABILIZATION, index);
					}
				}
				break;

			case 'u': // Touch AF area
				g_print("* Touch AF area !\n");
				camcordertest_get_attr_valid_intrange("camera-af-touch-x", &min, &max);
				if( max < min ) {
					g_print( "Not Supported camera-af-touch-x !!\n" );
					break;
				}
				camcordertest_get_attr_valid_intrange("camera-af-touch-y", &min, &max);
				if( max < min ) {
					g_print( "Not Supported camera-af-touch-y !!\n" );
					break;
				}
				camcordertest_get_attr_valid_intrange("camera-af-touch-width", &min, &max);
				if( max < min ) {
					g_print( "Not Supported camera-af-touch-width !!\n" );
					break;
				}
				camcordertest_get_attr_valid_intrange("camera-af-touch-height", &min, &max);
				if( max < min ) {
					g_print( "Not Supported camera-af-touch-height!!\n" );
					break;
				}

				flush_stdin();
				g_print( "\n Input x,y,width,height \n" );
				err = scanf( "%d,%d,%d,%d", &x, &y, &width, &height );
				err = mm_camcorder_set_attributes(hcamcorder->camcorder, &err_attr_name,
												MMCAM_CAMERA_AF_TOUCH_X, x,
												MMCAM_CAMERA_AF_TOUCH_Y, y,
												MMCAM_CAMERA_AF_TOUCH_WIDTH, width,
												MMCAM_CAMERA_AF_TOUCH_HEIGHT, height,
												NULL);

				if( err != MM_ERROR_NONE ) {
					g_print( "Failed to set touch AF area.(%x)(%s)\n", err, err_attr_name );
					free( err_attr_name );
					err_attr_name = NULL;
				} else {
					g_print( "Succeed to set touch AF area.\n" );
				}
				break;

			case 'm' : // Setting > Stream callback function
				g_print("\n Select Stream Callback Function\n");
				g_print("\t 1. Set Video Stream Callback \n");
				g_print("\t 2. Unset Video Stream Callback \n");
				flush_stdin();
				err = scanf("%d", &index);
				if(index == 1) {
					video_stream_cb_cnt = 0;
					error_num = mm_camcorder_set_video_stream_callback(hcamcorder->camcorder, (mm_camcorder_video_stream_callback)camcordertest_video_stream_cb, (void*)hcamcorder->camcorder);
					if( error_num == MM_ERROR_NONE ) {
						g_print("\n Setting Success\n");
					} else {
						g_print("\n Setting Failure\n");
					}
				} else if(index == 2) {
					mm_camcorder_set_video_stream_callback(hcamcorder->camcorder, NULL, (void*)hcamcorder->camcorder);
					video_stream_cb_cnt = 0;
					audio_stream_cb_cnt = 0;
					g_print("\n Unset stream callback success\n");
				} else {
					g_print("\t Invalid input \n");
				}
				break;

			case 'b' : // back
				hcamcorder->menu_state = MENU_STATE_MAIN;
				break;

			default :
				g_print("\t Invalid input \n");
				break;
		}
	} else {
		g_print("\t Invalid mode, back to upper menu \n");
		hcamcorder->menu_state = MENU_STATE_MAIN;
	}
}


/**
 * This function is to execute command.
 *
 * @param	channel	[in]	1st parameter
 *
 * @return	This function returns TRUE/FALSE
 * @remark
 * @see
 */
static gboolean cmd_input(GIOChannel *channel)
{
	gchar buf[256];
	gsize read;

	debug_msg_t("ENTER");

	g_io_channel_read(channel, buf, CAPTURE_FILENAME_LEN, &read);
	buf[read] = '\0';
	g_strstrip(buf);

	debug_msg_t("Menu Status : %d", hcamcorder->menu_state);
	switch(hcamcorder->menu_state)
	{
		case MENU_STATE_MAIN:
			main_menu(buf[0]);
			break;
		case MENU_STATE_SETTING:
			setting_menu(buf[0]);
			break;
		default:
			break;
	}
	print_menu();
	return TRUE;
}

void validity_print(MMCamAttrsInfo *info)
{
			printf("info(%d,%d, %d))\n", info->type, info->flag, info->validity_type);
			if (info->validity_type == MM_CAM_ATTRS_VALID_TYPE_INT_ARRAY)
			{
				printf("int array(%p, %d)\n", info->int_array.array, info->int_array.count);
			}
			else if (info->validity_type == MM_CAM_ATTRS_VALID_TYPE_INT_RANGE)
			{
				printf("int range(%d, %d)\n", info->int_range.min, info->int_range.max);
			}
			else if (info->validity_type == MM_CAM_ATTRS_VALID_TYPE_DOUBLE_ARRAY)
			{
				printf("double array(%p, %d)\n", info->double_array.array, info->double_array.count);
			}
			else if(info->validity_type == MM_CAM_ATTRS_VALID_TYPE_DOUBLE_RANGE)
			{
				printf("double range(%f, %f)\n", info->double_range.min, info->double_range.max);
			}
			else
			{
				printf("validity none\n");
			}
			return;
}


/**
 * This function is to initiate camcorder attributes .
 *
 * @param	type	[in]	image(capture)/video(recording) mode
 *
 * @return	This function returns TRUE/FALSE
 * @remark
 * @see		other functions
 */
static gboolean init(int type)
{
	int err;
	int size;
	int preview_format = MM_PIXEL_FORMAT_NV12;
	MMHandleType cam_handle = 0;

	char *err_attr_name = NULL;

	if (!hcamcorder)
		return FALSE;

	if(!hcamcorder->camcorder)
		return FALSE;

	cam_handle = (MMHandleType)(hcamcorder->camcorder);

	/*================================================================================
		Video capture mode
	*=================================================================================*/
	if (type == MODE_VIDEO_CAPTURE) {
		mm_camcorder_get_attributes((MMHandleType)cam_handle, NULL,
		                            MMCAM_RECOMMEND_PREVIEW_FORMAT_FOR_CAPTURE, &preview_format,
		                            NULL);

		/* camcorder attribute setting */
		err = mm_camcorder_set_attributes( (MMHandleType)cam_handle, &err_attr_name,
		                                   MMCAM_MODE, MM_CAMCORDER_MODE_VIDEO_CAPTURE,
		                                   MMCAM_CAMERA_FORMAT, preview_format,
		                                   "camera-delay-attr-setting", TRUE,
		                                   MMCAM_IMAGE_ENCODER, MM_IMAGE_CODEC_JPEG,
		                                   MMCAM_IMAGE_ENCODER_QUALITY, IMAGE_ENC_QUALITY,
		                                   MMCAM_TAG_ENABLE, 1,
		                                   MMCAM_TAG_LATITUDE, 35.3036944,
		                                   MMCAM_TAG_LONGITUDE, 176.67837,
		                                   MMCAM_TAG_ALTITUDE, 190.3455,
		                                   MMCAM_DISPLAY_DEVICE, MM_DISPLAY_DEVICE_MAINLCD,
		                                   MMCAM_DISPLAY_SURFACE, MM_DISPLAY_SURFACE_X,
		                                   MMCAM_DISPLAY_RECT_X, DISPLAY_X_0,
		                                   MMCAM_DISPLAY_RECT_Y, DISPLAY_Y_0,
		                                   MMCAM_DISPLAY_RECT_WIDTH, 480,
		                                   MMCAM_DISPLAY_RECT_HEIGHT, 640,
		                                   MMCAM_DISPLAY_ROTATION, MM_DISPLAY_ROTATION_270,
		                                   //MMCAM_DISPLAY_FLIP, MM_FLIP_HORIZONTAL,
		                                   MMCAM_DISPLAY_GEOMETRY_METHOD, MM_DISPLAY_METHOD_LETTER_BOX,
		                                   MMCAM_CAPTURE_COUNT, IMAGE_CAPTURE_COUNT_STILL,
		                                   "capture-thumbnail", TRUE,
		                                   "tag-gps-time-stamp", 72815.5436243543,
		                                   "tag-gps-date-stamp", "2010:09:20", 10,
		                                   "tag-gps-processing-method", "GPS NETWORK HYBRID ARE ALL FINE.", 32,
		                                   MMCAM_AUDIO_DEVICE, MM_AUDIO_DEVICE_MIC,
		                                   MMCAM_AUDIO_ENCODER, MM_AUDIO_CODEC_AAC,
		                                   MMCAM_VIDEO_ENCODER, MM_VIDEO_CODEC_MPEG4,
		                                   MMCAM_VIDEO_ENCODER_BITRATE, VIDEO_ENCODE_BITRATE,
		                                   MMCAM_FILE_FORMAT, MM_FILE_FORMAT_3GP,
		                                   MMCAM_CAMERA_FPS, SRC_VIDEO_FRAME_RATE_30,
		                                   MMCAM_CAMERA_FPS_AUTO, 0,
		                                   MMCAM_CAMERA_ROTATION, MM_VIDEO_INPUT_ROTATION_NONE,
		                                   MMCAM_AUDIO_SAMPLERATE, AUDIO_SOURCE_SAMPLERATE_AAC,
		                                   MMCAM_AUDIO_FORMAT, AUDIO_SOURCE_FORMAT,
		                                   MMCAM_AUDIO_CHANNEL, AUDIO_SOURCE_CHANNEL_AAC,
		                                   //MMCAM_AUDIO_DISABLE, TRUE,
		                                   MMCAM_TARGET_FILENAME, TARGET_FILENAME_VIDEO, strlen(TARGET_FILENAME_VIDEO),
		                                   //MMCAM_TARGET_TIME_LIMIT, 360000,
#ifndef _TIZEN_PUBLIC_
		                                   //MMCAM_TARGET_MAX_SIZE, 102400,
#endif /* _TIZEN_PUBLIC_ */
		                                   NULL );

		if (err != MM_ERROR_NONE) {
			warn_msg_t("Init fail. (%s:%x)", err_attr_name, err);
			SAFE_FREE (err_attr_name);
			goto ERROR;
		}

		mm_camcorder_set_video_capture_callback(hcamcorder->camcorder, (mm_camcorder_video_capture_callback)camcordertest_video_capture_cb, hcamcorder);
	}
	/*================================================================================
		Audio mode
	*=================================================================================*/
	else if (type == MODE_AUDIO)
	{
		size = strlen(TARGET_FILENAME_AUDIO)+1;

		/* camcorder attribute setting */
		err = mm_camcorder_set_attributes( hcamcorder->camcorder, &err_attr_name,
		                                   MMCAM_MODE, MM_CAMCORDER_MODE_AUDIO,
		                                   MMCAM_AUDIO_DEVICE, MM_AUDIO_DEVICE_MIC,
		                                   MMCAM_AUDIO_ENCODER, MM_AUDIO_CODEC_AMR,
		                                   MMCAM_FILE_FORMAT, MM_FILE_FORMAT_AMR,
		                                   MMCAM_AUDIO_SAMPLERATE, AUDIO_SOURCE_SAMPLERATE_AMR,
		                                   MMCAM_AUDIO_FORMAT, AUDIO_SOURCE_FORMAT,
		                                   MMCAM_AUDIO_CHANNEL, AUDIO_SOURCE_CHANNEL_AAC,
		                                   MMCAM_TARGET_FILENAME, TARGET_FILENAME_AUDIO, size,
		                                   MMCAM_TARGET_TIME_LIMIT, 360000,
		                                   MMCAM_AUDIO_ENCODER_BITRATE, 12200,
#ifndef _TIZEN_PUBLIC_
		                                   MMCAM_TARGET_MAX_SIZE, 300,
#endif /* _TIZEN_PUBLIC_ */
		                                   NULL);
 	
		if (err < 0) {
			warn_msg_t("Init fail. (%s:%x)", err_attr_name, err);
			SAFE_FREE (err_attr_name);
			goto ERROR;
		}

#ifdef USE_AUDIO_STREAM_CB
		mm_camcorder_set_audio_stream_callback(hcamcorder->camcorder, (mm_camcorder_audio_stream_callback)camcordertest_audio_stream_cb, (void*)hcamcorder->camcorder);
#endif /* USE_AUDIO_STREAM_CB */
	}

	debug_msg_t("Init DONE.");

	return TRUE;
	
ERROR:
	err_msg_t("init failed.");
  	return FALSE;
}

/**
 * This function is to represent messagecallback
 *
 * @param	message	[in]	Specifies the message 
 * @param	param	[in]	Specifies the message type
 * @param	user_param	[in]	Specifies the user poiner for passing to callback function
 * @return	This function returns TRUE/FALSE
 * @remark
 * @see		other functions
 */
static gboolean msg_callback(int message, void *msg_param, void *user_param)
{

	MMMessageParamType *param = (MMMessageParamType *) msg_param;
	
	switch (message) {
		case MM_MESSAGE_CAMCORDER_ERROR:
			g_print("MM_MESSAGE_ERROR : code = %x", param->code);
			break;
		case MM_MESSAGE_CAMCORDER_STATE_CHANGED:
			g_current_state = param->state.current;
			switch(g_current_state)
			{
				case MM_CAMCORDER_STATE_NULL :
					mmcamcorder_state = MM_CAMCORDER_STATE_NULL;
					debug_msg_t("Camcorder State is [NULL]");
					break;
				case MM_CAMCORDER_STATE_READY :
					mmcamcorder_state = MM_CAMCORDER_STATE_READY;					
					debug_msg_t("Camcorder State is [READY]");
					break;
				case MM_CAMCORDER_STATE_PREPARE :
					mmcamcorder_state = MM_CAMCORDER_STATE_PREPARE;
					debug_msg_t("Camcorder State is [PREPARE]");
					break;
				case MM_CAMCORDER_STATE_CAPTURING :
					mmcamcorder_state = MM_CAMCORDER_STATE_CAPTURING;
					debug_msg_t("Camcorder State is [CAPTURING]");
					break;
				case MM_CAMCORDER_STATE_RECORDING :
					mmcamcorder_state = MM_CAMCORDER_STATE_RECORDING;
					time_msg_t("Recording start time  : %12.6lf s", g_timer_elapsed(timer, NULL));
					debug_msg_t("Camcorder State is [RECORDING]");
					break;
				case MM_CAMCORDER_STATE_PAUSED :
					mmcamcorder_state = MM_CAMCORDER_STATE_PAUSED;
					debug_msg_t("Camcorder State is [PAUSED]");
					break;
			}
			break;

		case MM_MESSAGE_CAMCORDER_CAPTURED:
			time_msg_t("Stillshot capture  : %12.6lf s", g_timer_elapsed(timer, NULL));
			if (hcamcorder->isMultishot) {
				//g_print("[CamcorderApp] Camcorder Captured(Capture Count=%d)\n", param->code);
				if (param->code >= multishot_num) {
					get_me_out();
//					g_timeout_add (100, (GSourceFunc)get_me_out, NULL);
//					hcamcorder->isMultishot = FALSE;
				}
			} else {
				get_me_out();
//				g_timeout_add (1000, (GSourceFunc)get_me_out, NULL);
			}
			break;
		case MM_MESSAGE_CAMCORDER_VIDEO_CAPTURED:
		case MM_MESSAGE_CAMCORDER_AUDIO_CAPTURED:
		{
			MMCamRecordingReport* report ;

			time_msg_t("Recording commit time : %12.6lf s", g_timer_elapsed(timer, NULL));

			if (param) {
				report = (MMCamRecordingReport*)(param->data);
			} else {
				return FALSE;
			}

			if (report != NULL) {
				g_print("*******************************************************\n");
				g_print("[Camcorder Testsuite] Camcorder Captured(filename=%s)\n", report->recording_filename);
				g_print("*******************************************************\n");

				SAFE_FREE (report->recording_filename);
				SAFE_FREE (report);
			} else {
				g_print( "[Camcorder Testsuite] report is NULL.\n" );
			}
		}
			break;
		case MM_MESSAGE_CAMCORDER_TIME_LIMIT:
		{
			debug_msg_t("Got TIME LIMIT MESSAGE");
		}
			break;
		case MM_MESSAGE_CAMCORDER_RECORDING_STATUS:
		{
			unsigned long long elapsed;
			elapsed = param->recording_status.elapsed / 1000;
			if (hcamcorder->elapsed_time != elapsed) {
				unsigned long temp_time;
				unsigned long long hour, minute, second;
				hcamcorder->elapsed_time = elapsed;
				temp_time = elapsed;
				hour = temp_time / 3600;
				temp_time = elapsed % 3600;
				minute = temp_time / 60;
				second = temp_time % 60;
				debug_msg_t("Current Time - %lld:%lld:%lld, remained %lld ms, filesize %lld KB",
				             hour, minute, second, param->recording_status.remained_time, param->recording_status.filesize);
			}
		}
			break;			
		case MM_MESSAGE_CAMCORDER_MAX_SIZE:	
		{	
			int err;
			g_print("*Save Recording because receives message : MM_MESSAGE_CAMCORDER_MAX_SIZE\n");
			g_timer_reset(timer);
			err = mm_camcorder_commit(hcamcorder->camcorder);

			if (err < 0) 
			{
				warn_msg_t("Save recording mm_camcorder_commit  = %x", err);
//					goto ERROR;
			}
		}
			break;
		case MM_MESSAGE_CAMCORDER_CURRENT_VOLUME:
			break;
		case MM_MESSAGE_CAMCORDER_NO_FREE_SPACE:
		{	
			int err;
			g_print("*Save Recording because receives message : MM_MESSAGE_CAMCORDER_NO_FREE_SPACE\n");
			g_timer_reset(timer);
			err = mm_camcorder_commit(hcamcorder->camcorder);

			if (err < 0) 
			{
				warn_msg_t("Save recording mm_camcorder_commit  = %x", err);
//					goto ERROR;
			}
		}
			break;
		case MM_MESSAGE_CAMCORDER_FOCUS_CHANGED:
		{
			g_print( "Focus State changed. State:[%d]\n", param->code );
		}
			break;
		case MM_MESSAGE_CAMCORDER_FACE_DETECT_INFO:
		{
			static int info_count = 0;
			MMCamFaceDetectInfo *cam_fd_info = NULL;

			cam_fd_info = (MMCamFaceDetectInfo *)(param->data);

			if (cam_fd_info) {
				int i = 0;

				g_print("\tface detect num %d, pointer %p\n", cam_fd_info->num_of_faces, cam_fd_info);

				for (i = 0 ; i < cam_fd_info->num_of_faces ; i++) {
					g_print("\t\t[%2d][score %d] position %d,%d %dx%d\n",
					        cam_fd_info->face_info[i].id,
					        cam_fd_info->face_info[i].score,
					        cam_fd_info->face_info[i].rect.x,
					        cam_fd_info->face_info[i].rect.y,
					        cam_fd_info->face_info[i].rect.width,
					        cam_fd_info->face_info[i].rect.height);
				}

				if (info_count == 0) {
					mm_camcorder_set_attributes(hcamcorder->camcorder, NULL,
					                            MMCAM_CAMERA_FACE_ZOOM_MODE, MM_CAMCORDER_FACE_ZOOM_MODE_ON,
					                            MMCAM_CAMERA_FACE_ZOOM_X, cam_fd_info->face_info[0].rect.x + (cam_fd_info->face_info[0].rect.width>>1),
					                            MMCAM_CAMERA_FACE_ZOOM_Y, cam_fd_info->face_info[0].rect.y + (cam_fd_info->face_info[0].rect.height>>1),
					                            MMCAM_CAMERA_FACE_ZOOM_LEVEL, 0,
					                            NULL);
					info_count = 1;
					g_print("\n\t##### START FACE ZOOM [%d,%d] #####\n", cam_fd_info->face_info[0].rect.x, cam_fd_info->face_info[0].rect.y);
				} else if (info_count++ == 90) {
					mm_camcorder_set_attributes(hcamcorder->camcorder, NULL,
					                            MMCAM_CAMERA_FACE_ZOOM_MODE, MM_CAMCORDER_FACE_ZOOM_MODE_OFF,
					                            NULL);
					g_print("\n\t##### STOP FACE ZOOM #####\n");
					info_count = -60;
				}
			}
		}
			break;
		default:
			g_print("Message %x received\n", message);
			break;
	}

	return TRUE;
}

static gboolean init_handle()
{
	hcamcorder->mode = MODE_VIDEO_CAPTURE;  /* image(capture)/video(recording) mode */
	hcamcorder->isMultishot =  FALSE;
	hcamcorder->stillshot_count = 0;        /* total stillshot count */
	hcamcorder->multishot_count = 0;        /* total multishot count */
	hcamcorder->stillshot_filename = STILL_CAPTURE_FILE_PATH_NAME;  /* stored filename of  stillshot  */ 
	hcamcorder->multishot_filename = MULTI_CAPTURE_FILE_PATH_NAME;  /* stored filename of  multishot  */ 
	hcamcorder->menu_state = MENU_STATE_MAIN;
	hcamcorder->isMute = FALSE;
	hcamcorder->elapsed_time = 0;
	hcamcorder->fps = SRC_VIDEO_FRAME_RATE_15; /*SRC_VIDEO_FRAME_RATE_30;*/
	multishot_num = IMAGE_CAPTURE_COUNT_MULTI;

	return TRUE;
}
/**
 * This function is to change camcorder mode.
 *
 * @param	type	[in]	image(capture)/video(recording) mode
 *
 * @return	This function returns TRUE/FALSE
 * @remark
 * @see		other functions
 */
static gboolean mode_change()
{
	int err = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int name_size = 0;
	int device_count = 0;
	int facing_direction = 0;
	char media_type = '\0';
	char *evassink_name = NULL;
	bool check= FALSE;

	debug_msg_t("MMCamcorder State : %d", mmcamcorder_state);

	err = mm_camcorder_get_state(hcamcorder->camcorder, (MMCamcorderStateType *)&state);
	if(state != MM_CAMCORDER_STATE_NONE)
	{
		if ((state == MM_CAMCORDER_STATE_RECORDING)
			|| (state == MM_CAMCORDER_STATE_PAUSED))
		{
			debug_msg_t("mm_camcorder_cancel");
			err = mm_camcorder_cancel(hcamcorder->camcorder);

			if (err < 0) 
			{
				warn_msg_t("exit mm_camcorder_cancel  = %x", err);
				return FALSE;
			}
		}
		else if (state == MM_CAMCORDER_STATE_CAPTURING)
		{
			debug_msg_t("mm_camcorder_capture_stop");
			err = mm_camcorder_capture_stop(hcamcorder->camcorder);

			if (err < 0) 
			{
				warn_msg_t("exit mmcamcorder_capture_stop  = %x", err);
				return FALSE;
			}
		}

		err = mm_camcorder_get_state(hcamcorder->camcorder, (MMCamcorderStateType *)&state);
		if(state == MM_CAMCORDER_STATE_PREPARE)
		{
			debug_msg_t("mm_camcorder_stop");
			mm_camcorder_stop(hcamcorder->camcorder);
		}
		
		err = mm_camcorder_get_state(hcamcorder->camcorder, (MMCamcorderStateType *)&state);
		if(state == MM_CAMCORDER_STATE_READY)
		{
			debug_msg_t("mm_camcorder_unrealize");
			mm_camcorder_unrealize(hcamcorder->camcorder);
		}
		
		err = mm_camcorder_get_state(hcamcorder->camcorder, (MMCamcorderStateType *)&state);
		if(state == MM_CAMCORDER_STATE_NULL)
		{	
			debug_msg_t("mm_camcorder_destroy");
			mm_camcorder_destroy(hcamcorder->camcorder);

			mmcamcorder_state = MM_CAMCORDER_STATE_NONE;
		}
	}
	
	init_handle();
	mmcamcorder_print_state = MM_CAMCORDER_STATE_PREPARE;
	while(!check) {
		g_print("\n\t=======================================\n");
		g_print("\t   MM_CAMCORDER_TESTSUIT\n");
		g_print("\t=======================================\n");
		g_print("\t   '1' Video Capture - Front Camera\n");
		g_print("\t   '2' Video Capture - Rear Camera\n");
		g_print("\t   '3' Audio Recording\n");
		g_print("\t   'q' Exit\n");
		g_print("\t=======================================\n");

		g_print("\t  Enter the media type:\n\t");	

		err = scanf("%c", &media_type);
		
		switch (media_type) {
		case '1':
			hcamcorder->mode= MODE_VIDEO_CAPTURE;
			cam_info.videodev_type = MM_VIDEO_DEVICE_CAMERA1;
			check = TRUE;
			break;
		case '2':
			hcamcorder->mode= MODE_VIDEO_CAPTURE;
			cam_info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;
			check = TRUE;
			break;
		case '3':
			hcamcorder->mode= MODE_AUDIO;
			cam_info.videodev_type = MM_VIDEO_DEVICE_NONE;
			check = TRUE;
			break;
		case 'q':
			g_print("\t Quit Camcorder Testsuite!!\n");
			hcamcorder->mode = -1;
			if(g_main_loop_is_running(g_loop)) {
				g_main_loop_quit(g_loop);
			}
			return FALSE;
		default:
			g_print("\t Invalid media type(%d)\n", media_type);
			continue;
		}
	}

	debug_msg_t("mm_camcorder_create");
	g_get_current_time(&previous);
	g_timer_reset(timer);

	err = mm_camcorder_create(&hcamcorder->camcorder, &cam_info);
	time_msg_t("mm_camcorder_create()  : %12.6lfs", g_timer_elapsed(timer, NULL));

	if (err != MM_ERROR_NONE) {
		err_msg_t("mmcamcorder_create = %x", err);
		return -1;
	} else {
		mmcamcorder_state = MM_CAMCORDER_STATE_NULL;
	}

	/* get evassink name */
	mm_camcorder_get_attributes(hcamcorder->camcorder, NULL,
	                            MMCAM_DISPLAY_EVAS_SURFACE_SINK, &evassink_name, &name_size,
	                            MMCAM_CAMERA_DEVICE_COUNT, &device_count,
	                            MMCAM_CAMERA_FACING_DIRECTION, &facing_direction,
	                            NULL);
	debug_msg_t("evassink name[%s], device count[%d], facing direction[%d]",
	            evassink_name, device_count, facing_direction);

	mm_camcorder_set_message_callback(hcamcorder->camcorder, (MMMessageCallback)msg_callback, hcamcorder);

	if (!init(hcamcorder->mode)) {
		err_msg_t("testsuite init() failed.");
		return -1;
	}

	debug_msg_t("mm_camcorder_realize");

	g_timer_reset(timer);

	err =  mm_camcorder_realize(hcamcorder->camcorder);
	time_msg_t("mm_camcorder_realize()  : %12.6lfs", g_timer_elapsed(timer, NULL));
	if (err != MM_ERROR_NONE) {
		err_msg_t("mm_camcorder_realize  = %x", err);
		return -1;
	}

	/* start camcorder */
	debug_msg_t("mm_camcorder_start");

	g_timer_reset(timer);

	err = mm_camcorder_start(hcamcorder->camcorder);
	time_msg_t("mm_camcorder_start()  : %12.6lfs", g_timer_elapsed(timer, NULL));

	if (err != MM_ERROR_NONE) {
		err_msg_t("mm_camcorder_start  = %x", err);
		return -1;
	}

 	g_get_current_time(&current);
	timersub(&current, &previous, &result);
	time_msg_t("Camera Starting Time  : %ld.%lds", result.tv_sec, result.tv_usec);

	return TRUE;
}


/**
 * This function is the example main function for mmcamcorder API.
 *
 * @param	
 *
 * @return	This function returns 0.
 * @remark
 * @see		other functions
 */
int main(int argc, char **argv)
{
	int bret;

	if (!g_thread_supported())
		g_thread_init (NULL);

	timer = g_timer_new();

	gst_init(&argc, &argv);

	time_msg_t("gst_init() : %12.6lfs", g_timer_elapsed(timer, NULL));

	hcamcorder = (cam_handle_t *) g_malloc0(sizeof(cam_handle_t));
	mmcamcorder_state = MM_CAMCORDER_STATE_NONE;

	g_timer_reset(timer);

	bret = mode_change();
	if(!bret){
		return bret;
	}

	print_menu();

	g_loop = g_main_loop_new(NULL, FALSE);

	stdin_channel = g_io_channel_unix_new(fileno(stdin));/* read from stdin */
	g_io_add_watch(stdin_channel, G_IO_IN, (GIOFunc)cmd_input, NULL);

	debug_msg_t("RUN main loop");

	g_main_loop_run(g_loop);

	debug_msg_t("STOP main loop");

	if (timer) {
		g_timer_stop(timer);
		g_timer_destroy(timer);
		timer = NULL;
	}
	//g_print("\t Exit from the application.\n");
	g_free(hcamcorder);
	g_main_loop_unref(g_loop);
	g_io_channel_unref(stdin_channel);

	return bret;
}

/*EOF*/
