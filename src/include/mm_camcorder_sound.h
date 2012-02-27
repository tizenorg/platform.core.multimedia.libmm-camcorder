/*
 * libmm-camcorder
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

#ifndef __MM_CAMCORDER_SOUND_H__
#define __MM_CAMCORDER_SOUND_H__

/*=======================================================================================
| INCLUDE FILES										|
========================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*=======================================================================================
| GLOBAL DEFINITIONS AND DECLARATIONS FOR CAMCORDER					|
========================================================================================*/

/*=======================================================================================
| MACRO DEFINITIONS									|
========================================================================================*/
#define _MMCAMCORDER_FILEPATH_CAPTURE_SND       "/usr/share/sounds/mm-camcorder/capture_shutter_01.wav"
#define _MMCAMCORDER_FILEPATH_CAPTURE2_SND      "/usr/share/sounds/mm-camcorder/capture_shutter_02.wav"
#define _MMCAMCORDER_FILEPATH_REC_START_SND     "/usr/share/sounds/mm-camcorder/recording_start_01.wav"
#define _MMCAMCORDER_FILEPATH_REC_STOP_SND      "/usr/share/sounds/mm-camcorder/recording_stop_01.wav"
#define _MMCAMCORDER_FILEPATH_AF_SUCCEED_SND    "/usr/share/sounds/mm-camcorder/af_succeed.wav"
#define _MMCAMCORDER_FILEPATH_AF_FAIL_SND       "/usr/share/sounds/mm-camcorder/af_fail.wav"
#define _MMCAMCORDER_FILEPATH_NO_SND            "/usr/share/sounds/mm-camcorder/no_sound.wav"

/*=======================================================================================
| ENUM DEFINITIONS									|
========================================================================================*/
typedef enum {
	_MMCAMCORDER_SOUND_STATE_NONE,
	_MMCAMCORDER_SOUND_STATE_INIT,
	_MMCAMCORDER_SOUND_STATE_PREPARE,
	_MMCAMCORDER_SOUND_STATE_PLAYING,
} _MMCamcorderSoundState;

/*=======================================================================================
| STRUCTURE DEFINITIONS									|
========================================================================================*/
/**
 * Structure of sound info
 */
typedef struct __SOUND_INFO {
	SF_INFO sfinfo;
	SNDFILE *infile;
	short *pcm_buf;
	int pcm_size;
	char *filename;

	MMSoundPcmHandle_t handle;

	int thread_run;
	pthread_t thread;
	pthread_mutex_t play_mutex;
	pthread_cond_t play_cond;
	pthread_mutex_t open_mutex;
	pthread_cond_t open_cond;
	system_audio_route_t route_policy_backup;

	_MMCamcorderSoundState state;
} SOUND_INFO;

/*=======================================================================================
| CONSTANT DEFINITIONS									|
========================================================================================*/


/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
gboolean _mmcamcorder_sound_init(MMHandleType handle, char *filename);
gboolean _mmcamcorder_sound_prepare(MMHandleType handle);
gboolean _mmcamcorder_sound_play(MMHandleType handle);
gboolean _mmcamcorder_sound_finalize(MMHandleType handle);

void _mmcamcorder_sound_solo_play(MMHandleType handle, const char *filepath, gboolean sync);

#ifdef __cplusplus
}
#endif

#endif /* __MM_CAMCORDER_SOUND_H__ */
