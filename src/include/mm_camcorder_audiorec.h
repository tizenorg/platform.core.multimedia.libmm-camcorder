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

#ifndef __MM_CAMCORDER_AUDIOREC_H__
#define __MM_CAMCORDER_AUDIOREC_H__

/*=======================================================================================
| INCLUDE FILES										|
========================================================================================*/
#include <mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=======================================================================================
| STRUCTURE DEFINITIONS									|
========================================================================================*/
/**
 * MMCamcorder information for audio mode
 */
typedef struct {
	int iSamplingRate;		/**< Sampling rate */
	int iBitDepth;			/**< Bit depth */
	int iChannels;			/**< audio channels */
	char *filename;			/**< recorded file name */
	gboolean b_commiting;		/**< Is it commiting now? */
	gboolean bMuxing;		/**< whether muxing */
	guint64 filesize;		/**< current recorded file size */
	guint64 max_size;		/**< max recording size */
	guint64 max_time;		/**< max recording time */
	int fileformat;			/**< recording file format */
} _MMCamcorderAudioInfo;

/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
/**
 * This function creates audio pipeline for audio recording.
 *
 * @param[in]	handle		Handle of camcorder.
 * @return	This function returns MM_ERROR_NONE on success, or others on failure.
 * @remarks
 * @see		_mmcamcorder_destroy_audio_pipeline()
 *
 */
int _mmcamcorder_create_audio_pipeline(MMHandleType handle);

/**
 * This function destroy audio pipeline.
 *
 * @param[in]	handle		Handle of camcorder.
 * @return	void
 * @remarks
 * @see		_mmcamcorder_destroy_pipeline()
 *
 */
void _mmcamcorder_destroy_audio_pipeline(MMHandleType handle);

/**
 * This function runs command for audio recording.
 *
 * @param[in]	handle		Handle of camcorder.
 * @param[in]	command		audio recording command.
 * @return	This function returns MM_ERROR_NONE on success, or others on failure.
 * @remarks
 * @see
 *
 */
int _mmcamcorder_audio_command(MMHandleType handle, int command);

/**
 * This function handles EOS(end of stream) when audio recording is finished.
 *
 * @param[in]	handle		Handle of camcorder.
 * @return	This function returns TRUE on success, or FALSE on failure.
 * @remarks
 * @see
 *
 */
int _mmcamcorder_audio_handle_eos(MMHandleType handle);

#ifdef __cplusplus
}
#endif
#endif /* __MM_CAMCORDER_AUDIOREC_H__ */
