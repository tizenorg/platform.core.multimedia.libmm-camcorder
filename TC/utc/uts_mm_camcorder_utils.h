/*
 * libmm-camcorder TC
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

#ifndef UTS_MM_CAMCORDER_UTILS_H
#define UTS_MM_CAMCORDER_UTILS_H

#define _MMCAMCORDER_ENABLE_CAMPROC

#include <mm_camcorder.h>
#include <glib.h>
#include <tet_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** test case startup function */
void Startup();
/** test case clean up function */
void Cleanup();

enum {
	IMAGE_MODE = MM_CAMCORDER_MODE_IMAGE,
	VIDEO_MODE = MM_CAMCORDER_MODE_VIDEO,
	AUDIO_MODE = MM_CAMCORDER_MODE_AUDIO,
};

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
#define DISPLAY_W_640 						640
#define DISPLAY_H_480 						480
#define DISPLAY_ROTATION 					0

#define MM_CAMCORDER_MR59					59

#ifdef  __arm__
#define	AUDIO_SOURCE_CHANNEL 2
#else
#define	AUDIO_SOURCE_CHANNEL 1
#endif

#define AUDIO_SOURCE_SAMPLERATE 8000
#define	AUDIO_SOURCE_FORMAT MM_CAMCORDER_AUDIO_FORMAT_PCM_S16_LE
#define TARGET_FILENAME  "/opt/media/.tc_tmp"
#define TARGET_FILENAME_LENGTH  (sizeof(TARGET_FILENAME)+1)
#define AUDIO_SOURCE_VOLUME 1

#define DISPLAY_ID MM_DISPLAY_SURFACE_X

#define SRC_VIDEO_FRAME_RATE_15 15
#define SRC_VIDEO_FRAME_RATE_30 30
#define IMAGE_ENC_QUALITY 85
#define IMAGE_CAPTURE_COUNT_STILL 1						
#define IMAGE_CAPTURE_COUNT_MULTI 3 					
#define IMAGE_CAPTURE_COUNT_INTERVAL 1000

#define CAPTURE_FILENAME_LEN 256

#define CAMCORDER_RETURN_IF_FAIL(expr, val) \
	if(expr) { \
	} \
	else { \
	return (val); \
	};

typedef struct _camcorder_callback_parameters
{
	MMHandleType camcorder;
	int mode;				/* image(capture)/video(recording) mode */
//	int flip_mode;
	bool isMultishot;
	int stillshot_count; 			/* total stillshot count */ 
	int multishot_count;			/* total multishot count */ 
	char *stillshot_filename;		/* stored filename of  stillshot  */ 
	char *multishot_filename;		/* stored filename of  multishot  */ 
//	int	g_current_state;
//	int src_w, src_h;
//	GstCaps *filtercaps;

}camcorder_callback_parameters;

#define STRESS 3 //This is for stress testing.

extern camcorder_callback_parameters *gCamcorderCallbackParameters ;
extern camcorder_callback_parameters *gCamcorderCallbackParametersArray[STRESS] ;

gboolean CreateCamcorder(MMHandleType* camcorder, MMCamPreset* pInfo);
gboolean DestroyCamcorder(MMHandleType camcorder);

gboolean StartCamcorder(MMHandleType* camcorder, MMCamPreset* pInfo,gint mode);
gboolean StopCamcorder(MMHandleType camcorder);

gboolean StartCamcorderforNInstances(MMHandleType camcorderInstances[], gint noOfInstances);
gboolean StopCamcorderforNInstances(MMHandleType camcorderInstances[], gint noOfInstances);

gboolean SetDefaultAttributes(MMHandleType camcorder, gint mode);
gboolean GetDefaultAttributes(MMHandleType camcorder, gint mode);

int MessageCallbackFunctionCalledByCamcorder(int message, void *msg_param, void *user_param);
bool FilenameCallbackFunctionCalledByCamcorder(int format, char *filename, void *user_param);
bool VideoStreamCallbackFunctionCalledByCamcorder(void *stream, int width, int height, void *user_param);
bool VideoCaptureCallbackFunctionCalledByCamcorder(MMCamcorderCaptureDataType *frame, MMCamcorderCaptureDataType *thumbnail, void *user_param);

#endif /* UTS_MM_CAMCORDER_UTILS_H */
