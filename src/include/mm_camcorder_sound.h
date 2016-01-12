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
#define _MMCAMCORDER_SAMPLE_SOUND_NAME_CAPTURE01 "camera-shutter-01"
#define _MMCAMCORDER_SAMPLE_SOUND_NAME_CAPTURE02 "camera-shutter-02"
#define _MMCAMCORDER_SAMPLE_SOUND_NAME_REC_START "recording-start"
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
typedef struct _SOUND_INFO {
	/* mutex and cond */
	GMutex play_mutex;
	GCond play_cond;
	GMutex open_mutex;
	GCond open_cond;

	/* state */
	_MMCamcorderSoundState state;
} SOUND_INFO;

/*=======================================================================================
| CONSTANT DEFINITIONS									|
========================================================================================*/


/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
gboolean _mmcamcorder_sound_init(MMHandleType handle);
gboolean _mmcamcorder_sound_play(MMHandleType handle, const char *sample_name, gboolean sync_play);
gboolean _mmcamcorder_sound_finalize(MMHandleType handle);

int _mmcamcorder_sound_solo_play(MMHandleType handle, const char *sample_name, gboolean sync_play);
void _mmcamcorder_sound_solo_play_wait(MMHandleType handle);

#ifdef __cplusplus
}
#endif

#endif /* __MM_CAMCORDER_SOUND_H__ */
