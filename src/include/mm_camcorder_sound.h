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
#include <pulse/pulseaudio.h>
#include <pulse/ext-policy.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=======================================================================================
| GLOBAL DEFINITIONS AND DECLARATIONS FOR CAMCORDER					|
========================================================================================*/
#define _MMCAMCORDER_PREOPEN_PCM

/*=======================================================================================
| MACRO DEFINITIONS									|
========================================================================================*/
#define _MMCAMCORDER_FILEPATH_CAPTURE_SND        "/usr/share/sounds/mm-camcorder/capture_shutter_01.wav"
#define _MMCAMCORDER_FILEPATH_CAPTURE2_SND       "/usr/share/sounds/mm-camcorder/capture_shutter_02.wav"
#define _MMCAMCORDER_FILEPATH_REC_START_SND      "/usr/share/sounds/mm-camcorder/recording_start_01.wav"
#define _MMCAMCORDER_FILEPATH_REC_STOP_SND       "/usr/share/sounds/mm-camcorder/recording_stop_01.wav"
#define _MMCAMCORDER_SAMPLE_SOUND_NAME_CAPTURE   "camera-shutter"
#define _MMCAMCORDER_SAMPLE_SOUND_NAME_REC_STOP  "recording-stop"

/*=======================================================================================
| ENUM DEFINITIONS									|
========================================================================================*/
typedef enum {
	_MMCAMCORDER_SOUND_STATE_NONE,
	_MMCAMCORDER_SOUND_STATE_INIT,
	_MMCAMCORDER_SOUND_STATE_PLAYING,
} _MMCamcorderSoundState;

/*=======================================================================================
| STRUCTURE DEFINITIONS									|
========================================================================================*/
/**
 * Structure of sound info
 */
typedef struct __SOUND_INFO {
	/* PCM */
	MMSoundPcmHandle_t handle;
	mm_sound_device_out active_out_backup;
	int volume_type;
	unsigned int volume_level;

	/* mutex and cond */
	pthread_mutex_t play_mutex;
	pthread_cond_t play_cond;
	pthread_mutex_t open_mutex;
	pthread_cond_t open_cond;

	/* pulse audio */
	pa_threaded_mainloop *pulse_mainloop;
	pa_context *pulse_context;

#ifdef _MMCAMCORDER_UPLOAD_SAMPLE
	/* sound file */
	SF_INFO sfinfo;
	SNDFILE *infile;
	char *filename;
	pa_stream *sample_stream;
	pa_sample_spec sample_spec;
	size_t sample_length;
	pa_channel_map channel_map;
#else /* _MMCAMCORDER_UPLOAD_SAMPLE */
	pa_stream *sample_stream;
	pa_sample_spec sample_spec;
#endif /* _MMCAMCORDER_UPLOAD_SAMPLE */

	_MMCamcorderSoundState state;
} SOUND_INFO;

/*=======================================================================================
| CONSTANT DEFINITIONS									|
========================================================================================*/


/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
#ifdef _MMCAMCORDER_UPLOAD_SAMPLE
gboolean _mmcamcorder_sound_init(MMHandleType handle, char *filename);
#else /* _MMCAMCORDER_UPLOAD_SAMPLE */
gboolean _mmcamcorder_sound_init(MMHandleType handle);
#endif /* _MMCAMCORDER_UPLOAD_SAMPLE */
gboolean _mmcamcorder_sound_play(MMHandleType handle, const char *sample_name, gboolean sync_play);
gboolean _mmcamcorder_sound_finalize(MMHandleType handle);

void _mmcamcorder_sound_solo_play(MMHandleType handle, const char *filepath, gboolean sync_play);
void _mmcamcorder_sound_solo_play_wait(MMHandleType handle);

#ifdef __cplusplus
}
#endif

#endif /* __MM_CAMCORDER_SOUND_H__ */
